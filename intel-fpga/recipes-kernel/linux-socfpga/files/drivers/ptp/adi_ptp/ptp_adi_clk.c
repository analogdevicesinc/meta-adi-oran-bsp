// SPDX-License-Identifier: GPL-2.0+
/*
 * Clock driver for the ptp hardware clock FTW setup.
 *
 * Copyright (C) 2022 Analog Device, Inc.
 */
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/timekeeping.h>
#include <linux/clk/ad9545.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/tee_drv.h>
#include <linux/uuid.h>
#include <linux/acpi.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "../ptp_private.h"
#include "ptp_adi.h"


#if 0
const struct tee_client_device_id optee_clock_id_table[] = {
	{ UUID_INIT(0xbbe48373, 0xc63a, 0x41c5, 0xbc, 0xc7, 0x09, 0x05, 0x59, 0xb2, 0x7e, 0x48) },
	{}
};
MODULE_DEVICE_TABLE(tee, optee_clock_id_table);

static int optee_ctx_match(struct tee_ioctl_version_data *ver, const void *data)
{
	if (ver->impl_id == TEE_IMPL_ID_OPTEE)
		return 1;
	else
		return 0;
}

static int optee_clk_get_adj_freq_value(struct phc_hw_clk *hw_clk, unsigned long long *pdata)
{
	int ret = 0;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[4];
	unsigned long long *pshm = NULL;
	struct adi_phc *adi_phc = container_of(hw_clk, struct adi_phc, hw_clk);
	struct device *dev = adi_phc->dev;
	struct optee_clk_private *pvt_data = &(hw_clk->optee_clk);

	memset(&inv_arg, 0, sizeof(inv_arg));
	memset(&param, 0, sizeof(param));

	/* Invoke TA_CLOCK_GET_ADJ_FREQ_VALUE  function of Trusted App */
	inv_arg.func = TA_CLOCK_GET_ADJ_FREQ_VALUE;
	inv_arg.session = pvt_data->session_id;
	inv_arg.num_params = 4;

	/* Fill invoke cmd params */
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	param[0].u.memref.shm = pvt_data->shm;
	param[0].u.memref.size = sizeof(*pshm);
	param[0].u.memref.shm_offs = 0;

	ret = tee_client_invoke_func(pvt_data->ctx, &inv_arg, param);
	if ((ret < 0) || (inv_arg.ret != 0)) {
		dev_err(dev, "%s: TA_CLOCK_GET_ADJ_FREQ_VALUE invoke err: %x\n", __func__, inv_arg.ret);
		return -EINVAL;
	}

	pshm = (unsigned long long *)tee_shm_get_va(pvt_data->shm, 0);
	if (IS_ERR(pshm)) {
		dev_err(dev, "%s: tee_shm_get_va failed\n", __func__);
		return -EINVAL;
	}

	memcpy(pdata, pshm, sizeof(unsigned long long));

	return ret;
}

