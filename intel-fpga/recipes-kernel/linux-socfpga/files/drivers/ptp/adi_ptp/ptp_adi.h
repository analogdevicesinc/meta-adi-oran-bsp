// SPDX-License-Identifier: GPL-2.0+
/*
 * PTP hardware clock driver for the ADI low-phy soc of timing and synchronization devices.
 *
 * Copyright (C) 2022 Analog Device, Inc.
 */
#ifndef __PTP_ADI_H
#define __PTP_ADI_H

#include <linux/clk.h>
#include <linux/ktime.h>
#include "ptp_adi_clk.h"

#define PHC_HW_TOD_CDC_DOMAIN_CNT       (8u)

#define ADI_HW_TOD_DISABLE              (0)
#define ADI_HW_TOD_ENABLE               (1)


#define ADI_TOD_REG_MASK_ALL                    (0xFFFFFFFFu)
#define ADI_TOD_REG_SHIFT_NONE                  (0)

/* IP ID */
#define ADI_TOD_IP_ID                           (0x00u)

#define ADI_TOD_IP_ID_MASK                      (0xFFFFFFFFu)
#define ADI_TOD_IP_ID_SHIFT                     (0)


/* IP version register */
#define ADI_TOD_IP_VER                          (0x04u)

#define ADI_TOD_IP_VER_MINOR_MASK               (0xFFFFu)
#define ADI_TOD_IP_VER_MINOR_SHIFT              (0)

#define ADI_TOD_IP_VER_MAJOR_MASK               (0xFFFF0000u)
#define ADI_TOD_IP_VER_MAJOR_SHIFT               (16)

/* Config of the ToD counter clock period */
#define ADI_TOD_CFG_INCR                        (0x08U)

#define ADI_TOD_CFG_INCR_FRAC_NS_PER_CLK_MASK   (0xFFFFU)
#define ADI_TOD_CFG_INCR_FRAC_NS_PER_CLK_SHIFT  (0)

#define ADI_TOD_CFG_INCR_NS_PER_CLK_MASK        (0xF0000U)
#define ADI_TOD_CFG_INCR_NS_PER_CLK_SHIFT       (16)

#define ADI_TOD_CFG_INCR_CNT_CTRL_MASK          (0x3F00000U)
#define ADI_TOD_CFG_INCR_CNT_CTRL_SHIFT         (20)

#define ADI_TOD_CFG_INCR_CFG_TOD_CNT_EN_MASK    (0x10000000U)
#define ADI_TOD_CFG_INCR_CFG_TOD_CNT_EN_SHIFT   (28)

/* ToD counter operations. */
#define ADI_TOD_CFG_TOD_OP                      (0x20u)

#define ADI_TOD_CFG_TOD_OP_WR_TOD_MASK          (0x01u)
#define ADI_TOD_CFG_TOD_OP_WR_TOD_SHIFT         (0)

#define ADI_TOD_CFG_TOD_OP_RD_TOD_MASK          (0x10u)
#define ADI_TOD_CFG_TOD_OP_RD_TOD_SHIFT         (4)

#define ADI_TOD_CFG_TOD_OP_WR_TOD_PPS_MASK      (0x100u)
#define ADI_TOD_CFG_TOD_OP_WR_TOD_PPS_SHIFT     (8)

#define ADI_TOD_CFG_TOD_OP_RD_TOD_PPS_MASK      (0x1000u)
#define ADI_TOD_CFG_TOD_OP_RD_TOD_PPS_SHIFT     (12)

/* ToD counter write value, bits [31:0] */
#define ADI_TOD_CFG_TV_NSEC                     (0x24u)

#define ADI_TOD_CFG_TV_NSEC_FRAC_NSEC_MASK      (0xFFFFu)
#define ADI_TOD_CFG_TV_NSEC_FRAC_NSEC_SHIFT     (0)

#define ADI_TOD_CFG_TV_NSEC_NSEC_MASK      (0xFFFF0000u)
#define ADI_TOD_CFG_TV_NSEC_NSEC_SHIFT     (16)


/* ToD counter write value, bits [63:32] */
#define ADI_TOD_CFG_TV_SEC_0                    (0x28u)

#define ADI_TOD_CFG_TV_SEC_0_NSEC_MASK          (0xFFFFu)
#define ADI_TOD_CFG_TV_SEC_0_NSEC_SHIFT         (0)

#define ADI_TOD_CFG_TV_SEC_0_SEC_MASK           (0xFFFF0000u)
#define ADI_TOD_CFG_TV_SEC_0_SEC_SHIFT          (16)


