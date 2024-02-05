// SPDX-License-Identifier: GPL-2.0+
/*
 * PTP hardware clock driver for the ADI low-phy soc of timing and synchronization devices.
 *
 * Copyright (C) 2022 Analog Device, Inc.
 */

#include <linux/printk.h>
#include <linux/module.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/timekeeping.h>
#include <linux/string.h>
#include <linux/io.h>                               // Prototype of memremap
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>             // Macros used to mark up functions e.g., __init __exit

#include "../ptp_private.h"
#include "ptp_adi.h"

MODULE_DESCRIPTION("Driver for ADI ptp hardware clock devices");
MODULE_AUTHOR("landau zhang <landau.zhang@analog.com>");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");


/* The base address is axi_palau_gpio module + 0x0144 */
#define PPS_CTRL_REG			0x0
#define TOD_PPS_IN_SEL_PPS_OUT		BIT(2)
#define TOD_PPS_IN_SEL_EXTERNAL		0

#define TOD_TIMEOUT_RATIO               (1 / 20)

#define TOD_1_SEC_IN_MILLI              1000
#define TOD_1_SEC_IN_MICRO              1000000
#define TOD_1_SEC_IN_NANO               1000000000
#define TOD_1_MILLI_SEC_IN_NANO         1000000
#define TOD_1_MICRO_SEC_IN_NANO         1000

#define TOD_BILLION_NUM                 1000000000
#define TOD_FRAC_NANO_NUM               0x10000

static struct _tod_reg _tod_reg_op_trig[HW_TOD_TRIG_OP_CNT][HW_TOD_TRIG_MODE_CNT] = {
	[HW_TOD_TRIG_OP_WR] =		 {
		[HW_TOD_TRIG_MODE_GC] =	 {
			.regaddr	= ADI_TOD_CFG_TOD_OP,
			.regmask	= ADI_TOD_CFG_TOD_OP_WR_TOD_MASK,
			.regshift	= ADI_TOD_CFG_TOD_OP_WR_TOD_SHIFT
		},
		[HW_TOD_TRIG_MODE_PPS] = {
			.regaddr	= ADI_TOD_CFG_TOD_OP,
			.regmask	= ADI_TOD_CFG_TOD_OP_WR_TOD_PPS_MASK,
			.regshift	= ADI_TOD_CFG_TOD_OP_WR_TOD_PPS_SHIFT
		}
	},
	[HW_TOD_TRIG_OP_RD] =		 {
		[HW_TOD_TRIG_MODE_GC] =	 {
			.regaddr	= ADI_TOD_CFG_TOD_OP,
			.regmask	= ADI_TOD_CFG_TOD_OP_RD_TOD_MASK,
			.regshift	= ADI_TOD_CFG_TOD_OP_RD_TOD_SHIFT
		},
		[HW_TOD_TRIG_MODE_PPS] = {
			.regaddr	= ADI_TOD_CFG_TOD_OP,
			.regmask	= ADI_TOD_CFG_TOD_OP_RD_TOD_PPS_MASK,
			.regshift	= ADI_TOD_CFG_TOD_OP_RD_TOD_PPS_SHIFT
		}
	}
};

static struct _tod_reg _tod_reg_op_poll[HW_TOD_TRIG_OP_CNT][HW_TOD_TRIG_MODE_CNT] = {
	[HW_TOD_TRIG_OP_WR] =		 {
		[HW_TOD_TRIG_MODE_GC] =	 {
			.regaddr	= ADI_TOD_STAT_TOD_OP,
			.regmask	= ADI_TOD_STAT_TOD_OP_WR_TOD_MASK,
			.regshift	= ADI_TOD_STAT_TOD_OP_WR_TOD_SHIFT
		},
		[HW_TOD_TRIG_MODE_PPS] = {
			.regaddr	= ADI_TOD_STAT_TOD_OP,
			.regmask	= ADI_TOD_STAT_TOD_OP_WR_TOD_PPS_MASK,
			.regshift	= ADI_TOD_STAT_TOD_OP_WR_TOD_PPS_SHIFT
		}
	},
	[HW_TOD_TRIG_OP_RD] =		 {
		[HW_TOD_TRIG_MODE_GC] =	 {
			.regaddr	= ADI_TOD_STAT_TOD_OP,
			.regmask	= ADI_TOD_STAT_TOD_OP_RD_TOD_MASK,
			.regshift	= ADI_TOD_STAT_TOD_OP_RD_TOD_SHIFT
		},
		[HW_TOD_TRIG_MODE_PPS] = {
			.regaddr	= ADI_TOD_STAT_TOD_OP,
			.regmask	= ADI_TOD_STAT_TOD_OP_RD_TOD_PPS_MASK,
			.regshift	= ADI_TOD_STAT_TOD_OP_RD_TOD_PPS_SHIFT
		}
	}
};

