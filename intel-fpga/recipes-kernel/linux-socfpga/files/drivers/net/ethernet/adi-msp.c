// SPDX-License-Identifier: GPL-2.0-only
/* Driver for Analog Devices MS Plane Ethernet
 *
 * Copyright (C) 2022-2023 Analog Device Inc.
 */

#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/platform_device.h>
#include <linux/ethtool.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/adi_phc.h>

#define DRV_NAME	"adi-msp"
#define DRV_VERSION	"0.1"

#if IS_ENABLED(CONFIG_ADI_MSP_DEBUG)

#define MSP_DBG(...) trace_printk(__VA_ARGS__)
#define MSP_ERR(...) do {			\
	trace_printk(__VA_ARGS__);		\
	pr_err(__VA_ARGS__);			\
	tracing_off();				\
} while (0)

#else

#define MSP_DBG(...) do { } while (0)
#define MSP_ERR(...) pr_err(__VA_ARGS__)

#endif

#define MSP_INFO(...) pr_info(__VA_ARGS__)

/* Tx/Rx work unit type */
#define WU_TYPE_MASK		0x3
#define WU_TYPE_RX_DATA		1
#define WU_TYPE_RX_STAT		2
#define WU_TYPE_TX_DATA_SOF	1
#define WU_TYPE_TX_DATA_FUP	2
#define WU_TYPE_TX_STAT		3

#define RX_DATA_WU_HEADER_LEN	2
#define RX_DATA_WU_HEADER_SOF	(1 << 2)
#define RX_DATA_WU_HEADER_RESERVED_BITS 0xf8

#define RX_STAT_WU_HEADER_LEN	1
#define RX_STAT_WU_HEADER_ERR	(1 << 2)
#define RX_STAT_WU_HEADER_PORT	(1 << 3)
#define RX_STAT_WU_HEADER_DROPPED_ERR (1 << 4)
#define RX_STAT_WU_HEADER_RESERVED_BITS 0xe0

/* This is the header length for the first work unit for Tx.
 * NOTE
 * 1. Because the start of IP header is always aligned to 4 bytes and the
 *    Ethernet header is 14 bytes. We need to add a workunit header of 2 or 6
 *    bytes to make the work unit aligned to 4 bytes.
 * 2. For SKBs using paged data, we want to use one following work unit for
 *    each fragment. But it's impossible to add a header for such work unit
 *    without memory copy. Since we already provide the frame length in the
 *    header of the first work unit, we can tell the frame end by tracking
 *    how many bytes has been copied by DMA. So, we don't need a header for
 *    the work units following the first one.
 *    For now we are not going to support SKBs using paged data.
 */
struct tx_wu_header {
	u8 byte0;
	u8 frame_tag;
	u16 frame_len;
	u8 reserved[4];
};

#define TX_WU_HEADER_LEN	(sizeof(struct tx_wu_header))

#define TX_WU_PTP		(1 << 2)
#define TX_WU_PORT_0		(0 << 3)
#define TX_WU_PORT_1		(1 << 3)

/* Format of Tx/Rx status work unit */
struct status_wu_s {
	u8 byte0;
	u8 frame_tag;
	u8 timestamp[12];
	u16 frame_len;		/* Tx status wu: reserved */
};

/* 96-bit timestamp format
 *   [95:48]: seconds (48 bits)
 *   [47:16]: nanoseconds (32 bits)
 *   [15:0] : fractions of nanosecond (16 bits)
 * so in the following struct
 *   timestamp[0][7:0]  : byte0
 *   timestamp[0][16:8] : frame_tag
 *   timestamp[0][31:17]: fractions of nanosecond
 *   timestamp[1][31:0] : nanoseconds
 *   timestamp[2][31:0] : seconds[31:0]
 *   timestamp[3][15:0] : seconds[47:32]
 *   timestamp[3][32:16]: frame_len(Rx)/reserved(Tx)
 */
struct status_wu_t {
	u32 timestamp[4];
};

union status_wu {
	struct status_wu_s s;
	struct status_wu_t t;
};

#define STATUS_WU_LEN		(sizeof(union status_wu))

#define TX_STATUS_WU_ERR	(1 << 2)
#define TX_STATUS_WU_PTP	(1 << 3)

#define MSP_RST_CTRL		0x20103210
#define MSP_RST_CTRL_RX0	(1 << 0)
#define MSP_RST_CTRL_RX1	(1 << 1)
#define MSP_RST_CTRL_TX0	(1 << 2)
#define MSP_RST_CTRL_TX1	(1 << 3)

#define MSP_EN			(1 << 0)

#define MSP_RX_INT_FRAME_DROPPED	(1 << 0)
#define MSP_RX_INT_WORKUNIT_COMPLETE	(1 << 1)
#define MSP_RX_INT_STATUS_WR		(1 << 2)
#define MSP_RX_INT_CRC_ERR		(1 << 3)
#define MSP_RX_INT_FRAME_SIZE		(1 << 4)
#define MSP_RX_INT_ALL			0x1F

#define MSP_TX_INT_WU_HEADER_ERR	(1 << 0)
#define MSP_TX_INT_TX_WORKUNIT_COMPLETE	(1 << 1)
#define MSP_TX_INT_STATUS_WRITE_COMPLETE (1 << 2)
#define MSP_TX_INT_FRAME_SIZE		(1 << 3)
#define MSP_TX_INT_STATUS_FIFO_FULL	(1 << 4)
#define MSP_TX_INT_ALL			0x1F

struct msp_rx_regs {
	u32 stat_ctrl;
	u32 intr_en;
	u32 intr_stat;
	u32 frame_dropped_count_mplane;
	u32 frame_dropped_count_splane;
	u32 frame_size;
};

struct msp_tx_regs {
	u32 stat_ctrl;
	u32 intr_en;
	u32 intr_stat;
	u32 timeout_value;
	u32 frame_size;
};

struct dde_tester_regs {
	u32 ctrl;
};

/* Interrupt control register */
/* The base address is axi_palau_gpio module + 0x01D0 */
#define MSP_INT_CTRL_RX		0x0
#define MSP_INT_CTRL_TX		0x4
#define MSP_INT_CTRL_STATUS	0x8
#define MSP_INT_CTRL_DMADONE	(1 << 0)
#define MSP_INT_CTRL_DDE_ERR	(1 << 1)

#define DMA_STAT_PIRQ		(1 << 2)
#define DMA_STAT_IRQERR		(1 << 1)
#define DMA_STAT_IRQDONE	(1 << 0)
#define DMA_STAT_RUN(STAT)	(((STAT) >> 8) & 0x7)
#define DMA_STAT_HALT		0 /* IDLE or STOP */
#define DMA_STAT_DESC_FETCH	1 /* Descriptor Fetch */
#define DMA_STAT_DATA_TRANSFER	2 /* Data Transfer */
#define DMA_STAT_WAIT_FOR_TRIG	3 /* Wait for Trigger */
#define DMA_STAT_WAIT_FOR_WACK	4 /* Wait for Write ACK/FIFO Drain to Peri */

#define MSIZE01			0
#define MSIZE02			1
#define MSIZE04			2
#define MSIZE08			3
#define MSIZE16			4
#define MSIZE32			5

#define DMA_CFG_DESCIDCPY	(1 << 25)
#define DMA_CFG_INT_XCNT	(1 << 20)
#define DMA_CFG_INT_YCNT	(2 << 20)
#define DMA_CFG_INT_MASK	(3 << 20)
#define DMA_CFG_NDSIZE04	(3 << 16)
#define DMA_CFG_NDSIZE05	(4 << 16)
#define DMA_CFG_TWAIT		(1 << 15)
#define DMA_CFG_FLOW_STOP	(0 << 12)
#define DMA_CFG_FLOW_DSCL	(4 << 12)
#define DMA_CFG_FLOW_MASK	(7 << 12)
#define DMA_CFG_MSIZE01		(MSIZE01 << 8)
#define DMA_CFG_MSIZE02		(MSIZE02 << 8)
#define DMA_CFG_MSIZE04		(MSIZE04 << 8)
#define DMA_CFG_MSIZE08		(MSIZE08 << 8)
#define DMA_CFG_MSIZE16		(MSIZE16 << 8)
#define DMA_CFG_MSIZE32		(MSIZE32 << 8)
#define DMA_CFG_MSIZE_MASK	(7 << 8)
#define DMA_CFG_PSIZE01		(0 << 4)
#define DMA_CFG_PSIZE02		(1 << 4)
#define DMA_CFG_PSIZE04		(2 << 4)
#define DMA_CFG_PSIZE08		(3 << 4)
#define DMA_CFG_PSIZE_MASK	(7 << 4)
#define DMA_CFG_SYNC		(1 << 2)
#define DMA_CFG_READ		(0 << 1)
#define DMA_CFG_WRITE		(1 << 1)
#define DMA_CFG_EN		(1 << 0)

/* MSIZE and XMOD are dynamically calculated for TX DMA */
#define TX_DMA_CFG_COMMON \
	(DMA_CFG_DESCIDCPY | DMA_CFG_INT_XCNT | \
	 DMA_CFG_NDSIZE05 | DMA_CFG_PSIZE08 | \
	 DMA_CFG_SYNC | DMA_CFG_READ | DMA_CFG_EN)

#define RX_MSIZE		MSIZE08
#define RX_XMOD			(1 << RX_MSIZE)
#define RX_DMA_CFG_COMMON \
	(DMA_CFG_DESCIDCPY | DMA_CFG_INT_XCNT | \
	 DMA_CFG_NDSIZE05 | (RX_MSIZE << 8) | DMA_CFG_PSIZE08 | \
	 DMA_CFG_WRITE | DMA_CFG_EN)

#define STATUS_MSIZE		MSIZE08
#define STATUS_XMOD		(1 << STATUS_MSIZE)
#define STATUS_DMA_CFG_COMMON \
	(DMA_CFG_DESCIDCPY | DMA_CFG_INT_XCNT | \
	 DMA_CFG_NDSIZE05 | (STATUS_MSIZE << 8) | DMA_CFG_PSIZE04 | \
	 DMA_CFG_WRITE | DMA_CFG_EN)

/* DMA descriptor (in physical memory). */
struct dma_desc {
	u32 dscptr_nxt;
	u32 addrstart;
	u32 cfg;
	u32 xcnt;
	u32 xmod;
};

/* DMA register (within Internal Register Map).  */
struct dma_regs {
	u32 dscptr_nxt;		/* Pointer to next initial descriptor */
	u32 addrstart;		/* Start address of current buffer */
	u32 cfg;		/* Configuration */
	u32 xcnt;		/* Inner loop count start value */
	u32 xmod;		/* Inner loop address increment */
	u32 ycnt;		/* Outer loop count start value (2D only) */
	u32 ymod;		/* Outer loop address increment (2D only) */
	u32 dummy_1c;
	u32 dummy_20;
	u32 dscptr_cur;		/* Current descriptor pointer */
	u32 dscptr_prv;		/* Previous initial descriptor pointer */
	u32 addr_cur;		/* Current address */
	u32 stat;		/* Status */
	u32 xcnt_cur;		/* Current count (1D) or intra-row XCNT (2D) */
	u32 ycnt_cur;		/* Current row count (2D only) */
	u32 dummy_3c;
	u32 bwlcnt;             /* Bandwidth limit count */
	u32 bwlcnt_cur;         /* Bandwidth limit count current */
	u32 bwmcnt;             /* Bandwidth monitor count */
	u32 bwmcnt_cur;         /* Bandwidth monitor count current */
};

/* the following must be powers of two */
#define ADI_MSP_NUM_RDS		128  /* number of Rx descriptors */
#define ADI_MSP_NUM_TDS		128  /* number of Tx/Status descriptors */
#define ADI_MSP_NUM_SDS		ADI_MSP_NUM_TDS  /* number of Tx status descriptors */
#define ADI_MSP_RDS_MASK	(ADI_MSP_NUM_RDS - 1)
#define ADI_MSP_TDS_MASK	(ADI_MSP_NUM_TDS - 1)
#define ADI_MSP_SDS_MASK	(ADI_MSP_NUM_SDS - 1)
#define ADI_MSP_RD_RING_SIZE	(ADI_MSP_NUM_RDS * sizeof(struct dma_desc))
#define ADI_MSP_TD_RING_SIZE	(ADI_MSP_NUM_TDS * sizeof(struct dma_desc))
#define ADI_MSP_SD_RING_SIZE	(ADI_MSP_NUM_SDS * sizeof(struct dma_desc))

#if ADI_MSP_NUM_TDS != ADI_MSP_NUM_SDS
#error "Tx descriptors are not as many as Tx status descriptors"
#endif

/* Frame tag is an 8-bit field in work unit. It cannot be 0. So the number
 * of valid frame tags is 255.
 */
#define ADI_MSP_NUM_FRAME_TAGS		255
#define ADI_MSP_MIN_NONPTP_FRAME_TAG	1
#define ADI_MSP_MAX_NONPTP_FRAME_TAG	247
#define ADI_MSP_MIN_PTP_FRAME_TAG	(ADI_MSP_MAX_NONPTP_FRAME_TAG + 1)
#define ADI_MSP_MAX_PTP_FRAME_TAG	ADI_MSP_NUM_FRAME_TAGS

/* TODO  try threshold other than 1 */
#define ADI_MSP_STOP_QUEUE_TH	1

#define MTU			1500

#define TX_TIMEOUT_VALUE	0x100

/* If TX MAC does not do padding for us, we need to define this macro.
 * When this macro is define, the frame will be padded to at least 60 bytes.
 */
#ifdef CONFIG_ADI_MSP_TX_PADDING
#define TX_MIN_FRAME_SIZE	60U
#else
/* Intel E-Tile drops frames of less than nine bytes */
#define TX_MIN_FRAME_SIZE	9U
#endif
/* This includes optional 802.1Q tag */
#define TX_MAX_FRAME_SIZE	(MTU + 18)

/* Minimal length of Ethernet frame header */
#define RX_MIN_FRAME_SIZE	14
/* This includes optional 802.1Q tag */
#define RX_MAX_FRAME_SIZE	(MTU + 18)

#define RX_WU_LEN		1536

/* The size of array prev_rx_skb[] */
#define PREV_RX_SKB_NUM		6
/* How many data work units can be used for each frame.
 * Must be smaller than PREV_RX_SKB_NUM
 */
#define DATA_WU_PER_FRAME	DIV_ROUND_UP(RX_MAX_FRAME_SIZE, RX_WU_LEN)

#if DATA_WU_PER_FRAME >= PREV_RX_SKB_NUM
#error "PREV_RX_SKB_NUM should be larger than DATA_WU_PER_FRAME"
#endif

