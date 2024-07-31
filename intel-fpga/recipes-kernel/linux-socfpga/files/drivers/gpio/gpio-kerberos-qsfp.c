// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright Analog Devices, Inc. (C) 2023. All Rights Reserved
 *
 * GPIO driver for Kerberos QSFP pins
 *
 * Adapted from gpio-altera-a10sr.c
 */

#include <linux/platform_device.h>
#include <linux/gpio/driver.h>
#include <linux/module.h>

const char *signames[] = {
	"RESET", "MODSEL", "LPMODE", "INT", "MODPRS"
};

enum {
	QSFP_RESET,
	QSFP_MODSEL,
	QSFP_LPMODE,
	QSFP_INT,
	QSFP_MODPRS,
	QSFP_MAX
};

struct kerberos_qsfp_gpio_priv {
	struct gpio_chip gc;
	void __iomem *base;
	struct device *dev;
};

static int kerberos_qsfp_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	struct kerberos_qsfp_gpio_priv *priv = gpiochip_get_data(gc);
	unsigned int reg;
	u32 value;

	if (offset >= QSFP_MAX)
		return -EINVAL;

	reg = offset * 4;

	value = readl(priv->base + reg);

	return FIELD_GET(BIT(0), value);
}

static void kerberos_qsfp_gpio_set(struct gpio_chip *gc, unsigned int offset, int value)
{
	struct kerberos_qsfp_gpio_priv *priv = gpiochip_get_data(gc);
	unsigned int reg;

	if (offset >= QSFP_MAX)
		return;

	reg = offset * 4;

	if (value)
		writel(BIT(0), priv->base + reg);
	else
		writel(0, priv->base + reg);
}

static int kerberos_qsfp_gpio_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	if (offset >= QSFP_MAX)
		return -EINVAL;

	return 0;
}

static int kerberos_qsfp_gpio_direction_output(struct gpio_chip *gc, unsigned int offset, int value)
{
	if (offset >= QSFP_MAX)
		return -EINVAL;

	kerberos_qsfp_gpio_set(gc, offset, value);

	return 0;
}

static const struct gpio_chip kerberos_qsfp_gpio_chip = {
	.label = "kerberos_qsfp_gpio",
	.owner = THIS_MODULE,
	.get = kerberos_qsfp_gpio_get,
	.set = kerberos_qsfp_gpio_set,
	.direction_input = kerberos_qsfp_gpio_direction_input,
	.direction_output = kerberos_qsfp_gpio_direction_output,
	.can_sleep = true,
	.ngpio = 5,
	.base = -1,
};

static int kerberos_qsfp_gpio_probe(struct platform_device *pdev)
{
	struct kerberos_qsfp_gpio_priv *priv;
	void __iomem *p;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	p = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(p)) {
		dev_err(&pdev->dev, "cannot remap registers\n");
		return PTR_ERR(p);
	}
	priv->base = p;
	priv->dev = &pdev->dev;
	priv->gc = kerberos_qsfp_gpio_chip;
	priv->gc.parent = pdev->dev.parent;
	priv->gc.of_node = pdev->dev.of_node;

	return devm_gpiochip_add_data(&pdev->dev, &priv->gc, priv);
}

static const struct of_device_id kerberos_qsfp_gpio_of_match[] = {
	{ .compatible = "adi,kerberos-qsfp-gpio" },
	{ },
};
MODULE_DEVICE_TABLE(of, kerberos_qsfp_gpio_of_match);

static struct platform_driver kerberos_qsfp_gpio_driver = {
	.probe = kerberos_qsfp_gpio_probe,
	.driver = {
		.name	= "kerberos_qsfp_gpio",
		.of_match_table = of_match_ptr(kerberos_qsfp_gpio_of_match),
	},
};
module_platform_driver(kerberos_qsfp_gpio_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jie Zhang <jie.zhang@analog.com>");
MODULE_DESCRIPTION("Analog Devices Kerberos GPIO for QSFP pins");