struct tod_lc_clk_cfg lc_clk_cfg[HW_TOD_LC_CLK_FREQ_CNT] = {
	[HW_TOD_LC_100_P_000_M] = { 100000, 10, 0x0000, 0x00 },
	[HW_TOD_LC_122_P_880_M] = { 122880, 8,	0x2355, 0x04 },
	[HW_TOD_LC_125_P_000_M] = { 125000, 8,	0x0000, 0x00 },
	[HW_TOD_LC_156_P_250_M] = { 156250, 6,	0x6666, 0x01 },
	[HW_TOD_LC_245_P_760_M] = { 245760, 4,	0x11AA, 0x02 },
	[HW_TOD_LC_250_P_000_M] = { 250000, 4,	0x0000, 0x00 },
	[HW_TOD_LC_312_P_500_M] = { 312500, 3,	0x3333, 0x08 },
	[HW_TOD_LC_322_P_265_M] = { 322265, 3,	0x1A60, 0x20 },
	[HW_TOD_LC_390_P_625_M] = { 390625, 2,	0x8F5C, 0x10 },
	[HW_TOD_LC_491_P_520_M] = { 491520, 2,	0x08D5, 0x04 },
	[HW_TOD_LC_500_P_000_M] = { 500000, 2,	0x0000, 0x00 },
	[HW_TOD_LC_983_P_040_M] = { 983040, 1,	0x046A, 0x02 }
};

static int _tod_reg_wr(struct phc_hw_tod *tod,
		       u8 regaddr,
		       u32 val,
		       u32 mask,
		       u32 shift)
{
	int err = 0;
	u32 wr_val = 0u;
	u32 rd_val = 0u;

	if (mask == ADI_TOD_REG_MASK_ALL) {
		wr_val = val;
	} else {
		rd_val = __raw_readl(tod->regs + regaddr);
		rd_val &= (~mask);
		wr_val = rd_val | ((val << shift) & mask);
	}
	__raw_writel(wr_val, tod->regs + regaddr);

	return err;
}

static int _tod_reg_rd(struct phc_hw_tod *tod,
		       u8 regaddr,
		       u32 *buf,
		       u32 mask,
		       u32 shift)
{
	int err = 0;

	u32 rd_val = 0u;

	rd_val = __raw_readl(tod->regs + regaddr);
	*buf = (rd_val & mask) >> shift;

	return err;
}

static int _tod_cfg_lc_clk(struct phc_hw_tod *tod)
{
	int err = -1;
	int lp;

	for (lp = 0; lp < HW_TOD_LC_CLK_FREQ_CNT; lp++) {
		if (tod->lc_freq_khz == lc_clk_cfg[lp].freq_khz) {
			_tod_reg_wr(tod, ADI_TOD_CFG_INCR, lc_clk_cfg[lp].frac_ns_per_clk, ADI_TOD_CFG_INCR_FRAC_NS_PER_CLK_MASK, ADI_TOD_CFG_INCR_FRAC_NS_PER_CLK_SHIFT);
			_tod_reg_wr(tod, ADI_TOD_CFG_INCR, lc_clk_cfg[lp].ns_per_clk, ADI_TOD_CFG_INCR_NS_PER_CLK_MASK, ADI_TOD_CFG_INCR_NS_PER_CLK_SHIFT);
			_tod_reg_wr(tod, ADI_TOD_CFG_INCR, lc_clk_cfg[lp].cnt_ctrl, ADI_TOD_CFG_INCR_CNT_CTRL_MASK, ADI_TOD_CFG_INCR_CNT_CTRL_SHIFT);
			err = 0;
			break;
		}
	}

	/*
	 *  //Is the default configure used if cannot find the correct local clk info?
	 *  if (lp == HW_TOD_LC_CLK_FREQ_CNT)
	 *  {
	 *      _tod_reg_wr(tod, ADI_TOD_CFG_INCR, lc_clk_cfg[0].frac_ns_per_clk, ADI_TOD_CFG_INCR_FRAC_NS_PER_CLK_MASK, ADI_TOD_CFG_INCR_FRAC_NS_PER_CLK_SHIFT);
	 *      _tod_reg_wr(tod, ADI_TOD_CFG_INCR, lc_clk_cfg[0].ns_per_clk, ADI_TOD_CFG_INCR_NS_PER_CLK_MASK, ADI_TOD_CFG_INCR_NS_PER_CLK_SHIFT);
	 *      _tod_reg_wr(tod, ADI_TOD_CFG_INCR, lc_clk_cfg[0].cnt_ctrl, ADI_TOD_CFG_INCR_CNT_CTRL_MASK, ADI_TOD_CFG_INCR_CNT_CTRL_SHIFT);
	 *  }
	 *
	 */

	return err;
}