#if DATA_WU_PER_FRAME > 1
#error "Currently one Ethernet frame must fit into one data work unit"
#endif

/* This is used to assure Rx DMA buffers won't be adjacent to each other */
#define RX_WU_GUARD_SIZE	1
#define RX_WU_BUF_SIZE		(RX_WU_LEN + RX_WU_GUARD_SIZE)

/* This is used to assure Tx Status DMA buffers won't be adjacent to each other */
#define STATUS_WU_GUARD_SIZE	1
/* Must be a multiple of STATUS_XMOD since all Tx status workunit buffers are
 * allocated continuously.
 */
#define STATUS_WU_BUF_SIZE	round_up(STATUS_WU_LEN + STATUS_WU_GUARD_SIZE, \
					 STATUS_XMOD)

enum chain_status {
	FILLED,
	EMPTY
};

#define ADI_MSP_NL_STATS \
	ADI_MSP_NL_STAT(rx_packets) \
	ADI_MSP_NL_STAT(tx_packets) \
	ADI_MSP_NL_STAT(rx_bytes) \
	ADI_MSP_NL_STAT(tx_bytes) \
	ADI_MSP_NL_STAT(rx_errors) \
	ADI_MSP_NL_STAT(tx_errors) \
	ADI_MSP_NL_STAT(rx_dropped) \
	ADI_MSP_NL_STAT(tx_dropped) \
	ADI_MSP_NL_STAT(multicast) \
	ADI_MSP_NL_STAT(collisions) \
	ADI_MSP_NL_STAT(rx_length_errors) \
	ADI_MSP_NL_STAT(rx_over_errors) \
	ADI_MSP_NL_STAT(rx_crc_errors) \
	ADI_MSP_NL_STAT(rx_frame_errors) \
	ADI_MSP_NL_STAT(rx_fifo_errors) \
	ADI_MSP_NL_STAT(rx_missed_errors) \
	ADI_MSP_NL_STAT(tx_aborted_errors) \
	ADI_MSP_NL_STAT(tx_carrier_errors) \
	ADI_MSP_NL_STAT(tx_fifo_errors) \
	ADI_MSP_NL_STAT(tx_heartbeat_errors) \
	ADI_MSP_NL_STAT(tx_window_errors) \
	ADI_MSP_NL_STAT(tx_reset) \
	ADI_MSP_NL_STAT(rx_reset)

#define INTEL_ETILE_TX_STATS \
	INTEL_ETILE_STAT(tx_fragments, 0x800) \
	INTEL_ETILE_STAT(tx_jabbers, 0x802) \
	INTEL_ETILE_STAT(tx_fcs_errors, 0x804) \
	INTEL_ETILE_STAT(tx_crc_errors, 0x806) \
	INTEL_ETILE_STAT(tx_errored_multicast, 0x808) \
	INTEL_ETILE_STAT(tx_errored_broadcast, 0x80a) \
	INTEL_ETILE_STAT(tx_errored_unicast, 0x80c) \
	INTEL_ETILE_STAT(tx_err_mcast_ctrl_frames, 0x80e) \
	INTEL_ETILE_STAT(tx_err_bcast_ctrl_frames, 0x810) \
	INTEL_ETILE_STAT(tx_err_ucast_ctrl_frames, 0x812) \
	INTEL_ETILE_STAT(tx_pause_errors, 0x814) \
	INTEL_ETILE_STAT(tx_64byte_frames, 0x816) \
	INTEL_ETILE_STAT(tx_65to127bytes_frames, 0x818) \
	INTEL_ETILE_STAT(tx_128to255bytes_frames, 0x81a) \
	INTEL_ETILE_STAT(tx_256to511bytes_frames, 0x81c) \
	INTEL_ETILE_STAT(tx_512to1023bytes_frames, 0x81e) \
	INTEL_ETILE_STAT(tx_1024to1518bytes_frames, 0x820) \
	INTEL_ETILE_STAT(tx_1519tomax_frames, 0x822) \
	INTEL_ETILE_STAT(tx_oversize_frames, 0x824) \
	INTEL_ETILE_STAT(tx_multicast_frames, 0x826) \
	INTEL_ETILE_STAT(tx_broadcast_frames, 0x828) \
	INTEL_ETILE_STAT(tx_unicast_frames, 0x82a) \
	INTEL_ETILE_STAT(tx_multicast_ctrl_frames, 0x82c) \
	INTEL_ETILE_STAT(tx_broadcast_ctrl_frames, 0x82e) \
	INTEL_ETILE_STAT(tx_unicast_ctrl_frames, 0x830) \
	INTEL_ETILE_STAT(tx_pause_frames, 0x832) \
	INTEL_ETILE_STAT(tx_runt_packets, 0x834) \
	INTEL_ETILE_STAT(tx_frame_starts, 0x836) \
	INTEL_ETILE_STAT(tx_length_errored_frames, 0x838) \
	INTEL_ETILE_STAT(tx_prc_errored_frames, 0x83a) \
	INTEL_ETILE_STAT(tx_prc_frames, 0x83c) \
	INTEL_ETILE_STAT(tx_payload_bytes, 0x860) \
	INTEL_ETILE_STAT(tx_bytes, 0x862) \
	INTEL_ETILE_STAT(tx_errors, 0x864) \
	INTEL_ETILE_STAT(tx_dropped, 0x866) \
	INTEL_ETILE_STAT(tx_bad_length_type_frames, 0x868)

#define INTEL_ETILE_RX_STATS \
	INTEL_ETILE_STAT(rx_fragments, 0x900) \
	INTEL_ETILE_STAT(rx_jabbers, 0x902) \
	INTEL_ETILE_STAT(rx_fcs_errors, 0x904) \
	INTEL_ETILE_STAT(rx_crc_errors, 0x906) \
	INTEL_ETILE_STAT(rx_errored_multicast, 0x908) \
	INTEL_ETILE_STAT(rx_errored_broadcast, 0x90a) \
	INTEL_ETILE_STAT(rx_errored_unicast, 0x90c) \
	INTEL_ETILE_STAT(rx_err_mcast_ctrl_frames, 0x90e) \
	INTEL_ETILE_STAT(rx_err_bcast_ctrl_frames, 0x910) \
	INTEL_ETILE_STAT(rx_err_ucast_ctrl_frames, 0x912) \
	INTEL_ETILE_STAT(rx_pause_errors, 0x914) \
	INTEL_ETILE_STAT(rx_64byte_frames, 0x916) \
	INTEL_ETILE_STAT(rx_65to127bytes_frames, 0x918) \
	INTEL_ETILE_STAT(rx_128to255bytes_frames, 0x91a) \
	INTEL_ETILE_STAT(rx_256to511bytes_frames, 0x91c) \
	INTEL_ETILE_STAT(rx_512to1023bytes_frames, 0x91e) \
	INTEL_ETILE_STAT(rx_1024to1518bytes_frames, 0x920) \
	INTEL_ETILE_STAT(rx_1519tomax_frames, 0x922) \
	INTEL_ETILE_STAT(rx_oversize_frames, 0x924) \
	INTEL_ETILE_STAT(rx_multicast_frames, 0x926) \
	INTEL_ETILE_STAT(rx_broadcast_frames, 0x928) \
	INTEL_ETILE_STAT(rx_unicast_frames, 0x92a) \
	INTEL_ETILE_STAT(rx_multicast_ctrl_frames, 0x92c) \
	INTEL_ETILE_STAT(rx_broadcast_ctrl_frames, 0x92e) \
	INTEL_ETILE_STAT(rx_unicast_ctrl_frames, 0x930) \
	INTEL_ETILE_STAT(rx_pause_frames, 0x932) \
	INTEL_ETILE_STAT(rx_runt_packets, 0x934) \
	INTEL_ETILE_STAT(rx_frame_starts, 0x936) \
	INTEL_ETILE_STAT(rx_length_errored_frames, 0x938) \
	INTEL_ETILE_STAT(rx_prc_errored_frames, 0x93a) \
	INTEL_ETILE_STAT(rx_prc_frames, 0x93c) \
	INTEL_ETILE_STAT(rx_payload_bytes, 0x960) \
	INTEL_ETILE_STAT(rx_bytes, 0x962)

#define ADI_BRIDGE_MAC_OIF_STATS \
	ADI_BRIDGE_MAC_OIF_STAT(rx_mac_pkt_cnt, 0x28) \
	ADI_BRIDGE_MAC_OIF_STAT(tx_mac_pkt_cnt, 0x2c) \
	ADI_BRIDGE_MAC_OIF_STAT(rx_oif_pkt_cnt, 0x30) \
	ADI_BRIDGE_MAC_OIF_STAT(tx_oif_pkt_cnt, 0x34) \
	ADI_BRIDGE_MAC_OIF_STAT(rx_block_dropped_cnt, 0x38) \
	ADI_BRIDGE_MAC_OIF_STAT(tx_block_dropped_cnt, 0x3c)

/* ADI OIF TX */
#define ADI_OIF_TX_STATS \
	ADI_OIF_STAT(tx_frames, 0x34) \
	ADI_OIF_STAT(tx_preemption_frames, 0x38)

/* ADI OIF RX */
#define ADI_OIF_RX_STATS \
	ADI_OIF_STAT(rx_frames, 0x14) \
	ADI_OIF_STAT(rx_sw_frames, 0x70)

/* ADI Async FIFO RX */
#define ADI_ASYNC_FIFO_RX_STATS \
	ADI_ASYNC_FIFO_STAT(rx_dropped, 0x10)

/* ADI MSP RX */
#define ADI_MSP_RX_STATS \
	ADI_MSP_STAT(rx_dropped_mplane, 0xc) \
	ADI_MSP_STAT(rx_dropped_splane, 0x10)

struct adi_msp_nl_stats {
#define ADI_MSP_NL_STAT(S) u64 S;
	ADI_MSP_NL_STATS
#undef ADI_MSP_NL_STAT
};

struct intel_etile_tx_stats {
#define INTEL_ETILE_STAT(S, OFFSET) u64 S;
	INTEL_ETILE_TX_STATS
#undef INTEL_ETILE_STAT
};

struct intel_etile_rx_stats {
#define INTEL_ETILE_STAT(S, OFFSET) u64 S;
	INTEL_ETILE_RX_STATS
#undef INTEL_ETILE_STAT
};

struct adi_bridge_mac_oif_stats {
#define ADI_BRIDGE_MAC_OIF_STAT(S, OFFSET) u64 S;
	ADI_BRIDGE_MAC_OIF_STATS
#undef ADI_BRIDGE_MAC_OIF_STAT
};

struct adi_oif_tx_stats {
#define ADI_OIF_STAT(S, OFFSET) u64 S;
	ADI_OIF_TX_STATS
#undef ADI_OIF_STAT
};

struct adi_oif_rx_stats {
#define ADI_OIF_STAT(S, OFFSET) u64 S;
	ADI_OIF_RX_STATS
#undef ADI_OIF_STAT
};

#ifndef CONFIG_ADI_MSPRX_ASYNC_FIFO
struct adi_async_fifo_rx_stats {
#define ADI_ASYNC_FIFO_STAT(S, OFFSET) u64 S;
	ADI_ASYNC_FIFO_RX_STATS
#undef ADI_ASYNC_FIFO_STAT
};
#endif

struct adi_msp_rx_stats {
#define ADI_MSP_STAT(S, OFFSET) u64 S;
	ADI_MSP_RX_STATS
#undef ADI_MSP_STAT
};

struct adi_msp_stats {
	struct adi_msp_nl_stats		nl;
	struct intel_etile_tx_stats	etile_tx;
	struct intel_etile_rx_stats	etile_rx;
	struct adi_bridge_mac_oif_stats	bridge_mac_oif;
	struct adi_oif_tx_stats		oif_tx;
	struct adi_oif_rx_stats		oif_rx;
#ifndef CONFIG_ADI_MSPRX_ASYNC_FIFO
	struct adi_async_fifo_rx_stats	async_fifo_rx;
#endif
	struct adi_msp_rx_stats		msp_rx;
};

struct oif_tx_regs {
	u32 irq_event;		// 0x0
	u32 irq_mask;		// 0x4
	u32 irq_status;		// 0x8
	u32 dummy_c;		// 0xc
	u32 dummy_10;		// 0x10
	u32 dummy_14;		// 0x14
	u32 dummy_18;		// 0x18
	u32 dummy_1c;		// 0x1c
	u32 cfg_ip_headers;	// 0x20
	u32 dummy_24;		// 0x24
	u32 dummy_28;		// 0x28
	u32 dummy_2c;		// 0x2c
	u32 cfg_cdc_flow_ctrl;	// 0x30
	u32 stat_tx_pckt;	// 0x34
	u32 stat_pre_tx_pckt;	// 0x38
	u32 cfg_tx;		// 0x3c
	u32 cfg_tx_smac_0;	// 0x40
	u32 cfg_tx_smac_1;	// 0x44
	u32 cfg_tx_dip6_0;	// 0x48
	u32 cfg_tx_dip6_1;	// 0x4c
	u32 cfg_tx_dip6_2;	// 0x50
	u32 cfg_tx_dip6_3;	// 0x54
	u32 cfg_tx_sip6_0;	// 0x58
	u32 cfg_tx_sip6_1;	// 0x5c
	u32 cfg_tx_sip6_2;	// 0x60
	u32 cfg_tx_sip6_3;	// 0x64
};

struct oif_rx_regs {
	u32 irq_event;		// 0x0
	u32 irq_mask;		// 0x4
	u32 irq_status;		// 0x8
	u32 dummy_c;		// 0xc
	u32 ecpriid_nmatch;	// 0x10
	u32 stat_pck;		// 0x14
	u32 dummy_18;		// 0x18
	u32 dummy_1c;		// 0x1c
	u32 cfg_eaxc_en[8];	// 0x20
	u32 rx_ctrl;		// 0x40 : [4] ip_prom_mode
				//	  [0] rx_en
	u32 cfg_fr_mux_smac_0;	// 0x44 : [31:0] LSBs of 48-bit MAC addr
	u32 cfg_fr_mux_smac_1;	// 0x48 : [16] prom_mode
				//	  [15:0] MSBs of 48-bit MAC addr
	u32 dummy_4c;		// 0x4c
	u32 dummy_50;		// 0x50
	u32 dummy_54;		// 0x54
	u32 dummy_58;		// 0x58
	u32 dummy_5c;		// 0x5c
	u32 dummy_60;		// 0x60
	u32 cfg_ip_addr;	// 0x64 : [31:0] IP addr for frame mux
	u32 cfg_udp_port;	// 0x68 : [16] wildcard
				//	  [15:0] UDP Port for frame mux
	u32 dummy_6c;		// 0x6c
	u32 stat_sw_pck;	// 0x70
	u32 dummy_74;		// 0x74
	u32 dummy_78;		// 0x78
	u32 dummy_7c;		// 0x7c
	u32 stat_dmap_pck[2];	// 0x80
	u32 dummy_88;		// 0x88
	u32 dummy_8c;		// 0x8c
	u32 cfg_ipv6_addr_0;	// 0x90
	u32 cfg_ipv6_addr_1;	// 0x94
	u32 cfg_ipv6_addr_2;	// 0x98
	u32 cfg_ipv6_addr_3;	// 0x9c
};