static int optee_clk_set_adj_freq_value(struct phc_hw_clk *hw_clk, unsigned long long value)
{
	int ret = 0;
	struct tee_ioctl_invoke_arg inv_arg;
	struct tee_param param[4];
	unsigned long long *pshm = NULL;
	struct adi_phc *adi_phc = container_of(hw_clk, struct adi_phc, hw_clk);
	struct device *dev = adi_phc->dev;
	struct optee_clk_private *pvt_data = &(hw_clk->optee_clk);

	memset(&inv_arg, 0, sizeof(inv_arg));
	memset(&param, 0, sizeof(param));

	/* Invoke TA_CLOCK_SET_ADJ_FREQ_VALUE  function of Trusted App */
	inv_arg.func = TA_CLOCK_SET_ADJ_FREQ_VALUE;
	inv_arg.session = pvt_data->session_id;
	inv_arg.num_params = 4;

	/* Fill invoke cmd params */
	param[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = pvt_data->shm;
	param[0].u.memref.size = sizeof(*pshm);
	param[0].u.memref.shm_offs = 0;

	pshm = (unsigned long long *)tee_shm_get_va(pvt_data->shm, 0);
	if (IS_ERR(pshm)) {
		dev_err(dev, "%s: TA_CLOCK_SET_ADJ_FREQ_VALUE failed\n", __func__);
		return -EINVAL;
	}
	memset(pshm, 0, sizeof(unsigned long long));
	memcpy(pshm, (void *)&value, sizeof(unsigned long long));

	ret = tee_client_invoke_func(pvt_data->ctx, &inv_arg, param);
	if ((ret < 0) || (inv_arg.ret != 0)) {
		dev_err(dev, "%s: TA_CMD_GET_ENTROPY invoke err: %x\n", __func__, inv_arg.ret);
		return -EINVAL;
	}

	return ret;
}

/*
 * @brief Adjusts the frequency of the hardware clock by optee-os.
 *  param:
 *       hw_clk     -   not used
 *       scaled_ppm -   Desired frequency offset from nominal frequency in parts per million, but with a
 *                      16 bit binary fractional field.
 */
static int optee_clk_adjfine(struct phc_hw_clk *hw_clk, long scaled_ppm)
{
	int ret = 0;
	unsigned long long data;
	struct adi_phc *adi_phc = container_of(hw_clk, struct adi_phc, hw_clk);
	struct device *dev = adi_phc->dev;

	ret = optee_clk_get_adj_freq_value(hw_clk, &data);
	dev_info(dev, "%s: nco get value = %lld\n", __func__, data);

	ret = optee_clk_set_adj_freq_value(hw_clk, scaled_ppm);
	dev_info(dev, "%s: adjusted scaled ppm value = %ld\n", __func__, scaled_ppm);

	return ret;
}

static int phc_clk_optee_probe(struct phc_hw_clk *hw_clk)
{
	int ret = 0;
	int err = -ENODEV;
	struct tee_ioctl_open_session_arg sess_arg;
	struct adi_phc *adi_phc = container_of(hw_clk, struct adi_phc, hw_clk);
	struct device *dev = adi_phc->dev;
	struct optee_clk_private *pvt_data = &(hw_clk->optee_clk);

	pvt_data->ctx = tee_client_open_context(NULL, optee_ctx_match, NULL, NULL);
	if (IS_ERR(pvt_data->ctx)) {
		dev_err(dev, "%s: open tee context failed!\n", __func__);
		return -ENODEV;
	}

	memset(&sess_arg, 0, sizeof(sess_arg));
	export_uuid(sess_arg.uuid, &optee_clock_id_table[0].uuid);
	sess_arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
	sess_arg.num_params = 0;

	ret = tee_client_open_session(pvt_data->ctx, &sess_arg, NULL);
	if ((ret < 0) || (sess_arg.ret != 0)) {
		dev_err(dev, "%s: tee_client_open_session failed, err: %x\n", __func__, sess_arg.ret);
		err = -EINVAL;
		goto out_ctx;
	}
	pvt_data->session_id = sess_arg.session;

	pvt_data->shm = tee_shm_alloc(pvt_data->ctx, MAX_ENTROPY_REQ_PTP, TEE_SHM_MAPPED | TEE_SHM_DMA_BUF);
	if (IS_ERR(pvt_data->shm)) {
		dev_err(dev, "%s: tee_shm_alloc failed\n", __func__);
		err = -ENOMEM;
		goto out_shm_alloc;
	}

	return ret;

out_shm_alloc:
	tee_client_close_session(pvt_data->ctx, pvt_data->session_id);
out_ctx:
	tee_client_close_context(pvt_data->ctx);

	return err;
}

static int optee_clk_remove(struct phc_hw_clk *hw_clk)
{
	struct optee_clk_private *pvt_data = &(hw_clk->optee_clk);

	tee_shm_free(pvt_data->shm);
	tee_client_close_session(pvt_data->ctx, pvt_data->session_id);
	tee_client_close_context(pvt_data->ctx);

	return 0;
}
#endif

static int ad9545_adjfine(struct phc_hw_clk *hw_clk, long scaled_ppm)
{
	int neg_adj = 0;
	u64 freq, adj;

	if (scaled_ppm < 0) {
		neg_adj = 1;
		scaled_ppm = -scaled_ppm;
	}

	/*
	 * Center frequency of AUX NCO in ad9545:
	 * ---------------------------------------------------------------
	 * | 55 |    ...     | 40 | 39 |             ....            | 0 |
	 * | ->  INTEGER Hz    <- | ->          FRACTIONAL HZ         <- |
	 *                      |                                      |
	 *                     1 Hz                                2^(-40) Hz
	 */

	freq = hw_clk->freq;

	/* adj = freq * scaled_ppm / (1,000,000 * 2^16) */
	adj = mul_u64_u64_shr(freq, scaled_ppm, 16);
	adj = DIV_U64_ROUND_CLOSEST(adj, 1000000);

	if (neg_adj == 1)
		freq -= adj;
	else
		freq += adj;

	return ad9545_set_aux_nco_tuning_freq(hw_clk->tuning_clk, freq);
}

static int phc_clk_i2c_probe(struct phc_hw_clk *hw_clk)
{
	struct adi_phc *adi_phc = container_of(hw_clk, struct adi_phc, hw_clk);
	struct device *dev = adi_phc->dev;
	struct clk *tuning_clk, *pll_clk;
	int ret;

	tuning_clk = devm_clk_get(dev, "tuning_clk");
	if (IS_ERR(tuning_clk)) {
		dev_err(dev, "can not get tuning clk\n");
		return PTR_ERR(tuning_clk);
	}

	hw_clk->tuning_clk = tuning_clk;

	pll_clk = clk_get_parent(adi_phc->sys_clk);
	if (!pll_clk) {
		dev_err(dev, "can not get the parent clock of phc sys_clk\n");
		return -EINVAL;
	}

	ret = clk_set_parent(pll_clk, tuning_clk);
	if (ret < 0) {
		dev_err(dev, "can not set tuning_clk as parent of phc sys_clk\n");
		return ret;
	}

	return ad9545_get_aux_nco_tuning_freq(tuning_clk, &hw_clk->freq);
}

struct phc_clk_ops i2c_clk_ops = {
	.adjfine	= &ad9545_adjfine,
};

#if 0
struct phc_clk_ops optee_clk_ops = {
	.adjfine	= &optee_clk_adjfine,
	.close		= &optee_clk_remove,
};
#endif


int adi_phc_clk_probe(struct phc_hw_clk *hw_clk)
{
	struct adi_phc *adi_phc = container_of(hw_clk, struct adi_phc, hw_clk);
	struct device *dev = adi_phc->dev;

#if 0
	if (phc_clk_optee_probe(hw_clk) == 0) {
		hw_clk->clk_ops = optee_clk_ops;
	} else
#endif
	if (phc_clk_i2c_probe(hw_clk) == 0) {
		hw_clk->clk_ops = i2c_clk_ops;
	} else {
		dev_err(dev, "No valid phc hardware clock chip");
		return -ENODEV;
	}

	spin_lock_init(&hw_clk->clk_lock);
	return 0;
}

int adi_phc_clk_remove(struct phc_hw_clk *hw_clk)
{
	if (hw_clk->clk_ops.close)
		return hw_clk->clk_ops.close(hw_clk);

	return 0;
}