static inline void timespec_to_tstamp(struct tod_tstamp *tstamp, const struct timespec64 *ts)
{
	tstamp->seconds = ts->tv_sec;
	tstamp->nanoseconds = ts->tv_nsec;
	tstamp->frac_nanoseconds = 0;
}

static inline void tstamp_to_timespec(struct timespec64 *ts, const struct tod_tstamp *tstamp)
{
	ts->tv_sec = tstamp->seconds;

	if (tstamp->frac_nanoseconds < (TOD_FRAC_NANO_NUM / 2))
		ts->tv_nsec = tstamp->nanoseconds;
	else
		ts->tv_nsec = tstamp->nanoseconds + 1;
}


static int _gc_get_cnt(struct phc_hw_tod *tod, u64 *pCnt)
{
	int err = 0;
	u32 gc_rd = 1;
	u32 gc_reg_cnt[2] = { 0, 0 };
	u64 gc_cnt = 0;

	/* Write the OP_GC:RD_GC_MASK to latch the GC counter register */
	_tod_reg_wr(tod, ADI_TOD_CFG_OP_GC, gc_rd, ADI_TOD_CFG_OP_GC_RD_GC_MASK, ADI_TOD_CFG_OP_GC_RD_GC_SHIFT);

	/* Read back the Golden Counter */
	_tod_reg_rd(tod, ADI_TOD_STAT_GC_0, &gc_reg_cnt[0], ADI_TOD_STAT_GC_0_MASK, ADI_TOD_STAT_GC_0_SHIFT);
	_tod_reg_rd(tod, ADI_TOD_STAT_GC_1, &gc_reg_cnt[1], ADI_TOD_STAT_GC_1_MASK, ADI_TOD_STAT_GC_1_SHIFT);

	gc_cnt = gc_reg_cnt[0] | ((u64)(gc_reg_cnt[1] & 0xFFFF) << 32);
	*pCnt = gc_cnt;

	return err;
}
static int _gc_set_cnt(struct phc_hw_tod *tod, u64 cnt)
{
	int err = 0;
	u32 gc_reg_cnt[2] = { 0, 0 };

	gc_reg_cnt[0] = cnt & 0xFFFFFFFF;
	gc_reg_cnt[1] = (cnt >> 32) & 0xFFFF;

	/* Write the GC value */
	_tod_reg_wr(tod, ADI_TOD_CFG_OP_GC_VAL_0, gc_reg_cnt[0], ADI_TOD_CFG_OP_GC_VAL_0_MASK, ADI_TOD_CFG_OP_GC_VAL_0_SHIFT);
	_tod_reg_wr(tod, ADI_TOD_CFG_OP_GC_VAL_1, gc_reg_cnt[1], ADI_TOD_CFG_OP_GC_VAL_1_MASK, ADI_TOD_CFG_OP_GC_VAL_1_SHIFT);

	return err;
}

static void _tod_hw_op_trig(struct phc_hw_tod *tod, u8 op_flag, u8 set_flag)
{
	u8 trig_mode = tod->trigger_mode;
	struct _tod_reg *r = &_tod_reg_op_trig[op_flag][trig_mode];

	_tod_reg_wr(tod, r->regaddr, (u32)set_flag, r->regmask, r->regshift);
}