/* Information that need to be kept for each board. */
struct adi_msp_private {
	struct msp_rx_regs __iomem *rx_regs;
	struct msp_tx_regs __iomem *tx_regs;
	struct dde_tester_regs __iomem *dde_tester_regs;
	struct dma_regs __iomem *rx_dma_regs;
	struct dma_regs __iomem *tx_dma_regs;
	struct dma_regs __iomem *status_dma_regs;
	struct oif_rx_regs __iomem *oif_rx_regs;
	struct oif_tx_regs __iomem *oif_tx_regs;
	void __iomem *axi_palau_gpio_msp_ctrl;
	void __iomem *bridge_tx_regs;
	void __iomem *bridge_rx_regs;
	void __iomem *etile_regs;
#ifndef CONFIG_ADI_MSPRX_ASYNC_FIFO
	void __iomem *async_fifo_rx_regs;
#endif

	struct dma_desc *td_ring; /* transmit descriptor ring */
	struct dma_desc *rd_ring; /* receive descriptor ring  */
	struct dma_desc *sd_ring; /* status descriptor ring  */
	dma_addr_t td_dma;
	dma_addr_t rd_dma;
	dma_addr_t sd_dma;

	struct sk_buff *tx_skb[ADI_MSP_NUM_TDS];
	struct sk_buff *rx_skb[ADI_MSP_NUM_RDS];
	u8 *status_wu;

	u8 next_nonptp_frame_tag;
	u8 last_nonptp_frame_tag;
	atomic_t available_nonptp_frame_tag_count;

	u8 next_ptp_frame_tag;
	u8 last_ptp_frame_tag;
	atomic_t available_ptp_frame_tag_count;

	dma_addr_t rx_skb_dma[ADI_MSP_NUM_RDS];
	dma_addr_t tx_skb_dma[ADI_MSP_NUM_TDS];
	dma_addr_t status_wu_dma;

	/* Used to record previous RX SKBs */
	struct sk_buff *prev_rx_skb[PREV_RX_SKB_NUM];
	int prev_rx_skb_count;

	int rx_next_done;

	int tx_next_done;
	int tx_chain_head;
	int tx_chain_tail;
	enum chain_status tx_chain_status;
	atomic_t tx_count;

	int rx_dmadone_irq;
	int rx_dde_error_irq;
	int tx_dde_error_irq;
	int status_dmadone_irq;
	int status_dde_error_irq;

	struct adi_msp_stats stats;

	/* transmit lock */
	spinlock_t lock;

	int rx_dma_halt_cnt;
	int rx_dma_run_cnt;
	struct napi_struct rx_napi;

	int tx_dma_halt_cnt;
	int tx_dma_run_cnt;

	int status_dma_halt_cnt;
	int status_dma_run_cnt;
	struct napi_struct status_napi;

	struct net_device *dev;
	struct device *dmadev;

	bool has_ptp;
	bool hwtstamp_tx_en;
	bool hwtstamp_rx_en;
	struct ptp_clock *ptp_clk;
};

static int tx_dma_error_interrupt_count;
static int rx_dma_done_interrupt_count;
static int rx_dma_error_interrupt_count;
static int status_dma_done_interrupt_count;
static int status_dma_error_interrupt_count;

static u8 next_frame_tag(struct adi_msp_private *lp, bool ptp)
{
	return ptp ? lp->next_ptp_frame_tag : lp->next_nonptp_frame_tag;
}

static u8 get_frame_tag(struct adi_msp_private *lp, bool ptp)
{
	atomic_t *available_count;
	u8 min_tag, max_tag;
	u8 tag, *next_tag;
	const char *type;

	if (ptp) {
		type = "ptp";
		available_count = &lp->available_ptp_frame_tag_count;
		min_tag = ADI_MSP_MIN_PTP_FRAME_TAG;
		max_tag = ADI_MSP_MAX_PTP_FRAME_TAG;
		next_tag = &lp->next_ptp_frame_tag;
	} else {
		type = "nonptp";
		available_count = &lp->available_nonptp_frame_tag_count;
		min_tag = ADI_MSP_MIN_NONPTP_FRAME_TAG;
		max_tag = ADI_MSP_MAX_NONPTP_FRAME_TAG;
		next_tag = &lp->next_nonptp_frame_tag;
	}

	if (atomic_read(available_count) == 0) {
		MSP_DBG("%s: no available tags for %s frame\n", lp->dev->name, type);
		return 0;
	}

	atomic_dec(available_count);

	tag = *next_tag;

	if (*next_tag == max_tag)
		*next_tag = min_tag;
	else
		(*next_tag)++;

	MSP_DBG("%s: successfully get %s frame tag %d\n", lp->dev->name, type, tag);

	return tag;
}

static int put_frame_tag(struct adi_msp_private *lp, u8 tag, bool ptp)
{
	atomic_t *available_count;
	u8 min_tag, max_tag;
	u8 expected_tag, *last_tag;
	const char *type;

	if (ptp) {
		type = "ptp";
		available_count = &lp->available_ptp_frame_tag_count;
		min_tag = ADI_MSP_MIN_PTP_FRAME_TAG;
		max_tag = ADI_MSP_MAX_PTP_FRAME_TAG;
		last_tag = &lp->last_ptp_frame_tag;
	} else {
		type = "nonptp";
		available_count = &lp->available_nonptp_frame_tag_count;
		min_tag = ADI_MSP_MIN_NONPTP_FRAME_TAG;
		max_tag = ADI_MSP_MAX_NONPTP_FRAME_TAG;
		last_tag = &lp->last_nonptp_frame_tag;
	}

	if (tag < min_tag || tag > max_tag) {
		MSP_ERR("%s: frame tag %d is not %s frame tag\n", lp->dev->name,
			tag, type);
		return -1;
	}

	atomic_inc(available_count);

	if (*last_tag == max_tag)
		expected_tag = min_tag;
	else
		expected_tag = *last_tag + 1;

	if (tag != expected_tag) {
		MSP_ERR("%s: put %s frame tag %d is not expected %d\n", lp->dev->name,
			type, tag, expected_tag);
		return -1;
	}

	*last_tag = tag;

	MSP_DBG("%s: successfully put %s frame tag %d\n", lp->dev->name, type, tag);

	return 0;
}

static u64 get_timestamp_ns(union status_wu *wu)
{
	u64 second, ns;

	second = wu->t.timestamp[3] & 0xffff;
	second <<= 32;
	second |= wu->t.timestamp[2];

	ns = wu->t.timestamp[1];
	ns += second * NSEC_PER_SEC;

	return ns;
}

static void adi_msp_enable_rx_dma_interrupts(struct adi_msp_private *lp, u8 ints)
{
	u8 value = readb(lp->axi_palau_gpio_msp_ctrl + MSP_INT_CTRL_RX);

	writeb(value | ints, lp->axi_palau_gpio_msp_ctrl + MSP_INT_CTRL_RX);
}

static void adi_msp_disable_rx_dma_interrupts(struct adi_msp_private *lp, u8 ints)
{
	u8 value = readb(lp->axi_palau_gpio_msp_ctrl + MSP_INT_CTRL_RX);

	writeb(value & ~ints, lp->axi_palau_gpio_msp_ctrl + MSP_INT_CTRL_RX);
}

static void adi_msp_enable_status_dma_interrupts(struct adi_msp_private *lp, u8 ints)
{
	u8 value = readb(lp->axi_palau_gpio_msp_ctrl + MSP_INT_CTRL_STATUS);

	writeb(value | ints, lp->axi_palau_gpio_msp_ctrl + MSP_INT_CTRL_STATUS);
}

static void adi_msp_disable_status_dma_interrupts(struct adi_msp_private *lp, u8 ints)
{
	u8 value = readb(lp->axi_palau_gpio_msp_ctrl + MSP_INT_CTRL_STATUS);

	writeb(value & ~ints, lp->axi_palau_gpio_msp_ctrl + MSP_INT_CTRL_STATUS);
}

static dma_addr_t adi_msp_rx_dma(struct adi_msp_private *lp, int idx)
{
	return lp->rd_dma + (idx & ADI_MSP_RDS_MASK) * sizeof(struct dma_desc);
}

static dma_addr_t adi_msp_tx_dma(struct adi_msp_private *lp, int idx)
{
	return lp->td_dma + (idx & ADI_MSP_TDS_MASK) * sizeof(struct dma_desc);
}

static dma_addr_t adi_msp_status_dma(struct adi_msp_private *lp, int idx)
{
	return lp->sd_dma + (idx & ADI_MSP_SDS_MASK) * sizeof(struct dma_desc);
}

static dma_addr_t adi_msp_status_wu_dma(struct adi_msp_private *lp, int idx)
{
	return lp->status_wu_dma + (idx & ADI_MSP_SDS_MASK) * STATUS_WU_BUF_SIZE;
}

static union status_wu *adi_msp_status_wu(struct adi_msp_private *lp, int idx)
{
	u8 *wu;

	wu = lp->status_wu + (idx & ADI_MSP_SDS_MASK) * STATUS_WU_BUF_SIZE;

	return (union status_wu *)wu;
}

/* transmit packet */
static int adi_msp_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	unsigned char *wu;
	struct tx_wu_header *hdr;
	u8 ptp = 0, tx_port, tag;
	atomic_t *available_frame_tag_count;
	u32 chain_prev, chain_next;
	unsigned long flags;
	struct dma_desc *td;
	dma_addr_t as; // addrstart
	u32 msize, xmod;
	u32 frame_length, wu_length, dma_stat;
	int delta_headroom;
#if defined(CONFIG_ADI_MSP_WA_TX_WU_SIZE_MULTIPLE_OF_8) || defined(CONFIG_ADI_MSP_TX_PADDING)
	int delta_tailroom, needed_tailroom;
#endif
	int idx;

	MSP_DBG("%s: Entering %s ...\n", dev->name, __func__);

	if ((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) != 0)
		ptp = TX_WU_PTP;

	if (ptp)
		available_frame_tag_count = &lp->available_ptp_frame_tag_count;
	else
		available_frame_tag_count = &lp->available_nonptp_frame_tag_count;

	spin_lock_irqsave(&lp->lock, flags);

	MSP_DBG("%s: tx_count = %d\n", dev->name, atomic_read(&lp->tx_count));

	/* we cannot support skb length larger than 0xffff */
	/* TODO  find a better value related to MTU */
	if (skb->len > 0xffff) {
		MSP_ERR("%s: SKB too huge (len = %u)!\n", dev->name, skb->len);
		goto drop_packet;
	}

	/* TODO  remove this when the code becomes stable */
	if (atomic_read(&lp->tx_count) > ADI_MSP_NUM_TDS) {
		MSP_ERR("%s: tx_count (%d) > ADI_MSP_NUM_TDS (%d) !\n", dev->name,
			atomic_read(&lp->tx_count), ADI_MSP_NUM_TDS);
		goto drop_packet;
	}

	if (atomic_read(&lp->tx_count) == ADI_MSP_NUM_TDS) {
		MSP_DBG("%s: tx ring is full, drop packet\n", dev->name);
		goto drop_packet;
	}
	if (atomic_read(available_frame_tag_count) == 0) {
		MSP_DBG("%s: no available %s frame tags, drop packet\n", dev->name,
			ptp ? "ptp" : "nonptp");
		goto drop_packet;
	}
	if (!netif_queue_stopped(dev) &&
	    (atomic_read(&lp->tx_count) >= ADI_MSP_NUM_TDS - ADI_MSP_STOP_QUEUE_TH ||
	     atomic_read(available_frame_tag_count) <= ADI_MSP_STOP_QUEUE_TH)) {
		MSP_DBG("%s: call netif_stop_queue()\n", dev->name);
		netif_stop_queue(dev);
	}

#if 1
	/* make sure that there is enough headroom for workunit header */
	if (skb_headroom(skb) < TX_WU_HEADER_LEN)
		delta_headroom = TX_WU_HEADER_LEN - skb_headroom(skb);
	else
		delta_headroom = 0;

#ifdef CONFIG_ADI_MSP_TX_PADDING
	frame_length = max(skb->len, TX_MIN_FRAME_SIZE);
#else
	frame_length = skb->len;
#endif

#ifdef CONFIG_ADI_MSP_WA_TX_WU_SIZE_MULTIPLE_OF_8
	wu_length = round_up(frame_length + TX_WU_HEADER_LEN, 8);
#else
	wu_length = frame_length + TX_WU_HEADER_LEN;
#endif

	/* make sure that there is enough tailroom for pads */
	needed_tailroom = wu_length - (skb->len + TX_WU_HEADER_LEN);
	if (needed_tailroom > skb_tailroom(skb))
		delta_tailroom = needed_tailroom - skb_tailroom(skb);
	else
		delta_tailroom = 0;

	if (delta_headroom > 0 || delta_tailroom > 0) {
		if (pskb_expand_head(skb, delta_headroom, delta_tailroom,
				     GFP_ATOMIC) != 0) {
			MSP_ERR("%s: No enough headroom or tailroom for Tx work unit\n",
				dev->name);
			goto drop_packet;
		}
	}

#ifdef CONFIG_ADI_MSP_TX_PADDING
	if (skb->len < TX_MIN_FRAME_SIZE)
		skb_put(skb, TX_MIN_FRAME_SIZE - skb->len);
#endif

#else /* if no workarounds are needed, we can just use this simple one */
	/* make sure that there is enough headroom in SKB for workunit header */
	if (skb_headroom(skb) < TX_WU_HEADER_LEN) {
		skb = skb_expand_head(skb, TX_WU_HEADER_LEN);
		if (!skb) {
			MSP_ERR("%s: No enough headroom for Tx work unit header\n", dev->name);
			goto drop_packet;
		}
	}