/* ToD counter write value, bits [95:64] */
#define ADI_TOD_CFG_TV_SEC_1                    (0x2Cu)

#define ADI_TOD_CFG_TV_SEC_1_SEC_MASK           (0xFFFFFFFFu)
#define ADI_TOD_CFG_TV_SEC_1_SEC_SHIFT          (0)

/* Golden counter value, at operation */
#define ADI_TOD_CFG_OP_GC_VAL_0                 (0x30U)

#define ADI_TOD_CFG_OP_GC_VAL_0_MASK            (0xFFFFFFFFU)
#define ADI_TOD_CFG_OP_GC_VAL_0_SHIFT           (0)

/* Golden counter value, at operation */
#define ADI_TOD_CFG_OP_GC_VAL_1                 (0x34U)

#define ADI_TOD_CFG_OP_GC_VAL_1_MASK            (0xFFFFFFFFU)
#define ADI_TOD_CFG_OP_GC_VAL_1_SHIFT           (0)

/* Golden counter operations */
#define ADI_TOD_CFG_OP_GC                       (0x38U)

#define ADI_TOD_CFG_OP_GC_RD_GC_MASK            (0x01U)
#define ADI_TOD_CFG_OP_GC_RD_GC_SHIFT           (0U)

/* 1 PPS Pulse start time */
#define ADI_TOD_CFG_PPSX_START                  (0x44U)

#define ADI_TOD_CFG_PPSX_START_PSTART_MASK     (0xFFFFFFFFU)
#define ADI_TOD_CFG_PPSX_START_PSTART_SHIFT    (0)

/* 1 PPS Pulse end time */
#define ADI_TOD_CFG_PPSX_STOP                   (0x48U)

#define ADI_TOD_CFG_PPSX_STOP_PSTOP_MASK        (0xFFFFFFFFU)
#define ADI_TOD_CFG_PPSX_STOP_PSTOP_SHIFT       (0)

/* ToD CDC domain outputs alignment setting */
#define ADI_TOD_CFG_CDC_DELAY                   (0x50U)

#define ADI_TOD_CFG_CDC_DELAY_CDC_MASK          (0xFFU)
#define ADI_TOD_CFG_CDC_DELAY_CDC_SHIFT         (0)


/* Golden count, bits [31:0] */
#define ADI_TOD_STAT_GC_0                       (0x70U)

#define ADI_TOD_STAT_GC_0_MASK                  (0xFFFFFFFFU)
#define ADI_TOD_STAT_GC_0_SHIFT                 (0)

/* Golden count, bits [47:32] */
#define ADI_TOD_STAT_GC_1                       (0x74U)

#define ADI_TOD_STAT_GC_1_MASK                  (0xFFFFFFFFU)
#define ADI_TOD_STAT_GC_1_SHIFT                 (0)

/* Readout of the ToD counter, bits [31:0] */
#define ADI_TOD_STAT_TV_NSEC                    (0x78U)

#define ADI_TOD_STAT_TV_FRAC_NSEC_MASK         (0xFFFFu)
#define ADI_TOD_STAT_TV_FRAC_NSEC_SHIFT        (0)

#define ADI_TOD_STAT_TV_NSEC_NSEC_MASK         (0xFFFF0000u)
#define ADI_TOD_STAT_TV_NSEC_NSEC_SHIFT        (16)

/* Readout of the ToD counter, bits [63:32] */
#define ADI_TOD_STAT_TV_SEC_0                   (0x7CU)

#define ADI_TOD_STAT_TV_SEC_0_NSEC_MASK         (0xFFFFu)
#define ADI_TOD_STAT_TV_SEC_0_NSEC_SHIFT        (0)

#define ADI_TOD_STAT_TV_SEC_0_SEC_MASK          (0xFFFF0000u)
#define ADI_TOD_STAT_TV_SEC_0_SEC_SHIFT         (16)

/* Readout of the ToD counter, bits [95:64] */
#define ADI_TOD_STAT_TV_SEC_1                   (0x80U)

#define ADI_TOD_STAT_TV_SEC_1_SEC_MASK          (0xFFFFFFFFu)
#define ADI_TOD_STAT_TV_SEC_1_SEC_SHIFT         (0)

/* Status of TOD_OP */
#define ADI_TOD_STAT_TOD_OP                     (0x90U)

#define ADI_TOD_STAT_TOD_OP_WR_TOD_MASK         (0x01u)
#define ADI_TOD_STAT_TOD_OP_WR_TOD_SHIFT        (0)