static int _tod_hw_op_poll(struct phc_hw_tod *tod, u8 op_flag)
{
	u32 delay_us, delay_ms;
	u32 state;
	u8 trig_mode = tod->trigger_mode;
	struct _tod_reg *r = &_tod_reg_op_poll[op_flag][trig_mode];
	ktime_t timeout;

	delay_us = div_u64(tod->poll_delay_ns + TOD_1_MICRO_SEC_IN_NANO - 1,
			   TOD_1_MICRO_SEC_IN_NANO);
	delay_ms = delay_us / 1000;
	delay_us %= 1000;

	if (delay_ms != 0)
		mdelay(delay_ms);

	if (delay_us != 0)
		udelay(delay_us);

	timeout = ktime_add_us(ktime_get(), tod->poll_timeout_us);
	for (;;) {
		_tod_reg_rd(tod, r->regaddr, &state, r->regmask, r->regshift);
		if (state != HW_TOD_TRIG_OP_FLAG_GOING)
			break;
		if (tod->poll_timeout_us != 0 && ktime_compare(ktime_get(), timeout) > 0)
			break;
		udelay(10);
	}

	return state != HW_TOD_TRIG_OP_FLAG_GOING ? 0 : -ETIMEDOUT;
}

static void _tod_hw_tstamp_add_delay(struct phc_hw_tod *tod, struct tod_tstamp *tstamp)
{
	u64 ns;

	/* Update the fraction part of nanosecond and the nanosecond part in the tstamp */
	if ((tstamp->frac_nanoseconds + tod->trig_delay.frac_ns) < TOD_FRAC_NANO_NUM) {
		tstamp->frac_nanoseconds += tod->trig_delay.frac_ns;
		ns = tstamp->nanoseconds + tod->trig_delay.ns;
	} else {
		tstamp->frac_nanoseconds += tod->trig_delay.frac_ns;
		ns = tstamp->nanoseconds + tod->trig_delay.ns + 1;
	}

	/* Update the sencond part in the tstamp */
	if (ns >= TOD_1_SEC_IN_NANO)
		tstamp->seconds += div_u64_rem(ns, TOD_1_SEC_IN_NANO, &tstamp->nanoseconds);
	else
		tstamp->nanoseconds = ns;
}

static void _tod_hw_settstamp_to_reg(struct phc_hw_tod *tod, const struct tod_tstamp *tstamp)
{
	u32 reg_tstamp[3] = { 0 };

	reg_tstamp[0] = (tstamp->frac_nanoseconds & 0xFFFF) | ((tstamp->nanoseconds & 0xFFFF) << 16);
	reg_tstamp[1] = ((tstamp->nanoseconds & 0xFFFF0000) >> 16) | ((tstamp->seconds & 0xFFFF) << 16);
	reg_tstamp[2] = ((tstamp->seconds & 0xFFFFFFFF0000) >> 16);

	_tod_reg_wr(tod, ADI_TOD_CFG_TV_NSEC, reg_tstamp[0], ADI_TOD_REG_MASK_ALL, ADI_TOD_REG_SHIFT_NONE);
	_tod_reg_wr(tod, ADI_TOD_CFG_TV_SEC_0, reg_tstamp[1], ADI_TOD_REG_MASK_ALL, ADI_TOD_REG_SHIFT_NONE);
	_tod_reg_wr(tod, ADI_TOD_CFG_TV_SEC_1, reg_tstamp[2], ADI_TOD_REG_MASK_ALL, ADI_TOD_REG_SHIFT_NONE);
}

static void _tod_hw_gettstamp_from_reg(struct phc_hw_tod *tod, struct tod_tstamp *tstamp)
{
	u32 reg_tstamp[3] = { 0 };

	_tod_reg_rd(tod, ADI_TOD_STAT_TV_NSEC, &reg_tstamp[0], ADI_TOD_REG_MASK_ALL, ADI_TOD_REG_SHIFT_NONE);
	_tod_reg_rd(tod, ADI_TOD_STAT_TV_SEC_0, &reg_tstamp[1], ADI_TOD_REG_MASK_ALL, ADI_TOD_REG_SHIFT_NONE);
	_tod_reg_rd(tod, ADI_TOD_STAT_TV_SEC_1, &reg_tstamp[2], ADI_TOD_REG_MASK_ALL, ADI_TOD_REG_SHIFT_NONE);

	tstamp->frac_nanoseconds = reg_tstamp[0] & 0xFFFF;
	tstamp->nanoseconds = ((reg_tstamp[0] >> 16) & 0xFFFF) | ((reg_tstamp[1] & 0xFFFF) << 16);
	tstamp->seconds = ((reg_tstamp[1] >> 16) & 0xFFFF) | ((u64)reg_tstamp[2] << 16);
}