#endif

	/* TODO  find out how to determine which port to use. assume port 0 */
	tx_port = TX_WU_PORT_0;

	/* take a peek but not get it now */
	tag = next_frame_tag(lp, ptp);

	if (ptp)
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;

	/* fill work unit header */
	wu = skb->data - TX_WU_HEADER_LEN;
	hdr = (struct tx_wu_header *)wu;
	hdr->byte0 = WU_TYPE_TX_DATA_SOF | ptp | tx_port;
	hdr->frame_tag = tag;
	hdr->frame_len = frame_length;

	as = dma_map_single(lp->dmadev, wu, wu_length, DMA_TO_DEVICE);
	if (dma_mapping_error(lp->dmadev, as)) {
		skb_shinfo(skb)->tx_flags &= ~SKBTX_IN_PROGRESS;
		goto drop_packet;
	}

	/* get it now */
	get_frame_tag(lp, ptp);

	atomic_inc(&lp->tx_count);

	idx = lp->tx_chain_tail;
	MSP_DBG("%s: index = %d\n", dev->name, idx);
	td = &lp->td_ring[idx];

	lp->tx_skb[idx] = skb;

	/* setup the transmit DMA descriptor(s). */

	msize = min(__builtin_ctz(as), 3);
	xmod = 1 << msize;

	lp->tx_skb_dma[idx] = as;
	td->addrstart = as;
	td->cfg = TX_DMA_CFG_COMMON | DMA_CFG_FLOW_STOP | msize << 8;
	td->xcnt = (wu_length + xmod - 1) / xmod;
	td->xmod = xmod;

	chain_prev = (idx - 1) & ADI_MSP_TDS_MASK;
	chain_next = (idx + 1) & ADI_MSP_TDS_MASK;

	dma_stat = readl(&lp->tx_dma_regs->stat);
	if (DMA_STAT_RUN(dma_stat) == DMA_STAT_HALT) {
		if (lp->tx_chain_status == EMPTY) {
			MSP_DBG("%s: DMA is halted and chain is empty, just start DMA on this one: %d\n",
				dev->name, lp->tx_chain_head);

			/* Move tail */
			lp->tx_chain_tail = chain_next;

			/* Start DMA */
			writel(adi_msp_tx_dma(lp, lp->tx_chain_head),
			       &lp->tx_dma_regs->dscptr_nxt);
			writel(TX_DMA_CFG_COMMON | DMA_CFG_FLOW_DSCL,
			       &lp->tx_dma_regs->cfg);

			/* Move head to tail */
			lp->tx_chain_head = lp->tx_chain_tail;
		} else {
			MSP_DBG("%s: DMA is halted and chain is filled, link in td and start DMA from chain head %d\n",
				dev->name, lp->tx_chain_head);

			/* Link to prev */
			lp->td_ring[chain_prev].cfg |= DMA_CFG_FLOW_DSCL;
			lp->td_ring[chain_prev].dscptr_nxt = adi_msp_tx_dma(lp, idx);

			/* Move tail */
			lp->tx_chain_tail = chain_next;

			/* Start DMA */
			writel(adi_msp_tx_dma(lp, lp->tx_chain_head),
			       &lp->tx_dma_regs->dscptr_nxt);
			writel(TX_DMA_CFG_COMMON | DMA_CFG_FLOW_DSCL,
			       &lp->tx_dma_regs->cfg);

			/* Move head to tail */
			lp->tx_chain_head = lp->tx_chain_tail;
			lp->tx_chain_status = EMPTY;
		}
	} else {
		if (lp->tx_chain_status == EMPTY) {
			MSP_DBG("%s: DMA is running and chain is empty, create a new chain, head %d\n",
				dev->name, lp->tx_chain_head);

			/* Move tail */
			lp->tx_chain_tail = chain_next;

			lp->tx_chain_status = FILLED;
		} else {
			MSP_DBG("%s: DMA is running and chain is filled, link in td\n", dev->name);

			/* Link to prev */
			lp->td_ring[chain_prev].cfg |= DMA_CFG_FLOW_DSCL;
			lp->td_ring[chain_prev].dscptr_nxt = adi_msp_tx_dma(lp, idx);

			/* Move tail */
			lp->tx_chain_tail = chain_next;
		}
	}

	netif_trans_update(dev);

	spin_unlock_irqrestore(&lp->lock, flags);

	MSP_DBG("%s: ... Leaving %s\n", dev->name, __func__);
	return NETDEV_TX_OK;

drop_packet:
	MSP_DBG("%s: drop the packet\n", dev->name);
	lp->stats.nl.tx_dropped++;
	dev_kfree_skb_any(skb);
	spin_unlock_irqrestore(&lp->lock, flags);

	MSP_DBG("%s: ... Leaving %s\n", dev->name, __func__);
	return NETDEV_TX_OK;
}

/* TODO  We need an interrupt handler to count dropped frames and CRC errors */

/* Ethernet Rx DMA done interrupt */
static irqreturn_t adi_msp_rx_dma_done_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct adi_msp_private *lp = netdev_priv(dev);

	rx_dma_done_interrupt_count++;

	if (napi_schedule_prep(&lp->rx_napi)) {
		adi_msp_disable_rx_dma_interrupts(lp, MSP_INT_CTRL_DMADONE);
		__napi_schedule(&lp->rx_napi);
	}

	return IRQ_HANDLED;
}

/* Ethernet Rx DMA error interrupt */
static irqreturn_t adi_msp_rx_dma_error_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct adi_msp_private *lp = netdev_priv(dev);
	u32 dma_stat, dscptr_nxt, addrstart, cfg, xcnt, xmod;

	rx_dma_error_interrupt_count++;

	dma_stat = readl(&lp->rx_dma_regs->stat);
	dscptr_nxt = readl(&lp->rx_dma_regs->dscptr_nxt);
	addrstart = readl(&lp->rx_dma_regs->addrstart);
	cfg = readl(&lp->rx_dma_regs->cfg);
	xcnt = readl(&lp->rx_dma_regs->xcnt);
	xmod = readl(&lp->rx_dma_regs->xmod);
	MSP_ERR("%s: Rx DMA error %x %x %x %x %d %d %d\n", dev->name, dma_stat,
		dscptr_nxt, addrstart, cfg, xcnt, xmod, lp->rx_dma_halt_cnt);

	writel(DMA_STAT_IRQERR, &lp->rx_dma_regs->stat);

	return IRQ_HANDLED;
}

/* Print 16 bytes per line. Print only first 5 lines and last 5 lines */
#define BYTES_PER_LINE	16
#define FIRST_LINE_NUM	5
#define LAST_LINE_NUM	5
static void adi_msp_dump_prev_rx_skb(struct net_device *dev)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	int i, j, k;

	/* dump the first and last two bytes of the SKB buffer */
	for (i = 0; i < lp->prev_rx_skb_count; i++) {
		int header_type = lp->prev_rx_skb[i]->data[0] & WU_TYPE_MASK;
		bool is_sof = (lp->prev_rx_skb[i]->data[0] & 0x4) != 0;
		bool is_err = is_sof;
		const char *header_type_str;
		int total_lines;

		if (header_type == WU_TYPE_RX_DATA) {
			if (is_sof)
				header_type_str = "SOF data work unit";
			else
				header_type_str = "non-SOF data work unit";
		} else if (header_type == WU_TYPE_RX_STAT) {
			if (is_err)
				header_type_str = "status work unit (ERR = 1)";
			else
				header_type_str = "status work unit (ERR = 0)";
		} else {
			header_type_str = "UNKNOWN type work unit";
		}

		if (header_type != WU_TYPE_RX_STAT)
			total_lines = (RX_WU_LEN + BYTES_PER_LINE - 1)
					/ BYTES_PER_LINE;
		else
			total_lines = 1;

		MSP_DBG("%s: prev_rx_skb[%d] %s\n", dev->name, i, header_type_str);
		for (j = 0; j < total_lines; j++) {
			char buf[3 * BYTES_PER_LINE + 1];

			if (j >= FIRST_LINE_NUM &&
			    j < total_lines - LAST_LINE_NUM)
				continue;

			buf[0] = '\0';
			for (k = 0; k < BYTES_PER_LINE; k++)
				sprintf(buf + strlen(buf), " %02x",
					lp->prev_rx_skb[i]->data[j * BYTES_PER_LINE + k]);
			MSP_DBG("%s: %8x:%s\n", dev->name, j * BYTES_PER_LINE, buf);
		}
	}
}

static void adi_msp_drop_prev_rx_skb(struct net_device *dev, int budget)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	int i;

	MSP_DBG("%s: drop %d work units\n", dev->name, lp->prev_rx_skb_count);
	for (i = 0; i < lp->prev_rx_skb_count; i++) {
		napi_consume_skb(lp->prev_rx_skb[i], budget);
		lp->prev_rx_skb[i] = NULL;
	}
	lp->prev_rx_skb_count = 0;
}

static int adi_msp_rx(struct net_device *dev, int budget)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	struct dma_desc *rd = &lp->rd_ring[lp->rx_next_done];
	u32 dma_stat;
	int count;

	MSP_DBG("%s: Entering %s ...\n", dev->name, __func__);

	count = 0;

