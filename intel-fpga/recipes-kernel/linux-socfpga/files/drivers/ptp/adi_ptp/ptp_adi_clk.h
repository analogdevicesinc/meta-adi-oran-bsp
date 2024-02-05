// SPDX-License-Identifier: GPL-2.0+
/*
 * Clock driver for the ptp hardware clock FTW setup.
 *
 * Copyright (C) 2022 Analog Device, Inc.
 */
#ifndef __PTP_ADI_CLK_H
#define __PTP_ADI_CLK_H

#include <linux/clk.h>
#include <linux/ptp_clock_kernel.h>

#define MAX_ENTROPY_REQ_PTP             (4 * 1024)

/*Get ppm value via optee-os*/
#define TA_CLOCK_GET_ADJ_FREQ_VALUE         0
/*Set ppm value via optee-os*/
#define TA_CLOCK_SET_ADJ_FREQ_VALUE         1

struct phc_hw_clk;

struct phc_clk_ops {
	int (*adjfine)(struct phc_hw_clk *phc_clk, long scaled_ppm);
	int (*adjfreq)(struct phc_hw_clk *phc_clk, s32 delta);
	int (*close)(struct phc_hw_clk *phc_clk);
};

struct optee_clk_private {
	u32 session_id;
	struct tee_shm *shm;
	struct tee_context *ctx;
};

struct phc_hw_clk {
	u64 freq;
	struct clk *tuning_clk;
	spinlock_t clk_lock;
	struct phc_clk_ops clk_ops;
	struct optee_clk_private optee_clk;
};

int adi_phc_clk_probe(struct phc_hw_clk *hw_clk);
int adi_phc_clk_remove(struct phc_hw_clk *hw_clk);

#endif