static int _tod_hw_settstamp(struct phc_hw_tod *tod, const struct tod_tstamp *vector)
{
	int err = 0;
	u64 gc_cnt = 0;

	/* Set the trigger delay to GC value register when in GC mode */
	if (tod->trigger_mode == HW_TOD_TRIG_MODE_GC) {
		_gc_get_cnt(tod, &gc_cnt);
		gc_cnt += tod->trig_delay_tick;
		_gc_set_cnt(tod, gc_cnt);
	}
	_tod_hw_settstamp_to_reg(tod, vector);
	/* Trigger ToD write */
	_tod_hw_op_trig(tod, HW_TOD_TRIG_OP_WR, HW_TOD_TRIG_SET_FLAG_TRIG);

	/* Poll the trigger */
	err |= _tod_hw_op_poll(tod, HW_TOD_TRIG_OP_WR);

	/* Clear the ToD write operation */
	_tod_hw_op_trig(tod, HW_TOD_TRIG_OP_WR, HW_TOD_TRIG_SET_FLAG_CLEAR);

	return err;
}

static int _tod_hw_gettstamp(struct phc_hw_tod *tod, struct tod_tstamp *vector)
{
	int err = 0;
	u64 gc_cnt = 0;

	/* Set the trigger delay to GC value register when in GC mode */
	if (tod->trigger_mode == HW_TOD_TRIG_MODE_GC) {
		_gc_get_cnt(tod, &gc_cnt);
		gc_cnt += tod->trig_delay_tick;
		_gc_set_cnt(tod, gc_cnt);
	}

	/* Trigger ToD read */
	_tod_hw_op_trig(tod, HW_TOD_TRIG_OP_RD, HW_TOD_TRIG_SET_FLAG_TRIG);

	err |= _tod_hw_op_poll(tod, HW_TOD_TRIG_OP_RD);

	if (!err)
		_tod_hw_gettstamp_from_reg(tod, vector);

	/* Clear the ToD read operation */
	_tod_hw_op_trig(tod, HW_TOD_TRIG_OP_RD, HW_TOD_TRIG_SET_FLAG_CLEAR);

	return err;
}

static int _tod_adjtime(struct phc_hw_tod *tod, s64 delta)
{
	int err;
	struct tod_tstamp tstamp;
	s32 ns;
	s64 seconds;

	err = _tod_hw_gettstamp(tod, &tstamp);
	if (err != 0)
		return err;

	if (tod->trigger_mode == HW_TOD_TRIG_MODE_GC)
		_tod_hw_tstamp_add_delay(tod, &tstamp);
	else
		tstamp.seconds += 1;

	seconds = div_s64_rem(delta, TOD_1_SEC_IN_NANO, &ns);

	if (ns < 0 && abs(ns) > tstamp.nanoseconds) {
		tstamp.nanoseconds = TOD_1_SEC_IN_NANO + ns + tstamp.nanoseconds;
		tstamp.seconds -= 1;
	} else {
		tstamp.nanoseconds += ns;
	}

	if (tstamp.nanoseconds < TOD_1_SEC_IN_NANO) {
		tstamp.seconds += seconds;
	} else {
		tstamp.nanoseconds -= TOD_1_SEC_IN_NANO;
		tstamp.seconds += 1;
	}

	err = _tod_hw_settstamp(tod, &tstamp);

	return err;
}