msp_rx_loop:

	while (count < budget) {
		int idx = lp->rx_next_done;
		struct sk_buff *skb, *skb_new;
		u32 addr_cur, dscptr_prv, addrstart;
		u32 chain_prev;
		dma_addr_t as;

		MSP_DBG("%s: count = %d idx = %d\n", dev->name, count, idx);
		skb = lp->rx_skb[idx];
		skb_new = NULL;

		dma_sync_single_for_cpu(lp->dmadev, lp->rx_skb_dma[idx],
					RX_WU_LEN, DMA_FROM_DEVICE);

		MSP_DBG("%s: skb->data[0] = 0x%x\n", dev->name, skb->data[0]);

		/* If the first byte has not been written, this work unit has
		 * not been started yet.
		 */
		if (skb->data[0] == 0) {
			MSP_DBG("%s: skb->data[0] == 0  ==>  break\n", dev->name);
			break;
		}

		/* If skb->data[0] is not zero, this work unit has been
		 * started. But if the current address is still in this work
		 * unit and the initial descriptor address has not been
		 * copied into the DSCPTR_PREV register, this work unit is
		 * not done yet.
		 */
		addr_cur = readl(&lp->rx_dma_regs->addr_cur);
		dscptr_prv = readl(&lp->rx_dma_regs->dscptr_prv);
		dscptr_prv &= ~0x3;
		addrstart = lp->rd_ring[idx].addrstart;
		if (addr_cur >= addrstart &&
		    addr_cur < addrstart + RX_WU_LEN &&
		    dscptr_prv != adi_msp_rx_dma(lp, idx)) {
			MSP_DBG("%s: skb not done by DMA  ==>  break\n", dev->name);
			break;
		}

		/* Malloc up new buffer. */
		skb_new = napi_alloc_skb(&lp->rx_napi, RX_WU_BUF_SIZE);
		if (unlikely(!skb_new)) {
			MSP_ERR("%s: cannot alloc new skb\n", dev->name);
			break;
		}

		if (unlikely(((unsigned long long)skb_new->data) & 0x7)) {
			MSP_ERR("%s: new skb data not aligned to 8\n", dev->name);
			break;
		}
		/* Initialize the first byte of work unit header to 0 */
		skb_new->data[0] = 0;

		as = dma_map_single(lp->dmadev, skb_new->data, RX_WU_LEN,
				    DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(lp->dmadev, as))) {
			MSP_ERR("%s: dma map new skb failed\n", dev->name);
			napi_consume_skb(skb_new, budget);
			break;
		}

		dma_unmap_single(lp->dmadev, lp->rx_skb_dma[idx],
				 RX_WU_LEN, DMA_FROM_DEVICE);

		lp->rx_skb[idx] = skb_new;
		lp->rx_skb_dma[idx] = as;

		if ((skb->data[0] & WU_TYPE_MASK) == WU_TYPE_RX_DATA &&
		    (skb->data[0] & RX_DATA_WU_HEADER_RESERVED_BITS) == 0) {
			if (likely((skb->data[0] & RX_DATA_WU_HEADER_SOF) != 0)) {
				/* this is start of frame work unit*/

				if (unlikely(lp->prev_rx_skb_count > 0)) {
					MSP_ERR("%s: unexpected SOF work unit, will drop previous %d work unit(s)\n",
						dev->name, lp->prev_rx_skb_count);

					adi_msp_dump_prev_rx_skb(dev);
					adi_msp_drop_prev_rx_skb(dev, budget);

					lp->stats.nl.rx_errors++;
				}

				lp->prev_rx_skb[0] = skb;
				lp->prev_rx_skb_count = 1;
			} else {
				if (unlikely(lp->prev_rx_skb_count == 0)) {
					MSP_ERR("%s: non-SOF work unit does not follow an SOF work unit, will be dropped\n",
						dev->name);

					lp->prev_rx_skb[0] = skb;
					lp->prev_rx_skb_count = 1;

					adi_msp_dump_prev_rx_skb(dev);
					adi_msp_drop_prev_rx_skb(dev, budget);

					lp->stats.nl.rx_errors++;
				} else if (unlikely(lp->prev_rx_skb_count == PREV_RX_SKB_NUM - 1)) {
					MSP_ERR("%s: Ethernet frame uses too many work units, will be dropped\n",
						dev->name);
					lp->prev_rx_skb[lp->prev_rx_skb_count] = skb;
					lp->prev_rx_skb_count++;

					adi_msp_dump_prev_rx_skb(dev);
					adi_msp_drop_prev_rx_skb(dev, budget);

					lp->stats.nl.rx_errors++;
				} else {
					lp->prev_rx_skb[lp->prev_rx_skb_count] = skb;
					lp->prev_rx_skb_count++;
				}
			}
		} else if ((skb->data[0] & WU_TYPE_MASK) == WU_TYPE_RX_STAT &&
			   (skb->data[0] & RX_STAT_WU_HEADER_RESERVED_BITS) == 0) {
			MSP_DBG("%s: ethernet frame received from port %d\n",
				dev->name, (skb->data[0] & RX_STAT_WU_HEADER_PORT) ? 1 : 0);

			if (unlikely((skb->data[0] & RX_STAT_WU_HEADER_DROPPED_ERR) != 0)) {
				MSP_ERR("%s: status work unit indicates frame dropped error\n",
					dev->name);

				lp->prev_rx_skb[lp->prev_rx_skb_count] = skb;
				lp->prev_rx_skb_count++;

				adi_msp_dump_prev_rx_skb(dev);
				adi_msp_drop_prev_rx_skb(dev, budget);

				/* According to the spec, it can be a CRC error
				 * or frame length error. But we don't know
				 * which it is.
				 */
				lp->stats.nl.rx_errors++;

				count++;
			} else if (unlikely((skb->data[0] & RX_STAT_WU_HEADER_ERR) != 0)) {
				MSP_ERR("%s: status work unit indicates error, will be dropped\n",
					dev->name);

				lp->prev_rx_skb[lp->prev_rx_skb_count] = skb;
				lp->prev_rx_skb_count++;

				adi_msp_dump_prev_rx_skb(dev);
				adi_msp_drop_prev_rx_skb(dev, budget);

				/* According to the spec, it can be a CRC error
				 * or frame length error. But we don't know
				 * which it is.
				 */
				lp->stats.nl.rx_errors++;

				count++;
			} else if (unlikely(lp->prev_rx_skb_count == 0)) {
				MSP_ERR("%s: status work unit does not follow data work unit(s), will be dropped\n",
					dev->name);

				lp->prev_rx_skb[0] = skb;
				lp->prev_rx_skb_count = 1;

				adi_msp_dump_prev_rx_skb(dev);
				adi_msp_drop_prev_rx_skb(dev, budget);

				lp->stats.nl.rx_errors++;
			} else if (unlikely(lp->prev_rx_skb_count > DATA_WU_PER_FRAME)) {
				MSP_ERR("%s: Ethernet frame larger than MTU, will be dropped\n",
					dev->name);

				lp->prev_rx_skb[lp->prev_rx_skb_count] = skb;
				lp->prev_rx_skb_count++;

				adi_msp_dump_prev_rx_skb(dev);
				adi_msp_drop_prev_rx_skb(dev, budget);

				/* According to the spec, it can be a CRC error
				 * or frame length error. But we don't know
				 * which it is.
				 */
				lp->stats.nl.rx_errors++;

				count++;
			} else {
				/* TODO  support DATA_WU_PER_FRAME > 1 */
				struct sk_buff *skb_prev;
				union status_wu *status_wu;
				u32 pkt_len;

				MSP_DBG("%s: processing received ethernet frame data and status work units\n",
					dev->name);

				skb_prev = lp->prev_rx_skb[0];
				lp->prev_rx_skb[0] = NULL;
				lp->prev_rx_skb_count = 0;

				status_wu = (union status_wu *)skb->data;
				pkt_len = status_wu->s.frame_len;

				MSP_DBG("%s: Ethernet frame length = %d\n", dev->name, pkt_len);

				if (unlikely(lp->hwtstamp_rx_en)) {
					struct skb_shared_hwtstamps *hwtstamps;
					u64 ns = get_timestamp_ns(status_wu);

					MSP_DBG("%s: timestamp = %llu\n", dev->name, ns);

					hwtstamps = skb_hwtstamps(skb_prev);
					memset(hwtstamps, 0, sizeof(*hwtstamps));
					hwtstamps->hwtstamp = ns_to_ktime(ns);
				}

				napi_consume_skb(skb, budget);

				skb_put(skb_prev, pkt_len + RX_DATA_WU_HEADER_LEN);
				/* Remove work unit header */
				skb_pull(skb_prev, RX_DATA_WU_HEADER_LEN);
				skb_prev->protocol =
					eth_type_trans(skb_prev, dev);

				/* Pass the packet to upper layers */
				netif_receive_skb(skb_prev);
				//napi_gro_receive(&lp->rx_napi, skb_prev);
				lp->stats.nl.rx_packets++;
				lp->stats.nl.rx_bytes += pkt_len;

				count++;
			}
		} else {
			/* Invalid work unit header type */

			MSP_DBG("%s: invalid work unit header type, will be dropped\n",
				dev->name);

			lp->prev_rx_skb[lp->prev_rx_skb_count] = skb;
			lp->prev_rx_skb_count++;

			adi_msp_dump_prev_rx_skb(dev);
			adi_msp_drop_prev_rx_skb(dev, budget);

			lp->stats.nl.rx_errors++;
		}

		MSP_DBG("%s: now put back rd to rd_ring ...\n", dev->name);

		rd->addrstart = lp->rx_skb_dma[idx];
		rd->cfg = RX_DMA_CFG_COMMON | DMA_CFG_FLOW_STOP;

		chain_prev = (idx - 1) & ADI_MSP_RDS_MASK;
		lp->rd_ring[chain_prev].cfg = RX_DMA_CFG_COMMON | DMA_CFG_FLOW_DSCL;

		lp->rx_next_done = (idx + 1) & ADI_MSP_RDS_MASK;
		rd = &lp->rd_ring[lp->rx_next_done];

		writel(DMA_STAT_IRQDONE, &lp->rx_dma_regs->stat);
	}

	/* If DMA is idle or stops, there are three cases:
	 *
	 *  - It stops at the one before RD, all completed rds have been done
	 *
	 *    Restart DMA and return min(COUNT, BUDGET - 1)
	 *
	 *  - It stops at RD or some rd after RD, we just used up budget
	 *
	 *    Do not restart DMA and return COUNT which is equal to BUDGET
	 *
	 *  - It stops at RD or some rd after RD
	 *    When we check RD, it has not been done. But DMA starts process it
	 *    after checking. When we reach here, DMA has done it and might also
	 *    some rds after it and halted.
	 *
	 *    Go back to the loop to proccess the newly done rds
	 */
	dma_stat = readl(&lp->rx_dma_regs->stat);
	if (DMA_STAT_RUN(dma_stat) == DMA_STAT_HALT) {
		int idx = lp->rx_next_done;
		struct sk_buff *skb = lp->rx_skb[idx];

		lp->rx_dma_halt_cnt++;

		if (skb->data[0] == 0) {
			MSP_DBG("%s: Rx DMA is halted. Restart it from %d\n",
				dev->name, idx);

			writel(DMA_STAT_IRQDONE | DMA_STAT_IRQERR,
			       &lp->rx_dma_regs->stat);

			writel(adi_msp_rx_dma(lp, rd - lp->rd_ring),
			       &lp->rx_dma_regs->dscptr_nxt);
			writel(RX_DMA_CFG_COMMON | DMA_CFG_FLOW_DSCL,
			       &lp->rx_dma_regs->cfg);

			count = min(count, budget - 1);
		} else if (count < budget) {
			MSP_DBG("%s: Rx DMA is halted. Use remaining budget\n",
				dev->name);
			goto msp_rx_loop;
		} else {
			MSP_DBG("%s: Rx DMA is halted. Budget is used up\n",
				dev->name);
		}
	} else {
		MSP_DBG("%s: DMA is running\n", dev->name);
	}

	MSP_DBG("%s: ... Leaving count = %d\n", dev->name, count);

	return count;
}

static int adi_msp_rx_poll(struct napi_struct *napi, int budget)
{
	struct adi_msp_private *lp =
		container_of(napi, struct adi_msp_private, rx_napi);
	struct net_device *dev = lp->dev;
	int work_done;

	work_done = adi_msp_rx(dev, budget);
	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		adi_msp_enable_rx_dma_interrupts(lp, MSP_INT_CTRL_DMADONE);
	}
	return work_done;
}

/* TODO  implement a recover method */
static irqreturn_t adi_msp_tx_dma_error_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct adi_msp_private *lp = netdev_priv(dev);
	u32 dma_stat, dscptr_nxt, addrstart, cfg, xcnt, xmod;

	tx_dma_error_interrupt_count++;

	dma_stat = readl(&lp->tx_dma_regs->stat);
	dscptr_nxt = readl(&lp->tx_dma_regs->dscptr_nxt);
	addrstart = readl(&lp->tx_dma_regs->addrstart);
	cfg = readl(&lp->tx_dma_regs->cfg);
	xcnt = readl(&lp->tx_dma_regs->xcnt);
	xmod = readl(&lp->tx_dma_regs->xmod);
	MSP_ERR("%s: Tx DMA error %x %x %x %x %d %d %d\n", dev->name,
		dma_stat, dscptr_nxt, addrstart, cfg, xcnt, xmod, lp->tx_dma_halt_cnt);

	writel(DMA_STAT_IRQERR, &lp->tx_dma_regs->stat);

	return IRQ_HANDLED;
}

/* Ethernet Tx status DMA done interrupt */
static irqreturn_t adi_msp_status_dma_done_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct adi_msp_private *lp = netdev_priv(dev);

	status_dma_done_interrupt_count++;

	if (napi_schedule_prep(&lp->status_napi)) {
		adi_msp_disable_status_dma_interrupts(lp, MSP_INT_CTRL_DMADONE);
		__napi_schedule(&lp->status_napi);
	}

	return IRQ_HANDLED;
}

/* Ethernet Tx status DMA error interrupt */
static irqreturn_t adi_msp_status_dma_error_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct adi_msp_private *lp = netdev_priv(dev);
	u32 dma_stat, dscptr_nxt, addrstart, cfg, xcnt, xmod;

	status_dma_error_interrupt_count++;

	dma_stat = readl(&lp->status_dma_regs->stat);
	dscptr_nxt = readl(&lp->status_dma_regs->dscptr_nxt);
	addrstart = readl(&lp->status_dma_regs->addrstart);
	cfg = readl(&lp->status_dma_regs->cfg);
	xcnt = readl(&lp->status_dma_regs->xcnt);
	xmod = readl(&lp->status_dma_regs->xmod);
	MSP_ERR("%s: Tx status DMA error %x %x %x %x %d %d %d\n", dev->name,
		dma_stat, dscptr_nxt, addrstart, cfg, xcnt, xmod, lp->status_dma_halt_cnt);

	writel(DMA_STAT_IRQERR, &lp->status_dma_regs->stat);

	return IRQ_HANDLED;
}

static void adi_msp_show_tx_status(struct net_device *dev)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	u32 stat_ctrl, intr_stat;

	stat_ctrl = readl(&lp->tx_regs->stat_ctrl);
	intr_stat = readl(&lp->tx_regs->intr_stat);

	MSP_ERR("%s:   MSP TX status:\n", dev->name);
	MSP_ERR("%s:       STAT_CTRL: 0x%08x\n",
		dev->name, stat_ctrl);
	MSP_ERR("%s:    frame length: %d\n",
		dev->name, (stat_ctrl >> 4) & 0xffff);
	MSP_ERR("%s:       frame_err: %d\n",
		dev->name, (stat_ctrl >> 3) & 0x1);
	MSP_ERR("%s:         wu type: %d\n",
		dev->name, (stat_ctrl >> 1) & 0x3);
	MSP_ERR("%s:       INTR_STAT: 0x%08x\n",
		dev->name, intr_stat);
	MSP_ERR("%s:  stat fifo full: %d\n",
		dev->name, (intr_stat >> 4) & 0x1);
	MSP_ERR("%s:            size: %d\n",
		dev->name, (intr_stat >> 3) & 0x1);
	MSP_ERR("%s:      stat compl: %d\n",
		dev->name, (intr_stat >> 2) & 0x1);
	MSP_ERR("%s:        wu compl: %d\n",
		dev->name, (intr_stat >> 1) & 0x1);
	MSP_ERR("%s:      header err: %d\n",
		dev->name, intr_stat & 0x1);
}

static int adi_msp_status(struct net_device *dev, int budget)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	struct dma_desc *sd = &lp->sd_ring[lp->tx_next_done];
	unsigned long flags;
	u32 dma_stat;
	int count;

	MSP_DBG("%s: Entering %s ...\n", dev->name, __func__);

	count = 0;

msp_status_loop:

	while (count < budget) {
		int idx = lp->tx_next_done;
		union status_wu *wu;
		unsigned char *tx_wu;
		struct tx_wu_header *tx_wu_hdr;
		struct sk_buff *skb;
		int tmp;
		u32 addr_cur, dscptr_prv, addrstart;
		u32 chain_prev;
		u32 length;
		u8 byte0, tag, ptp;

		MSP_DBG("%s: count = %d idx = %d\n", dev->name, count, idx);

		wu = adi_msp_status_wu(lp, idx);

		byte0 = wu->s.byte0;
		ptp = byte0 & TX_STATUS_WU_PTP;
		tag = wu->s.frame_tag;

		MSP_DBG("%s: byte0 = %02x\n", dev->name, byte0);

		/* If the first byte has not been written, this work unit has
		 * not been started yet.
		 */
		if (byte0 == 0)
			break;

		/* If the first byte is not zero, this work unit has been
		 * started. But if the current address is still in this work
		 * unit and the initial descriptor address has not been
		 * copied into the DSCPTR_PREV register, this work unit is
		 * not done yet.
		 */
		addr_cur = readl(&lp->status_dma_regs->addr_cur);
		dscptr_prv = readl(&lp->status_dma_regs->dscptr_prv);
		dscptr_prv &= ~0x3;
		addrstart = lp->sd_ring[idx].addrstart;
		if (addr_cur >= addrstart &&
		    addr_cur < addrstart + STATUS_WU_LEN &&
		    dscptr_prv != adi_msp_status_dma(lp, idx))
			break;

		/* If work unit type is not expected, have to reset Tx */
		if (unlikely((byte0 & WU_TYPE_MASK) != WU_TYPE_TX_STAT)) {
			MSP_ERR("%s: Invalid Tx status work unit header type (%d)",
				dev->name, byte0 & WU_TYPE_MASK);
			lp->stats.nl.tx_errors++;
			lp->stats.nl.tx_reset++;
			goto reset_tx;
		}

		/* If the corresponding Tx work unit does not exist, have to
		 * reset Tx
		 */
		skb = lp->tx_skb[idx];
		if (unlikely(!skb)) {
			MSP_ERR("%s: tx_skb[%d] == NULL\n", dev->name, idx);
			lp->stats.nl.tx_errors++;
			lp->stats.nl.tx_reset++;
			goto reset_tx;
		}

		/* Process this SKB and Tx wu */

#ifdef CONFIG_ADI_MSP_WA_TX_WU_SIZE_MULTIPLE_OF_8
		length = round_up(skb->len + TX_WU_HEADER_LEN, 8);
#else
		length = skb->len + TX_WU_HEADER_LEN;
#endif

		dma_unmap_single(lp->dmadev, lp->tx_skb_dma[idx], length,
				 DMA_TO_DEVICE);

		lp->tx_skb[idx] = NULL;

		tmp = atomic_dec_return(&lp->tx_count);
		MSP_DBG("%s: tx_count dec by 1 = %d\n", dev->name, tmp);

		if (unlikely(put_frame_tag(lp, tag, ptp) < 0)) {
			lp->stats.nl.tx_errors++;
			lp->stats.nl.tx_reset++;
			goto reset_tx;
		}

		if (netif_queue_stopped(dev) &&
		    atomic_read(&lp->tx_count) <= ADI_MSP_NUM_TDS - ADI_MSP_STOP_QUEUE_TH &&
		    atomic_read(&lp->available_ptp_frame_tag_count) >= ADI_MSP_STOP_QUEUE_TH &&
		    atomic_read(&lp->available_nonptp_frame_tag_count) >= ADI_MSP_STOP_QUEUE_TH) {
			MSP_DBG("%s: call netif_wake_queue()\n", dev->name);
			netif_wake_queue(dev);
		}

		tx_wu = skb->data - TX_WU_HEADER_LEN;
		tx_wu_hdr = (struct tx_wu_header *)tx_wu;

		if (unlikely(tag != tx_wu_hdr->frame_tag)) {
			MSP_ERR("%s: status wu tag (%d) does not match Tx wu tag (%d)\n",
				dev->name, tag, tx_wu_hdr->frame_tag);
			lp->stats.nl.tx_errors++;
			lp->stats.nl.tx_reset++;
			goto reset_tx;
		}

		if (unlikely(byte0 & TX_STATUS_WU_ERR)) {
			if (ptp && get_timestamp_ns(wu) == 0)
				MSP_ERR("%s: Failed to get timestamp for TX PTP (frame tag: %d)",
					dev->name, tag);
			else
				MSP_ERR("%s: Transmit error for SKB (frame tag: %d)",
					dev->name, tag);

			adi_msp_show_tx_status(dev);
			lp->stats.nl.tx_errors++;
			napi_consume_skb(skb, budget);
			goto reset_desc_and_wu;
		} else if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_IN_PROGRESS)) {
			MSP_DBG("%s: SKBTX_IN_PROGRESS is set in this SKB\n", dev->name);

			if (likely(ptp)) {
				struct skb_shared_hwtstamps shhwtstamps;
				u64 ns = get_timestamp_ns(wu);

				memset(&shhwtstamps, 0, sizeof(shhwtstamps));
				shhwtstamps.hwtstamp = ns_to_ktime(ns);
				MSP_DBG("%s: hardware timestamp: %llu\n", dev->name, ns);
				skb_tstamp_tx(skb, &shhwtstamps);
			} else {
				MSP_ERR("%s: PTP flag not set in Tx status work unit (frame tag: %d)",
					dev->name, tag);
				lp->stats.nl.tx_errors++;
				napi_consume_skb(skb, budget);
				goto reset_desc_and_wu;
			}
		}

		lp->stats.nl.tx_packets++;
		lp->stats.nl.tx_bytes += tx_wu_hdr->frame_len;

		napi_consume_skb(skb, budget);

