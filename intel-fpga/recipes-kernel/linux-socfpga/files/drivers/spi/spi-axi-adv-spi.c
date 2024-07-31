// SPDX-License-Identifier: GPL-2.0-only
/*
 * Analog Devices AXI Advanced SPI controller driver
 *
 * Copyright (C) 2023  Analog Devices, Inc.
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/delay.h>

#define DRV_NAME			"axi-adv-spi"

#define VERSION				0x0
#define INSTANCE_ID			0x4
#define SCRATCH				0x8
#define NUMBER_OF_SLAVES		0xc
#define DGIER				0x1c
#define IPISR				0x20
#define IPIER				0x28
#define SRR				0x40
#define SPICR				0x60
#define SPISR				0x64
#define SPIDTR				0x68
#define SPIDRR				0x6c
#define SPISSR				0x70
#define TX_FIFO_OCY			0x74
#define RX_FIFO_OCY			0x78
#define ADV_FEAT_CONFIG0		0x80
#define ADV_FEAT_CONFIG(cs)		(ADV_FEAT_CONFIG0 + (cs) * 4)
#define IOCHECK_CONTROL			0x100
#define IOCHECK_CONFIG_0		0x104
#define IOCHECK_CONFIG_1		0x108
#define IOCHECK_CONFIG_2		0x10c
#define IOCHECK_STATUS_0		0x110
#define IOCHECK_STATUS_1		0x114
#define IOCHECK_STATUS_2		0x118
#define CLOCK_DIVIDER_PHASE		0x11c

#define SPI_CLK_DIV_MAX			0x10000
#define SPI_CLK_DIV_MIN			0x1

#define SPICR_MAN_SS_EN			BIT(7)
#define SPICR_RX_FIFO_RESET		BIT(6)
#define SPICR_TX_FIFO_RESET		BIT(5)
#define SPICR_MASTER			BIT(2)
#define SPICR_SPE			BIT(1)

#define ADV_FEAT_MISO_SAMPLE_SLIP(n)	(((n) & 0x3) << 5)
#define ADV_FEAT_FOUR_WIRE		BIT(4)
#define ADV_FEAT_LSB_FIRST		BIT(3)
#define ADV_FEAT_CPHA			BIT(2)
#define ADV_FEAT_CPOL			BIT(1)
#define ADV_FEAT_ENABLE			BIT(0)

#define SPI_ENABLE			(SPICR_MASTER | SPICR_SPE)
#define SPI_DISABLE			(SPICR_MASTER)
#define SPI_RESET			(SPICR_RX_FIFO_RESET | SPICR_TX_FIFO_RESET | SPICR_MASTER)

#define FIFO_SIZE			8

#define RESET_TIMEOUT			20

/*
 1. load default configuration and advanced feature configuration
 2. reset transmit and receive FIFOs
 4. set desired slave select
 3. load data into transmit FIFO
 5. set the ENABLE bit to start the transaction
 6. when transaction is done, clear the ENABLE bit
 ?. reset transmit and receive FIFOs
 */


struct axi_adv_spi {
	struct spi_master	*master;
	void __iomem		*base;
	u32			spi_clk;
};

static void axi_adv_spi_set_cs(struct spi_device *spi, bool value)
{
	struct axi_adv_spi *axi_adv_spi = spi_master_get_devdata(spi->master);
	u32 cs;

	dev_dbg(axi_adv_spi->master->dev.parent, "chipselect %d\n", spi->chip_select);

	cs = readl(axi_adv_spi->base + SPISSR);

	if (value)
		cs |= (1 << spi->chip_select);
	else
		cs &= ~(1 << spi->chip_select);

	writel(cs, axi_adv_spi->base + SPISSR);
}

static int axi_adv_spi_setup(struct spi_device *spi)
{
	struct axi_adv_spi *axi_adv_spi = spi_master_get_devdata(spi->master);
	u32 clk_div, cfg;

	clk_div = DIV_ROUND_UP(axi_adv_spi->spi_clk / 2, spi->max_speed_hz);
	if (clk_div > SPI_CLK_DIV_MAX)
		clk_div = SPI_CLK_DIV_MAX;
	else if (clk_div < SPI_CLK_DIV_MIN)
		clk_div = SPI_CLK_DIV_MIN;

	cfg = (clk_div - 1) << 16;

	/* This setting seems related to the SPI frequency
	   2 seems good for 50MHz */
	cfg |= ADV_FEAT_MISO_SAMPLE_SLIP(2);

	if (!(spi->mode & SPI_3WIRE))
		cfg |= ADV_FEAT_FOUR_WIRE;

	if (spi->mode & SPI_CPHA)
		cfg |= ADV_FEAT_CPHA;

	if (spi->mode & SPI_CPOL)
		cfg |= ADV_FEAT_CPOL;

	if (spi->mode & SPI_LSB_FIRST)
		cfg |= ADV_FEAT_LSB_FIRST;

	cfg |= ADV_FEAT_ENABLE;

	writel(cfg, axi_adv_spi->base + ADV_FEAT_CONFIG(spi->chip_select));

	return 0;
}