static int _tod_cfg_ppsx(struct phc_hw_tod *tod)
{
	int err = 0;
	u32 stop = 0;

	if (tod->ppsx.en) {
		_tod_reg_wr(tod, ADI_TOD_CFG_PPSX_START, tod->ppsx.delay_offset_ns, ADI_TOD_CFG_PPSX_START_PSTART_MASK, ADI_TOD_CFG_PPSX_START_PSTART_SHIFT);
		stop = (tod->ppsx.delay_offset_ns + tod->ppsx.pulse_width_ns) & 0xFFFFFFFF;
		_tod_reg_wr(tod, ADI_TOD_CFG_PPSX_STOP, stop, ADI_TOD_CFG_PPSX_STOP_PSTOP_MASK, ADI_TOD_CFG_PPSX_STOP_PSTOP_SHIFT);
	}

	return err;
}

static int adi_tod_module_init(struct phc_hw_tod *tod)
{
	u32 val;
	int err = 0;

	/* Update the ns and frac_ns part to the CFG_INCR */
	err = _tod_cfg_lc_clk(tod);

	/* Enable the ToD counter */
	if (!err)
		_tod_reg_wr(tod, ADI_TOD_CFG_INCR, ADI_HW_TOD_ENABLE,
			    ADI_TOD_CFG_INCR_CFG_TOD_CNT_EN_MASK,
			    ADI_TOD_CFG_INCR_CFG_TOD_CNT_EN_SHIFT);

	/* Enable and configure the PPSX */
	_tod_cfg_ppsx(tod);

	/* Connect pps_o to pps_i */
	val = readl(tod->axi_palau_gpio_pps_ctrl + PPS_CTRL_REG);
	writel(val | TOD_PPS_IN_SEL_PPS_OUT, tod->axi_palau_gpio_pps_ctrl + PPS_CTRL_REG);

	return err;
}

static int adi_tod_dt_parse(struct phc_hw_tod *tod)
{
	int ret;
	u32 val;
	struct adi_phc *adi_phc = container_of(tod, struct adi_phc, hw_tod);
	struct device *dev = adi_phc->dev;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "platform data missing!\n");
		return -ENODEV;
	}

	/* Required properties */
	ret = of_property_read_u32(np, "adi,trigger-mode", &val);
	if (ret) {
		dev_err(dev, "can not get the trigger mode, use the default GC trigger mode!\n");
		val = 0;
	}
	tod->trigger_mode = val;

	ret = of_property_read_u32(np, "adi,trigger-delay-tick", &val);
	if (ret) {
		dev_err(dev, "can not get the trigger delay tick, use the default delay tick count!\n");
		/* Default GC trigger delay is 1ms */
		val = (u32)div_u64((u64)tod->gc_clk_freq_khz, 1000);
		ret = 0;
	}
	tod->trig_delay_tick = val;

	/* Optional properties */
	ret = of_property_read_u32(np, "adi,ppsx-delay-offset-ns", &val);
	if (ret) {
		dev_err(dev, "can not get the ppsx delay offset, use the default delay tick count!\n");
		/* Default GC trigger delay is 1ms */
		val = 0;
		ret = 0;
	}
	tod->ppsx.delay_offset_ns = val;

	ret = of_property_read_u32(np, "adi,ppsx-pulse-width-ns", &val);
	if (ret) {
		dev_err(dev, "can not get the ppsx pulse width, use the default delay tick count!\n");
		/* Default GC trigger delay is 1ms */
		val = 500000000;
		ret = 0;
	}
	tod->ppsx.pulse_width_ns = val;
	tod->ppsx.en = 1;

	return ret;
}

static int adi_tod_enable(struct phc_hw_tod *tod, struct ptp_clock_request *request, int on)
{
	struct adi_phc *phc = container_of(tod, struct adi_phc, hw_tod);

	dev_err(phc->dev, "adi_tod: Doesn't support the enable call\n");
	return -EOPNOTSUPP;
}

static int adi_tod_settime(struct phc_hw_tod *tod, const struct timespec64 *ts)
{
	int err;
	unsigned long flags;
	struct tod_tstamp tstamp;

	timespec_to_tstamp(&tstamp, ts);

	spin_lock_irqsave(&tod->reg_lock, flags);
	err = _tod_hw_settstamp(tod, &tstamp);
	spin_unlock_irqrestore(&tod->reg_lock, flags);

	return err;
}

static int adi_tod_adjtime(struct phc_hw_tod *tod, s64 delta)
{
	int err;
	unsigned long flags;

	spin_lock_irqsave(&(tod->reg_lock), flags);
	err = _tod_adjtime(tod, delta);
	spin_unlock_irqrestore(&(tod->reg_lock), flags);

	return err;
}