reset_desc_and_wu:
		count++;

		MSP_DBG("%s: reset desc and wu\n", dev->name);

		memset(wu, 0, STATUS_WU_LEN);

		sd->cfg = STATUS_DMA_CFG_COMMON | DMA_CFG_FLOW_STOP;

		chain_prev = (idx - 1) & ADI_MSP_SDS_MASK;
		lp->sd_ring[chain_prev].cfg = STATUS_DMA_CFG_COMMON | DMA_CFG_FLOW_DSCL;

		lp->tx_next_done = (idx + 1) & ADI_MSP_SDS_MASK;
		sd = &lp->sd_ring[lp->tx_next_done];

		writel(DMA_STAT_IRQDONE, &lp->status_dma_regs->stat);
	}

	/* Restart Tx DMA if we have something to send and it's halted */
	spin_lock_irqsave(&lp->lock, flags);

	dma_stat = readl(&lp->tx_dma_regs->stat);
	if (DMA_STAT_RUN(dma_stat) == DMA_STAT_HALT) {
		if (lp->tx_chain_status == FILLED) {
			MSP_DBG("%s: Tx DMA is halted and chain is filled, restart DMA from chain head %d\n",
				dev->name, lp->tx_chain_head);

			writel(adi_msp_tx_dma(lp, lp->tx_chain_head),
			       &lp->tx_dma_regs->dscptr_nxt);
			writel(TX_DMA_CFG_COMMON | DMA_CFG_FLOW_DSCL,
			       &lp->tx_dma_regs->cfg);

			lp->tx_chain_head = lp->tx_chain_tail;
			lp->tx_chain_status = EMPTY;

			netif_trans_update(dev);

		} else {
			MSP_DBG("%s: Tx DMA is halted but chain is empty\n", dev->name);
		}
	} else {
		MSP_DBG("%s: Tx DMA is running\n", dev->name);
	}

	spin_unlock_irqrestore(&lp->lock, flags);

	/* See comment in adi_msp_rx() */

	dma_stat = readl(&lp->status_dma_regs->stat);
	if (DMA_STAT_RUN(dma_stat) == DMA_STAT_HALT) {
		int idx = lp->tx_next_done;
		union status_wu *wu = adi_msp_status_wu(lp, idx);

		lp->status_dma_halt_cnt++;

		if (wu->s.byte0 == 0) {
			MSP_DBG("%s: Tx status DMA is halted, restart DMA from %d\n",
				dev->name, idx);

			writel(DMA_STAT_IRQDONE | DMA_STAT_IRQERR,
			       &lp->status_dma_regs->stat);

			writel(adi_msp_status_dma(lp, sd - lp->sd_ring),
			       &lp->status_dma_regs->dscptr_nxt);
			writel(STATUS_DMA_CFG_COMMON | DMA_CFG_FLOW_DSCL,
			       &lp->status_dma_regs->cfg);

			count = min(count, budget - 1);
		} else if (count < budget) {
			MSP_DBG("%s: Tx status DMA is halted. Use remaining budget\n",
				dev->name);
			goto msp_status_loop;
		} else {
			MSP_DBG("%s: Tx status DMA is halted. Budget is used up\n",
				dev->name);
		}
	} else {
		MSP_DBG("%s: Tx status DMA is running\n", dev->name);
	}

	MSP_DBG("%s: ... Leaving count = %d\n", dev->name, count);

	return count;

reset_tx:
	/* TODO  implement reset MSP Tx */
	MSP_ERR("%s: reset MSP Tx\n", dev->name);
	return count;
}

static int adi_msp_status_poll(struct napi_struct *napi, int budget)
{
	struct adi_msp_private *lp =
		container_of(napi, struct adi_msp_private, status_napi);
	struct net_device *dev = lp->dev;
	int work_done;

	work_done = adi_msp_status(dev, budget);
	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		adi_msp_enable_status_dma_interrupts(lp, MSP_INT_CTRL_DMADONE);
	}
	return work_done;
}

/* ethtool helpers */
static void adi_msp_get_drvinfo(struct net_device *dev,
				struct ethtool_drvinfo *info)
{
	struct adi_msp_private *lp = netdev_priv(dev);

	strlcpy(info->driver, DRV_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_VERSION, sizeof(info->version));
	strlcpy(info->bus_info, lp->dev->name, sizeof(info->bus_info));
}

static const char adi_msp_gstrings[][ETH_GSTRING_LEN] = {
#define ADI_MSP_NL_STAT(S) "netlink."#S,
	ADI_MSP_NL_STATS
#undef ADI_MSP_NL_STAT

#define INTEL_ETILE_STAT(S, OFFSET) "etile."#S,
	INTEL_ETILE_TX_STATS
	INTEL_ETILE_RX_STATS
#undef INTEL_ETILE_STAT

#define ADI_BRIDGE_MAC_OIF_STAT(S, OFFSET) "bridge."#S,
	ADI_BRIDGE_MAC_OIF_STATS
#undef  ADI_BRIDGE_MAC_OIF_STAT

#define ADI_OIF_STAT(S, OFFSET) "oif."#S,
	ADI_OIF_TX_STATS
	ADI_OIF_RX_STATS
#undef ADI_OIF_STAT

#ifndef CONFIG_ADI_MSPRX_ASYNC_FIFO
#define ADI_ASYNC_FIFO_STAT(S, OFFSET) "async_fifo."#S,
	ADI_ASYNC_FIFO_RX_STATS
#undef ADI_ASYNC_FIFO_STAT
#endif

#define ADI_MSP_STAT(S, OFFSET) "msp."#S,
	ADI_MSP_RX_STATS
#undef ADI_MSP_STAT
};

#define ADI_MSP_STATS_LEN ARRAY_SIZE(adi_msp_gstrings)

static const int intel_etile_tx_stats_offsets[] = {
#define INTEL_ETILE_STAT(S, OFFSET) OFFSET,
	INTEL_ETILE_TX_STATS
#undef INTEL_ETILE_STAT
};

static const int intel_etile_rx_stats_offsets[] = {
#define INTEL_ETILE_STAT(S, OFFSET) OFFSET,
	INTEL_ETILE_RX_STATS
#undef INTEL_ETILE_STAT
};

static const int adi_bridge_mac_oif_stats_offsets[] = {
#define ADI_BRIDGE_MAC_OIF_STAT(S, OFFSET) OFFSET,
	ADI_BRIDGE_MAC_OIF_STATS
#undef ADI_BRIDGE_MAC_OIF_STAT
};

static const int adi_oif_tx_stats_offsets[] = {
#define ADI_OIF_STAT(S, OFFSET) OFFSET,
	ADI_OIF_TX_STATS
#undef ADI_OIF_STAT
};

static const int adi_oif_rx_stats_offsets[] = {
#define ADI_OIF_STAT(S, OFFSET) OFFSET,
	ADI_OIF_RX_STATS
#undef ADI_OIF_STAT
};

#ifndef CONFIG_ADI_MSPRX_ASYNC_FIFO
static const int adi_async_fifo_rx_stats_offsets[] = {
#define ADI_ASYNC_FIFO_STAT(S, OFFSET) OFFSET,
	ADI_ASYNC_FIFO_RX_STATS
#undef ADI_ASYNC_FIFO_STAT
};
#endif

static const int adi_msp_rx_stats_offsets[] = {
#define ADI_MSP_STAT(S, OFFSET) OFFSET,
	ADI_MSP_RX_STATS
#undef ADI_MSP_STAT
};

static int adi_msp_get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ADI_MSP_STATS_LEN;
	case ETH_SS_TEST:
	case ETH_SS_PRIV_FLAGS:
	default:
		return -EOPNOTSUPP;
	}
}

static void adi_msp_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	switch (stringset) {
	case ETH_SS_STATS:
		memcpy(data, adi_msp_gstrings, sizeof(adi_msp_gstrings));
		break;
	default:
		break;
	}
}

static void fill_intel_etile_tx_stats(struct adi_msp_private *lp)
{
	u64 *data = (u64 *)&lp->stats.etile_tx;
	int num = ARRAY_SIZE(intel_etile_tx_stats_offsets);
	const int *offsets = intel_etile_tx_stats_offsets;
	int i;

	for (i = 0; i < num; i++) {
		data[i] = readl(lp->etile_regs + (offsets[i] + 1) * 4);
		data[i] = (data[i] << 32) + readl(lp->etile_regs + offsets[i] * 4);
	}
}

static void fill_intel_etile_rx_stats(struct adi_msp_private *lp)
{
	u64 *data = (u64 *)&lp->stats.etile_rx;
	int num = ARRAY_SIZE(intel_etile_rx_stats_offsets);
	const int *offsets = intel_etile_rx_stats_offsets;
	int i;

	for (i = 0; i < num; i++) {
		data[i] = readl(lp->etile_regs + (offsets[i] + 1) * 4);
		data[i] = (data[i] << 32) + readl(lp->etile_regs + offsets[i] * 4);
	}
}

static void fill_bridge_mac_oif_stats(struct adi_msp_private *lp)
{
	u64 *data = (u64 *)&lp->stats.bridge_mac_oif;
	int num = ARRAY_SIZE(adi_bridge_mac_oif_stats_offsets);
	const int *offsets = adi_bridge_mac_oif_stats_offsets;
	int i;

	/* write (0x4000 + offsets[i])[25:14] */
	writel(1, lp->etile_regs + 0x4004);
	for (i = 0; i < num; i++)
		data[i] = readl(lp->etile_regs + ((0x4000 + offsets[i]) & 0x3ff));
	writel(0, lp->etile_regs + 0x4004);
}

static void fill_oif_tx_stats(struct adi_msp_private *lp)
{
	u64 *data = (u64 *)&lp->stats.oif_tx;
	int num = ARRAY_SIZE(adi_oif_tx_stats_offsets);
	const int *offsets = adi_oif_tx_stats_offsets;
	int i;

	for (i = 0; i < num; i++)
		data[i] = readl((void __iomem *)(lp->oif_tx_regs) + offsets[i]);
}

static void fill_oif_rx_stats(struct adi_msp_private *lp)
{
	u64 *data = (u64 *)&lp->stats.oif_rx;
	int num = ARRAY_SIZE(adi_oif_rx_stats_offsets);
	const int *offsets = adi_oif_rx_stats_offsets;
	int i;

	for (i = 0; i < num; i++)
		data[i] = readl((void __iomem *)(lp->oif_rx_regs) + offsets[i]);
}

#ifndef CONFIG_ADI_MSPRX_ASYNC_FIFO
static void fill_async_fifo_rx_stats(struct adi_msp_private *lp)
{
	u64 *data = (u64 *)&lp->stats.async_fifo_rx;
	int num = ARRAY_SIZE(adi_async_fifo_rx_stats_offsets);
	const int *offsets = adi_async_fifo_rx_stats_offsets;
	int i;

	for (i = 0; i < num; i++) {
		data[i] = readl(lp->async_fifo_rx_regs + offsets[i]);
		if (offsets[i] == 0x10)
			data[i] &= 0x7fffffff;
	}
}
#endif

static void fill_msp_rx_stats(struct adi_msp_private *lp)
{
	u64 *data = (u64 *)&lp->stats.msp_rx;
	int num = ARRAY_SIZE(adi_msp_rx_stats_offsets);
	const int *offsets = adi_msp_rx_stats_offsets;
	int i;

	for (i = 0; i < num; i++)
		data[i] = readl((void __iomem *)(lp->rx_regs) + offsets[i]);
}

static void adi_msp_get_ethtool_stats(struct net_device *dev,
				      struct ethtool_stats *stats, u64 *data)
{
	struct adi_msp_private *lp = netdev_priv(dev);

	fill_intel_etile_tx_stats(lp);
	fill_intel_etile_rx_stats(lp);
	fill_bridge_mac_oif_stats(lp);
	fill_oif_tx_stats(lp);
	fill_oif_rx_stats(lp);
#ifndef CONFIG_ADI_MSPRX_ASYNC_FIFO
	fill_async_fifo_rx_stats(lp);
#endif
	fill_msp_rx_stats(lp);

	memcpy(data, &lp->stats, sizeof(lp->stats));
}

static int adi_msp_get_ts_info(struct net_device *dev,
			       struct ethtool_ts_info *info)
{
	struct adi_msp_private *lp = netdev_priv(dev);