#define ADI_TOD_STAT_TOD_OP_RD_TOD_MASK         (0x10u)
#define ADI_TOD_STAT_TOD_OP_RD_TOD_SHIFT        (4)

#define ADI_TOD_STAT_TOD_OP_WR_TOD_PPS_MASK     (0x100u)
#define ADI_TOD_STAT_TOD_OP_WR_TOD_PPS_SHIFT    (8)

#define ADI_TOD_STAT_TOD_OP_RD_TOD_PPS_MASK     (0x1000u)
#define ADI_TOD_STAT_TOD_OP_RD_TOD_PPS_SHIFT    (12)

struct phc_hw_tod;

enum hw_tod_trig_mode {
	HW_TOD_TRIG_MODE_GC	= 0,    /* Tod triggered by the PPS */
	HW_TOD_TRIG_MODE_PPS	= 1,    /* Tod triggered by the Golden Counter */
	HW_TOD_TRIG_MODE_CNT,
};

enum hw_tod_lc_clk_freq {
	HW_TOD_LC_100_P_000_M = 0,
	HW_TOD_LC_122_P_880_M,
	HW_TOD_LC_125_P_000_M,
	HW_TOD_LC_156_P_250_M,
	HW_TOD_LC_245_P_760_M,
	HW_TOD_LC_250_P_000_M,
	HW_TOD_LC_312_P_500_M,
	HW_TOD_LC_322_P_265_M,
	HW_TOD_LC_390_P_625_M,
	HW_TOD_LC_491_P_520_M,
	HW_TOD_LC_500_P_000_M,
	HW_TOD_LC_983_P_040_M,
	HW_TOD_LC_CLK_FREQ_CNT,
};

struct _tod_reg {
	u16 regaddr;
	u32 regmask;
	u32 regshift;
};

enum _hw_tod_trig_op {
	HW_TOD_TRIG_OP_WR	= 0,                    /* Trigger reading the ToD */
	HW_TOD_TRIG_OP_RD	= 1,                    /* Trigger writing the ToD */
	HW_TOD_TRIG_OP_CNT
};

enum _hw_tod_trig_set_flag {
	HW_TOD_TRIG_SET_FLAG_CLEAR	= 0,
	HW_TOD_TRIG_SET_FLAG_TRIG	= 1,
	HW_TOD_TRIG_SET_FALG_CNT
};

enum _hw_tod_trig_op_flag {
	HW_TOD_TRIG_OP_FLAG_GOING	= 0,
	HW_TOD_TRIG_OP_FLAG_DONE	= 1,
	HW_TOD_TRIG_OP_FALG_CNT
};

struct tod_tstamp {
	u16 frac_nanoseconds;
	u32 nanoseconds;
	u64 seconds;
};

struct tod_trig_delay {
	u64 ns;
	u16 frac_ns;
};

struct tod_lc_clk_cfg {
	u32 freq_khz;                   /* frequency of the local clock */
	u32 ns_per_clk;                 /* nanosecond per clock */
	u32 frac_ns_per_clk;            /* fraction part of nanosecond per clock */
	u32 cnt_ctrl;                   /* correction control word */
};

struct tod_ppsx {
	u32 en;
	u32 delay_offset_ns;
	u32 pulse_width_ns;
};

struct tod_cdc {
	u32 domain_ref_freq[PHC_HW_TOD_CDC_DOMAIN_CNT];
	u32 delay_cnt;
};

struct phc_hw_tod {
	void __iomem *regs;
	void __iomem *axi_palau_gpio_pps_ctrl;
	u8 hw_tod_en;
	u8 trigger_mode;                        /* Trigger mode of Tod, 0 for GC, 1 for PPS */
	u32 lc_freq_khz;                        /* Clock frequency for the ToD counter block */
	u32 gc_clk_freq_khz;                    /* Clock frequency for the Golden counter block */
	u64 trig_delay_tick;
	struct tod_trig_delay trig_delay;
	u64 poll_delay_ns;
	u32 poll_timeout_us;
	/* Serialize access to hw_registers of the ToD module */
	spinlock_t reg_lock;
	struct tod_ppsx ppsx;
};

/* PTP Hardware Clock interface */
struct adi_phc {
	struct device *dev;
	struct ptp_clock *ptp_clk;
	struct clk *sys_clk;
	struct ptp_clock_info caps;
	struct phc_hw_tod hw_tod;
	struct phc_hw_clk hw_clk;
};


#endif