static int adi_tod_gettimex(struct phc_hw_tod *tod,
			    struct timespec64 *ts,
			    struct ptp_system_timestamp *sts)
{
	int err;
	unsigned long flags;
	struct tod_tstamp tstamp;

	spin_lock_irqsave(&(tod->reg_lock), flags);
	ptp_read_system_prets(sts);
	err = _tod_hw_gettstamp(tod, &tstamp);
	ptp_read_system_postts(sts);
	tstamp_to_timespec(ts, &tstamp);
	spin_unlock_irqrestore(&(tod->reg_lock), flags);

	return err;
}

static int adi_tod_probe(struct phc_hw_tod *tod)
{
	unsigned long rate;
	struct adi_phc *phc = container_of(tod, struct adi_phc, hw_tod);
	int ret;

	spin_lock_init(&tod->reg_lock);

	/* get the gc and local clock frequency from the system clock */
	rate = clk_get_rate(phc->sys_clk);
	tod->gc_clk_freq_khz = (u32)div_u64((u64)rate, 1000);
	tod->lc_freq_khz = tod->gc_clk_freq_khz;

	ret = adi_tod_dt_parse(tod);

	if (tod->trigger_mode == HW_TOD_TRIG_MODE_GC) {
		u32 rem;
		/**
		 * In GC mode, the trigger delay value depends on the phc_hw_tod->trig_delay_tick
		 * tod_trig_delay.ns = tod->trig_delay_tick * 1e6 / tod->gc_clk_freq_khz
		 * tod_trig_delay.frac_ns = tod->trig_delay_tick * 1e6 % tod->gc_clk_freq_khz
		 * 1e6 is used to calculate the nano-second of the trigger tick so that
		 * the "tod->trig_delay_tick * 1e6" will not overflow unless tod->trig_delay_tick
		 * beyond the value "2^44".
		 */
		tod->trig_delay.ns = div_u64_rem(tod->trig_delay_tick * TOD_1_SEC_IN_MICRO,
						 tod->gc_clk_freq_khz, &rem);
		/**
		 * Fraction part of the nanosecond stores as a 16bit value in the ToD tstamp:
		 * frac_ns_tstamp = (trig_delay.rem_ns / gc_clk_frequency) * 2^16
		 */
		tod->trig_delay.frac_ns = (u16)div_u64(rem * TOD_FRAC_NANO_NUM,
						       tod->gc_clk_freq_khz);

		tod->poll_delay_ns = tod->trig_delay.ns;
		tod->poll_timeout_us = tod->poll_delay_ns * TOD_TIMEOUT_RATIO;
	} else {
		tod->poll_delay_ns = 0;
		tod->poll_timeout_us = TOD_1_SEC_IN_MICRO;
		tod->poll_timeout_us += tod->poll_timeout_us * TOD_TIMEOUT_RATIO;
	}

	ret |= adi_tod_module_init(tod);

	return ret;
}

static long adi_phc_aux_work(struct ptp_clock_info *ptp)
{
	struct adi_phc *phc = container_of(ptp, struct adi_phc, caps);

	dev_err(phc->dev, "ADI_PHC_Driver: Doesn't support the enable call\n");

	return -EOPNOTSUPP;
}

static int adi_phc_enable(struct ptp_clock_info *ptp,
			  struct ptp_clock_request *request, int on)
{
	struct adi_phc *phc = container_of(ptp, struct adi_phc, caps);

	return adi_tod_enable(&phc->hw_tod, request, on);
}

static int adi_phc_settime(struct ptp_clock_info *ptp, const struct timespec64 *ts)
{
	struct adi_phc *phc = container_of(ptp, struct adi_phc, caps);

	return adi_tod_settime(&phc->hw_tod, ts);
}

static int adi_phc_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct adi_phc *phc = container_of(ptp, struct adi_phc, caps);

	return adi_tod_adjtime(&phc->hw_tod, delta);
}

static int adi_phc_gettimex(struct ptp_clock_info *ptp,
			    struct timespec64 *ts,
			    struct ptp_system_timestamp *sts)
{
	struct adi_phc *phc = container_of(ptp, struct adi_phc, caps);

	return adi_tod_gettimex(&phc->hw_tod, ts, sts);
}