	if (lp->has_ptp) {
		info->phc_index = ptp_clock_index(lp->ptp_clk);

		info->so_timestamping = SOF_TIMESTAMPING_TX_HARDWARE |
					SOF_TIMESTAMPING_RX_HARDWARE |
					SOF_TIMESTAMPING_RAW_HARDWARE;

		info->tx_types = (1 << HWTSTAMP_TX_OFF) |
				 (1 << HWTSTAMP_TX_ON);

		info->rx_filters = (1 << HWTSTAMP_FILTER_NONE) |
				   (1 << HWTSTAMP_FILTER_ALL);

		return 0;
	} else {
		return ethtool_op_get_ts_info(dev, info);
	}
}

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= adi_msp_get_drvinfo,
	.get_ethtool_stats	= adi_msp_get_ethtool_stats,
	.get_strings		= adi_msp_get_strings,
	.get_sset_count		= adi_msp_get_sset_count,
	.get_ts_info		= adi_msp_get_ts_info,
};

static int adi_msp_alloc_ring(struct net_device *dev)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	struct sk_buff *skb;
	dma_addr_t as; // addrstart
	int i;

	/* Initialize the transmit descriptors */
	for (i = 0; i < ADI_MSP_NUM_TDS; i++) {
		lp->td_ring[i].dscptr_nxt = 0;
		lp->td_ring[i].addrstart = 0;
		lp->td_ring[i].cfg = 0;
		lp->td_ring[i].xcnt = 0;
		lp->td_ring[i].xmod = 0;
	}
	lp->tx_next_done = 0;
	lp->tx_chain_head = 0;
	lp->tx_chain_tail = 0;
	atomic_set(&lp->tx_count, 0);
	lp->tx_chain_status = EMPTY;

	/* Initialize the receive descriptors */
	for (i = 0; i < ADI_MSP_NUM_RDS; i++) {
		skb = napi_alloc_skb(&lp->rx_napi, RX_WU_BUF_SIZE);
		if (!skb)
			return -ENOMEM;
		if (((unsigned long long)skb->data) & 0x7)
			pr_info("not aligned to 8\n");
		/* Initialize work unit header byte to 0 */
		skb->data[0] = 0;
		lp->rx_skb[i] = skb;

		lp->rd_ring[i].cfg = RX_DMA_CFG_COMMON;
#if 1
		lp->rd_ring[i].cfg |= (i == ADI_MSP_NUM_RDS - 1) ?
			DMA_CFG_FLOW_STOP : DMA_CFG_FLOW_DSCL;
#else
		/*
		lp->rd_ring[i].cfg |= (i % 2 == 1) ?
			DMA_CFG_FLOW_STOP : DMA_CFG_FLOW_DSCL;
		*/
		lp->rd_ring[i].cfg |= DMA_CFG_FLOW_STOP;
#endif
		lp->rd_ring[i].xcnt = RX_WU_LEN / RX_XMOD;
		lp->rd_ring[i].xmod = RX_XMOD;

		as = dma_map_single(lp->dmadev, skb->data, RX_WU_LEN,
				    DMA_FROM_DEVICE);
		if (dma_mapping_error(lp->dmadev, as))
			return -ENOMEM;
		lp->rd_ring[i].addrstart = as;
		lp->rx_skb_dma[i] = as;

		lp->rd_ring[i].dscptr_nxt = adi_msp_rx_dma(lp, i + 1);
	}

	lp->rx_next_done  = 0;

	/* Initialize the transmit status descriptors */

	for (i = 0; i < ADI_MSP_NUM_SDS; i++) {
		lp->sd_ring[i].cfg = STATUS_DMA_CFG_COMMON;
		lp->sd_ring[i].cfg |= (i == ADI_MSP_NUM_SDS - 1) ?
			DMA_CFG_FLOW_STOP : DMA_CFG_FLOW_DSCL;
		lp->sd_ring[i].xcnt = STATUS_WU_LEN / STATUS_XMOD;
		lp->sd_ring[i].xmod = STATUS_XMOD;
		lp->sd_ring[i].addrstart = adi_msp_status_wu_dma(lp, i);
		lp->sd_ring[i].dscptr_nxt = adi_msp_status_dma(lp, i + 1);
	}

	return 0;
}

static void adi_msp_free_ring(struct net_device *dev)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	int i;

	for (i = 0; i < ADI_MSP_NUM_RDS; i++) {
		lp->rd_ring[i].cfg = 0;
		lp->rd_ring[i].xcnt = 0;
		if (lp->rx_skb[i]) {
			dma_unmap_single(lp->dmadev, lp->rx_skb_dma[i],
					 RX_WU_LEN, DMA_FROM_DEVICE);
			dev_kfree_skb_any(lp->rx_skb[i]);
			lp->rx_skb[i] = NULL;
		}
	}

	for (i = 0; i < ADI_MSP_NUM_TDS; i++) {
		lp->td_ring[i].cfg = 0;
		lp->td_ring[i].xcnt = 0;

		if (lp->tx_skb[i]) {
#ifdef CONFIG_ADI_MSP_TX_PADDING
			u32 frame_length = max(lp->tx_skb[i]->len, TX_MIN_FRAME_SIZE);
#else
			u32 frame_length = lp->tx_skb[i]->len;
#endif

#ifdef CONFIG_ADI_MSP_WA_TX_WU_SIZE_MULTIPLE_OF_8
			u32 wu_length = round_up(frame_length + TX_WU_HEADER_LEN, 8);
#else
			u32 wu_length = frame_length + TX_WU_HEADER_LEN;
#endif

			dma_unmap_single(lp->dmadev, lp->tx_skb_dma[i],
					 wu_length, DMA_TO_DEVICE);
			dev_kfree_skb_any(lp->tx_skb[i]);
			lp->tx_skb[i] = NULL;
		}
	}

	for (i = 0; i < ADI_MSP_NUM_SDS; i++) {
		lp->sd_ring[i].cfg = 0;
		lp->sd_ring[i].xcnt = 0;
	}
}

static void adi_msp_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
	MSP_ERR("%s: Entering %s ...\n", dev->name, __func__);
#if 0
	struct adi_msp_private *lp = netdev_priv(dev);
	struct net_device *dev = lp->dev;
	u32 tmp;

	/*
	 * Disable interrupts
	 */
	disable_irq(lp->rx_dmadone_irq);
	disable_irq(lp->rx_dde_error_irq);
	disable_irq(lp->tx_dmadone_irq);
	disable_irq(lp->tx_dde_error_irq);

	tmp = readl(&lp->tx_dma_regs->cfg);
	tmp &= ~DMA_CFG_INT_MASK;
	writel(tmp, &lp->tx_dma_regs->cfg);

	tmp = readl(&lp->rx_dma_regs->cfg);
	tmp &= ~DMA_CFG_INT_MASK;
	writel(tmp, &lp->rx_dma_regs->cfg);

	napi_disable(&lp->tx_napi);

	adi_msp_free_ring(dev);

	if (adi_msp_init(dev) < 0) {
		printk(KERN_ERR "%s: cannot restart device\n", dev->name);
		return;
	}

	enable_irq(lp->rx_dmadone_irq);
	enable_irq(lp->rx_dde_error_irq);
	enable_irq(lp->tx_dmadone_irq);
	enable_irq(lp->tx_dde_error_irq);
#endif
	MSP_DBG("%s: ... Leaving %s\n", dev->name, __func__);
}

static int adi_msp_open(struct net_device *dev)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	u32 frame_size, dma_cfg;
	int i, ret;

	MSP_DBG("%s: Entering %s ...\n", dev->name, __func__);

	/* Make sure MSP Tx and Rx interfaces are disabled */
	writel(0, &lp->tx_regs->stat_ctrl);
	writel(0, &lp->rx_regs->stat_ctrl);

	writel(TX_TIMEOUT_VALUE, &lp->tx_regs->timeout_value);

	/* Set MIN/MAX frame size */
	frame_size = TX_MIN_FRAME_SIZE | TX_MAX_FRAME_SIZE << 16;
	writel(frame_size, &lp->tx_regs->frame_size);
	frame_size = RX_MIN_FRAME_SIZE | RX_MAX_FRAME_SIZE << 16;
	writel(frame_size, &lp->rx_regs->frame_size);

	/* Enable all MSP Tx/Rx interrupts */
	writel(MSP_TX_INT_ALL, &lp->tx_regs->intr_en);
	writel(MSP_RX_INT_ALL, &lp->rx_regs->intr_en);

	/* Make sure DMAs are disabled */
	writel(0, &lp->tx_dma_regs->cfg);
	writel(0, &lp->status_dma_regs->cfg);
	writel(0, &lp->rx_dma_regs->cfg);

	/* Allocate rings */
	if (adi_msp_alloc_ring(dev)) {
		MSP_ERR("%s: descriptor allocation failed\n", dev->name);
		adi_msp_free_ring(dev);
		return -ENOMEM;
	}

	for (i = 0; i < PREV_RX_SKB_NUM; i++)
		lp->prev_rx_skb[i] = NULL;
	lp->prev_rx_skb_count = 0;

	lp->next_nonptp_frame_tag = ADI_MSP_MIN_NONPTP_FRAME_TAG;
	lp->last_nonptp_frame_tag = ADI_MSP_MAX_NONPTP_FRAME_TAG;
	atomic_set(&lp->available_nonptp_frame_tag_count,
		   lp->last_nonptp_frame_tag - lp->next_nonptp_frame_tag + 1);

	lp->next_ptp_frame_tag = ADI_MSP_MIN_PTP_FRAME_TAG;
	lp->last_ptp_frame_tag = ADI_MSP_MAX_PTP_FRAME_TAG;
	atomic_set(&lp->available_ptp_frame_tag_count,
		   lp->last_ptp_frame_tag - lp->next_ptp_frame_tag + 1);

	ret = request_irq(lp->rx_dmadone_irq, adi_msp_rx_dma_done_interrupt,
			  0, "ADI MSP Rx DMA done", dev);
	if (ret < 0) {
		MSP_ERR("%s: unable to get MSP Rx DMA done IRQ %d\n",
			dev->name, lp->rx_dmadone_irq);
		goto err_release;
	}

	ret = request_irq(lp->rx_dde_error_irq, adi_msp_rx_dma_error_interrupt,
			  0, "ADI MSP Rx DMA error", dev);
	if (ret < 0) {
		MSP_ERR("%s: unable to get MSP Rx DMA error IRQ %d\n",
			dev->name, lp->rx_dde_error_irq);
		goto err_free_irq_1;
	}

	ret = request_irq(lp->tx_dde_error_irq, adi_msp_tx_dma_error_interrupt,
			  0, "ADI MSP Tx DMA error", dev);
	if (ret < 0) {
		MSP_ERR("%s: unable to get Tx DMA error IRQ %d\n",
			dev->name, lp->tx_dde_error_irq);
		goto err_free_irq_3;
	}

	ret = request_irq(lp->status_dmadone_irq, adi_msp_status_dma_done_interrupt,
			  0, "ADI MSP Tx status DMA done", dev);
	if (ret < 0) {
		MSP_ERR("%s: unable to get Tx status DMA done IRQ %d\n",
			dev->name, lp->status_dmadone_irq);
		goto err_free_irq_4;
	}

	ret = request_irq(lp->status_dde_error_irq, adi_msp_status_dma_error_interrupt,
			  0, "ADI MSP Tx status DMA error", dev);
	if (ret < 0) {
		MSP_ERR("%s: unable to get Tx status DMA error IRQ %d\n",
			dev->name, lp->status_dde_error_irq);
		goto err_free_irq_5;
	}

	/* Start Tx status DMA */
	dma_cfg = STATUS_DMA_CFG_COMMON | DMA_CFG_FLOW_DSCL;
	writel(adi_msp_status_dma(lp, 0), &lp->status_dma_regs->dscptr_nxt);
	writel(dma_cfg, &lp->status_dma_regs->cfg);

	/* Start MSP Tx interface */
	writel(MSP_EN, &lp->tx_regs->stat_ctrl);

	/* DDE_tester    0xfe: MSP Rx    0xff: on chip memory */
	//writel(0xfe, &lp->dde_tester_regs->ctrl);

	/* Start Rx DMA */
	dma_cfg = RX_DMA_CFG_COMMON | DMA_CFG_FLOW_DSCL;
	writel(adi_msp_rx_dma(lp, 0), &lp->rx_dma_regs->dscptr_nxt);
	writel(dma_cfg, &lp->rx_dma_regs->cfg);

	/* Start MSP Rx interface */
	writel(MSP_EN, &lp->rx_regs->stat_ctrl);

	napi_enable(&lp->rx_napi);
	napi_enable(&lp->status_napi);

	netif_start_queue(dev);

	MSP_DBG("%s: ... Leaving %s\n", dev->name, __func__);
out:
	return ret;

err_free_irq_5:
	free_irq(lp->status_dmadone_irq, dev);
err_free_irq_4:
	free_irq(lp->tx_dde_error_irq, dev);
err_free_irq_3:
	free_irq(lp->rx_dde_error_irq, dev);
err_free_irq_1:
	free_irq(lp->rx_dmadone_irq, dev);
err_release:
	adi_msp_free_ring(dev);
	goto out;
}

#if 0
static void adi_msp_reset(struct net_device *dev)
{
	u32 rst_ctrl;

	/* FIXME  For now we only handle Tx0 and Rx0 */
	rst_ctrl = readl(MSP_RST_CTRL);
	rst_ctrl &= ~(MSP_RST_CTRL_TX0 | MSP_RST_CTRL_RX0);
	writel(rst_ctrl, MSP_RST_CTRL);
	rst_ctrl |= MSP_RST_CTRL_TX0 | MSP_RST_CTRL_RX0;
	writel(rst_ctrl, MSP_RST_CTRL);
	/* FIXME  Is there a bit telling us that reset is done? */
}
#endif