static int axi_adv_spi_transfer_one(struct spi_master *master,
				    struct spi_device *spi, struct spi_transfer *t)
{
	struct axi_adv_spi *axi_adv_spi = spi_master_get_devdata(spi->master);
	const unsigned char *tx_buf = t->tx_buf;
	unsigned char *rx_buf = t->rx_buf;
	int remain = t->len;
	int cur_len, timeout, i;
	int reset_timeout = RESET_TIMEOUT;
	u32 adv_feat_config, control, status;
	int clk_div;
	u32 dummy = 0;

	adv_feat_config = readl(axi_adv_spi->base + ADV_FEAT_CONFIG(spi->chip_select));
	if (adv_feat_config & ADV_FEAT_ENABLE)
		clk_div = FIELD_GET(GENMASK(31, 16), adv_feat_config) + 1;
	else
		clk_div = 1;

	writel(SPI_RESET, axi_adv_spi->base + SPICR);
	for (i = 0; i < reset_timeout; i++) {
		control = readl(axi_adv_spi->base + SPICR);
		if (!(control & (SPICR_RX_FIFO_RESET | SPICR_TX_FIFO_RESET)))
			break;
	}
	if (control & (SPICR_RX_FIFO_RESET | SPICR_TX_FIFO_RESET))
		return -ETIMEDOUT;

	while (remain > 0) {
		cur_len = min(FIFO_SIZE, remain);

		if (tx_buf) {
			for (i = 0; i < cur_len; i++)
				writel(tx_buf[i], axi_adv_spi->base + SPIDTR);
		} else {
			for (i = 0; i < cur_len; i++)
				writel(dummy, axi_adv_spi->base + SPIDTR);
		}

		writel(SPI_ENABLE, axi_adv_spi->base + SPICR);

		timeout = clk_div * cur_len * 8;

		for (i = 0; i < timeout; i++) {
			status = readl(axi_adv_spi->base + SPISR);
			if (!(status & BIT(5)))
				break;
		}

		if (status & BIT(5))
			return -ETIMEDOUT;

		if (rx_buf) {
			for (i = 0; i < cur_len; i++)
				rx_buf[i] = readl(axi_adv_spi->base + SPIDRR);
		} else {
			/* TODO  for some cases, we should just do a reset */
			for (i = 0; i < cur_len; i++)
				dummy = readl(axi_adv_spi->base + SPIDRR);
		}

		writel(SPI_DISABLE, axi_adv_spi->base + SPICR);

		remain -= cur_len;
	}

	return 0;
}

static int axi_adv_spi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct spi_master *master;
	struct axi_adv_spi *axi_adv_spi;
	struct clk *clk;
	u32 version;
	int ret;

	master = spi_alloc_master(&pdev->dev, sizeof(*axi_adv_spi));
	if (!master)
		return -ENOMEM;

	axi_adv_spi = spi_master_get_devdata(master);

	axi_adv_spi->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(axi_adv_spi->base)) {
		ret = PTR_ERR(axi_adv_spi->base);
		goto put_master;
	}

	version = readl(axi_adv_spi->base + VERSION);
	dev_info(&pdev->dev, "ADI AXI Advanced SPI version: 0x%x\n", version);

	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "could not get spi clock\n");
		return PTR_ERR(clk);
	}

	axi_adv_spi->spi_clk = clk_get_rate(clk);

	/* TODO  We should get this value from register */
	master->num_chipselect = 32;
	/* TODO  We may support more mode bits */
	master->mode_bits = SPI_CPOL | SPI_CPHA;
	master->setup = axi_adv_spi_setup;
	master->transfer_one = axi_adv_spi_transfer_one;
	master->set_cs = axi_adv_spi_set_cs;
	master->dev.of_node = np;
	master->bus_num = pdev->id;

	axi_adv_spi->master = master;
	platform_set_drvdata(pdev, axi_adv_spi);

	/* TODO  Do we need to reset? */

	ret = devm_spi_register_master(&pdev->dev, master);
	if (ret)
		goto put_master;

	return 0;

put_master:
	spi_master_put(master);
	return ret;
}

static const struct of_device_id axi_adv_spi_of_match[] = {
	{ .compatible = "adi,axi-adv-spi" },
	{},
};
MODULE_DEVICE_TABLE(of, axi_adv_spi_of_match);

static struct platform_driver axi_adv_spi_driver = {
	.probe = axi_adv_spi_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = axi_adv_spi_of_match,
	},
};

module_platform_driver(axi_adv_spi_driver);

MODULE_DESCRIPTION("Analog Device AXI Advanced SPI Controller driver");
MODULE_AUTHOR("Jie Zhang <jie.zhang@analog.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