static int adi_phc_adjfine(struct ptp_clock_info *ptp, long scaled_ppm)
{
	int err;
	struct adi_phc *phc = container_of(ptp, struct adi_phc, caps);
	struct phc_hw_clk *hw_clk = &phc->hw_clk;

	if (hw_clk->clk_ops.adjfine)
		err = hw_clk->clk_ops.adjfine(hw_clk, scaled_ppm);
	else
		dev_err(phc->dev, "ADI_PHC_Driver: Doesn't support the adjfine call\n");
	return err;
}

static struct ptp_clock_info adi_ptp_caps = {
	.owner		= THIS_MODULE,
	.max_adj	= 50,
	.n_per_out	= 1,
	.adjfine	= &adi_phc_adjfine,
	.adjtime	= &adi_phc_adjtime,
	.gettimex64	= &adi_phc_gettimex,
	.settime64	= &adi_phc_settime,
	.enable		= &adi_phc_enable,
	.do_aux_work	= &adi_phc_aux_work,  /* Use the aux */
};

static int adi_ptp_probe(struct platform_device *pdev)
{
	int ret;
	u32 val;
	struct adi_phc *adi_phc;
	struct device *dev = &pdev->dev;
	struct clk *sys_clk;
	struct device_node *np = dev->of_node;
	void __iomem *p;

	if (!np) {
		dev_err(dev, "platform data missing!\n");
		return -ENODEV;
	}

	sys_clk = devm_clk_get(dev, "sys_clk");
	if (IS_ERR(sys_clk)) {
		dev_err(dev, "can not get sys clk\n");
		return PTR_ERR(sys_clk);
	}

	/* Required properties */
	ret = of_property_read_u32(np, "adi,max-adj", &val);
	if (ret)
		dev_warn(dev, "can not get the maximum frequency adjustment, use the defalt one!\n");
	else
		adi_ptp_caps.max_adj = val;

	adi_phc = devm_kzalloc(dev, sizeof(struct adi_phc), GFP_KERNEL);
	if (!adi_phc)
		return -ENOMEM;

	p = devm_platform_ioremap_resource_byname(pdev, "tod");
	if (IS_ERR(p)) {
		dev_err(dev, "cannot remap TOD registers\n");
		return PTR_ERR(p);
	}
	adi_phc->hw_tod.regs = p;

	p = devm_platform_ioremap_resource_byname(pdev,
						  "axi_palau_gpio_pps_ctrl");
	if (IS_ERR(p)) {
		dev_err(dev,
			"cannot remap axi_palau_gpio PPS control register\n");
		return PTR_ERR(p);
	}
	adi_phc->hw_tod.axi_palau_gpio_pps_ctrl = p;

	adi_phc->dev = dev;

	adi_phc->sys_clk = sys_clk;

	ret = adi_tod_probe(&adi_phc->hw_tod);

	ret = adi_phc_clk_probe(&adi_phc->hw_clk);

	adi_phc->caps = adi_ptp_caps;
	adi_phc->ptp_clk = ptp_clock_register(&adi_phc->caps, &pdev->dev);
	if (IS_ERR(adi_phc->ptp_clk)) {
		ret = PTR_ERR(adi_phc->ptp_clk);
		return ret;
	}

	platform_set_drvdata(pdev, adi_phc);

	dev_info(dev, "trigger method: %s\n",
		 adi_phc->hw_tod.trigger_mode == 0 ? "GC" : "1PPS");

	return ret;
}

static int adi_ptp_remove(struct platform_device *pdev)
{
	struct adi_phc *adi_phc = platform_get_drvdata(pdev);

	ptp_clock_unregister(adi_phc->ptp_clk);

	adi_phc_clk_remove(&adi_phc->hw_clk);

	/* Reset the ToD module, TBD */
	return 0;
}

static const struct of_device_id ptp_adi_of_match[] = {
	{
		.compatible = "adi,adi-ptp",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ptp_adi_of_match);

static struct platform_driver ptp_adi_driver = {
	.driver			= {
		.name		= "adi-ptp",
		.of_match_table = ptp_adi_of_match,
	},
	.probe			= adi_ptp_probe,
	.remove			= adi_ptp_remove,
};
MODULE_SOFTDEP("pre: ad9545");
module_platform_driver(ptp_adi_driver);