static int adi_msp_close(struct net_device *dev)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	int i;

	MSP_DBG("%s: Entering %s ...\n", dev->name, __func__);

	/* Make sure MSP Tx and Rx interfaces are disabled */
	writel(0, &lp->tx_regs->stat_ctrl);
	writel(0, &lp->rx_regs->stat_ctrl);

	/* Disable interrupts */
	disable_irq(lp->rx_dmadone_irq);
	disable_irq(lp->rx_dde_error_irq);
	disable_irq(lp->tx_dde_error_irq);
	disable_irq(lp->status_dmadone_irq);
	disable_irq(lp->status_dde_error_irq);

	/* Disable DMAs */
	writel(0, &lp->tx_dma_regs->cfg);
	writel(0, &lp->status_dma_regs->cfg);
	writel(0, &lp->rx_dma_regs->cfg);

	napi_disable(&lp->rx_napi);
	napi_disable(&lp->status_napi);

	adi_msp_free_ring(dev);

	for (i = 0; i < PREV_RX_SKB_NUM; i++)
		if (lp->prev_rx_skb[i])
			dev_kfree_skb_any(lp->prev_rx_skb[i]);
	lp->prev_rx_skb_count = 0;

	free_irq(lp->rx_dmadone_irq, dev);
	free_irq(lp->rx_dde_error_irq, dev);
	free_irq(lp->tx_dde_error_irq, dev);
	free_irq(lp->status_dmadone_irq, dev);
	free_irq(lp->status_dde_error_irq, dev);

	MSP_DBG("%s: ... Leaving %s\n", dev->name, __func__);

	return 0;
}

static void adi_msp_get_stats64(struct net_device *dev,
				struct rtnl_link_stats64 *stats)
{
	struct adi_msp_private *lp = netdev_priv(dev);

	stats->rx_packets	= lp->stats.nl.rx_packets;
	stats->tx_packets	= lp->stats.nl.tx_packets;
	stats->rx_bytes		= lp->stats.nl.rx_bytes;
	stats->tx_bytes		= lp->stats.nl.tx_bytes;
	stats->rx_errors	= lp->stats.nl.rx_errors;
	stats->tx_errors	= lp->stats.nl.tx_errors;
	stats->rx_dropped	= lp->stats.nl.rx_dropped;
	stats->tx_dropped	= lp->stats.nl.tx_dropped;
}

static int adi_msp_hwtstamp_set(struct net_device *dev, struct ifreq *ifr)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	struct hwtstamp_config config;

	if (copy_from_user(&config, ifr->ifr_data, sizeof(config)))
		return -EFAULT;

	if (config.flags)
		return -EINVAL;

	switch (config.tx_type) {
	case HWTSTAMP_TX_OFF:
		lp->hwtstamp_tx_en = false;
		break;
	case HWTSTAMP_TX_ON:
		lp->hwtstamp_tx_en = true;
		break;
	default:
		return -ERANGE;
	}

	switch (config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		lp->hwtstamp_rx_en = false;
		break;
	default:
		lp->hwtstamp_rx_en = true;
		config.rx_filter = HWTSTAMP_FILTER_ALL;
		break;
	}

	if (copy_to_user(ifr->ifr_data, &config, sizeof(config)))
		return -EFAULT;
	else
		return 0;
}

static int adi_msp_hwtstamp_get(struct net_device *dev, struct ifreq *ifr)
{
	struct adi_msp_private *lp = netdev_priv(dev);
	struct hwtstamp_config config;

	config.flags = 0;
	config.tx_type = lp->hwtstamp_tx_en ? HWTSTAMP_TX_ON : HWTSTAMP_TX_OFF;
	config.rx_filter = lp->hwtstamp_rx_en ?
				HWTSTAMP_FILTER_ALL : HWTSTAMP_FILTER_NONE;

	if (copy_to_user(ifr->ifr_data, &config, sizeof(config)))
		return -EFAULT;
	else
		return 0;
}

static int adi_msp_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct adi_msp_private *lp = netdev_priv(dev);

	if (!netif_running(dev))
		return -EINVAL;

	if (!lp->has_ptp)
		return -EOPNOTSUPP;

	switch (cmd) {
	case SIOCSHWTSTAMP:
		return adi_msp_hwtstamp_set(dev, ifr);
	case SIOCGHWTSTAMP:
		return adi_msp_hwtstamp_get(dev, ifr);
	default:
		return -EOPNOTSUPP;
	}
}

static const struct net_device_ops adi_msp_netdev_ops = {
	.ndo_open		= adi_msp_open,
	.ndo_stop		= adi_msp_close,
	.ndo_start_xmit		= adi_msp_send_packet,
	.ndo_tx_timeout		= adi_msp_tx_timeout,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_get_stats64	= adi_msp_get_stats64,
	.ndo_do_ioctl		= adi_msp_ioctl,
};

#define TX_TIMEOUT	(6000 * HZ / 1000)

static int adi_msp_probe(struct platform_device *pdev)
{
	struct adi_msp_private *lp;
	struct net_device *dev;
	const char *etile_name, *oif_tx_name, *oif_rx_name;
	bool has_ptp;
	struct device_node *ptp_clk_node;
	struct platform_device *ptp_clk_dev;
	struct adi_phc *phc;
	void __iomem *p;
	u32 eth;
	int ret;

	MSP_DBG("Entering %s ...\n", __func__);

	/* First we check if PTP PHC has been initialized and registered */
	ptp_clk_node = of_parse_phandle(pdev->dev.of_node, "adi,ptp-clk", 0);
	if (ptp_clk_node) {
		ptp_clk_dev = of_find_device_by_node(ptp_clk_node);
		of_node_put(ptp_clk_node);
		if (!ptp_clk_dev) {
			MSP_DBG("ADI PTP PHC device not found\n");
			has_ptp = false;
#ifdef MODULE
			goto ptp_check_done;
#else
			return -EPROBE_DEFER;
#endif
		}
		phc = platform_get_drvdata(ptp_clk_dev);
		if (!phc) {
			MSP_DBG("ADI PTP PHC device not initialized correctly\n");
			has_ptp = false;
#ifdef MODULE
			goto ptp_check_done;
#else
			return -EPROBE_DEFER;
#endif
		}
		if (!phc->ptp_clk) {
			MSP_ERR("ADI PTP PHC device not registered correctly\n");
			return -EINVAL;
		}
		MSP_INFO("ADI PTP PHC device has been registered\n");
		has_ptp = true;
	} else {
		MSP_DBG("no device tree node for ADI PTP PHC device\n");
		has_ptp = false;
	}

ptp_check_done:
	dev = devm_alloc_etherdev(&pdev->dev, sizeof(struct adi_msp_private));
	if (!dev)
		return -ENOMEM;

	SET_NETDEV_DEV(dev, &pdev->dev);
	lp = netdev_priv(dev);

	lp->has_ptp = has_ptp;
	lp->hwtstamp_tx_en = has_ptp;
	lp->hwtstamp_rx_en = has_ptp;
	lp->ptp_clk = lp->has_ptp ? phc->ptp_clk : NULL;

	ret = platform_get_irq_byname(pdev, "rx_dmadone_irq");
	if (ret < 0)
		return ret;
	lp->rx_dmadone_irq = ret;
	MSP_DBG("%s: rx_dmadone_irq = %d\n", dev->name, ret);

	ret = platform_get_irq_byname(pdev, "rx_dde_error_irq");
	if (ret < 0)
		return ret;
	lp->rx_dde_error_irq = ret;
	MSP_DBG("%s: rx_dde_error_irq = %d\n", dev->name, ret);

	ret = platform_get_irq_byname(pdev, "tx_dde_error_irq");
	if (ret < 0)
		return ret;
	lp->tx_dde_error_irq = ret;
	MSP_DBG("%s: tx_dde_error_irq = %d\n", dev->name, ret);

	ret = platform_get_irq_byname(pdev, "status_dmadone_irq");
	if (ret < 0)
		return ret;
	lp->status_dmadone_irq = ret;
	MSP_DBG("%s: status_dmadone_irq = %d\n", dev->name, ret);

	ret = platform_get_irq_byname(pdev, "status_dde_error_irq");
	if (ret < 0)
		return ret;
	lp->status_dde_error_irq = ret;
	MSP_DBG("%s: status_dde_error_irq = %d\n", dev->name, ret);

	ret = of_property_read_u32(pdev->dev.of_node, "eth", &eth);
	if (ret < 0)
		return ret;
	if (eth != 0 && eth != 1) {
		MSP_ERR("%s: bad eth value %u\n", dev->name, eth);
		return -EINVAL;
	}

	etile_name = (eth == 0) ? "etile0" : "etile1";
	p = devm_platform_ioremap_resource_byname(pdev, etile_name);
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap %s registers\n",
			dev->name, etile_name);
		return PTR_ERR(p);
	}
	lp->etile_regs = p;

	oif_tx_name = (eth == 0) ? "oif0_tx" : "oif1_tx";
	p = devm_platform_ioremap_resource_byname(pdev, oif_tx_name);
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap OIF Tx registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->oif_tx_regs = p;

	oif_rx_name = (eth == 0) ? "oif0_rx" : "oif1_rx";
	p = devm_platform_ioremap_resource_byname(pdev, oif_rx_name);
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap OIF Rx registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->oif_rx_regs = p;

#ifndef CONFIG_ADI_MSPRX_ASYNC_FIFO
	p = devm_platform_ioremap_resource_byname(pdev, "async_fifo_rx");
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap Async FIFO Rx registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->async_fifo_rx_regs = p;
#endif

	p = devm_platform_ioremap_resource_byname(pdev, "axi_palau_gpio_msp_ctrl");
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap axi_palau_gpio MSP control registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->axi_palau_gpio_msp_ctrl = p;

	/* MAC address should have been set. But it is not. So we set it. */

	/*
	u32 smac_lo, smac_hi, prom_mode;
	smac_lo = readl(&(lp->oif_rx_regs->cfg_fr_mux_smac_0));
	smac_hi = readl(&(lp->oif_rx_regs->cfg_fr_mux_smac_1));
	prom_mode = (smac_hi >> 16) & 0x1;
	smac_hi &= 0xffff;
	pr_info("mac_addr = %04x %08x prom_mode = %d\n", smac_hi, smac_lo, prom_mode);
	*/
	/*
	{
	u32 prom_mode = 1;
	unsigned char mac_addr[6] = {0x10, 0x22, 0x33, 0x44, 0x55, 0x66};
	u32 smac_0 = mac_addr[0] | (mac_addr[1] << 8) | (mac_addr[2] << 16) | (mac_addr[3] << 24);
	u32 smac_1 = mac_addr[4] | (mac_addr[5] << 8) | prom_mode << 16;
	writel(smac_0, &(lp->oif_rx_regs->cfg_fr_mux_smac_0));
	writel(smac_1, &(lp->oif_rx_regs->cfg_fr_mux_smac_1));
	memcpy(dev->dev_addr, mac_addr, ETH_ALEN);
	}
	*/
	{
	unsigned char mac_addr[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
	memcpy(dev->dev_addr, mac_addr, ETH_ALEN);
	}

	p = devm_platform_ioremap_resource_byname(pdev, "rx");
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap MSP Rx registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->rx_regs = p;

	p = devm_platform_ioremap_resource_byname(pdev, "tx");
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap MSP Tx registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->tx_regs = p;

	p = devm_platform_ioremap_resource_byname(pdev, "rx_dma");
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap Rx DMA registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->rx_dma_regs = p;

	p = devm_platform_ioremap_resource_byname(pdev, "tx_dma");
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap Tx DMA registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->tx_dma_regs = p;

	p = devm_platform_ioremap_resource_byname(pdev, "status_dma");
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap Tx Status DMA registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->status_dma_regs = p;

	p = devm_platform_ioremap_resource_byname(pdev, "dde_tester");
	if (IS_ERR(p)) {
		MSP_ERR("%s: cannot remap DDE_tester registers\n", dev->name);
		return PTR_ERR(p);
	}
	lp->dde_tester_regs = p;

	lp->td_ring = dmam_alloc_coherent(&pdev->dev, ADI_MSP_TD_RING_SIZE,
					  &lp->td_dma, GFP_KERNEL);
	if (!lp->td_ring) {
		MSP_ERR("%s: cannot alloc Tx ring\n", dev->name);
		return -ENOMEM;
	}

	lp->rd_ring = dmam_alloc_coherent(&pdev->dev, ADI_MSP_RD_RING_SIZE,
					  &lp->rd_dma, GFP_KERNEL);
	if (!lp->rd_ring) {
		MSP_ERR("%s: cannot alloc Rx ring\n", dev->name);
		return -ENOMEM;
	}

	lp->sd_ring = dmam_alloc_coherent(&pdev->dev, ADI_MSP_SD_RING_SIZE,
					  &lp->sd_dma, GFP_KERNEL);
	if (!lp->sd_ring) {
		MSP_ERR("%s: cannot alloc Status ring\n", dev->name);
		return -ENOMEM;
	}

	lp->status_wu = dmam_alloc_coherent(&pdev->dev,
					    ADI_MSP_NUM_SDS * STATUS_WU_BUF_SIZE,
					    &lp->status_wu_dma, GFP_KERNEL);
	if (!lp->status_wu) {
		MSP_ERR("%s: cannot alloc buffer for Tx status work units\n",
			dev->name);
		return -ENOMEM;
	}
	memset(lp->status_wu, 0, ADI_MSP_NUM_SDS * STATUS_WU_BUF_SIZE);

	spin_lock_init(&lp->lock);

	/* Each packet needs to have a Tx work unit header */
	dev->needed_headroom = TX_WU_HEADER_LEN;

	/* just use the rx dma done irq */
	dev->irq = lp->rx_dmadone_irq;
	lp->dev = dev;
	lp->dmadev = &pdev->dev;

	dev->netdev_ops = &adi_msp_netdev_ops;
	dev->ethtool_ops = &netdev_ethtool_ops;
	dev->watchdog_timeo = TX_TIMEOUT;

	netif_napi_add(dev, &lp->rx_napi, adi_msp_rx_poll, NAPI_POLL_WEIGHT);
	netif_napi_add(dev, &lp->status_napi, adi_msp_status_poll,
		       NAPI_POLL_WEIGHT);

	platform_set_drvdata(pdev, dev);

	ret = register_netdev(dev);
	if (ret < 0) {
		MSP_ERR("%s: cannot register net device: %d\n", dev->name, ret);
		return ret;
	}

	MSP_INFO("%s: " DRV_NAME "-" DRV_VERSION "\n", dev->name);
	return ret;
}

static int adi_msp_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);

	unregister_netdev(dev);

	return 0;
}

static const struct of_device_id adi_msp_match[] = {
	{
		.compatible = "adi,msp",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, adi_msp_match);

static struct platform_driver adi_msp_driver = {
	.driver = {
		.name = "adi_msp",
		.of_match_table = of_match_ptr(adi_msp_match),
	},
	.probe = adi_msp_probe,
	.remove = adi_msp_remove,
};

module_platform_driver(adi_msp_driver);

MODULE_AUTHOR("Jie Zhang <jie.zhang@analog.com>");
MODULE_DESCRIPTION("Analog Devices MS Plane Ethernet driver");
MODULE_LICENSE("GPL");
