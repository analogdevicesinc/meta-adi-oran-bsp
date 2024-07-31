/* SPDX-License-Identifier: GPL-2.0-only */
/* Altera TSE SGDMA and MSGDMA Linux driver
 * Copyright (C) 2014 Altera Corporation. All rights reserved
 * Portions Copyright (C) 2024 Analog Devices, Inc.
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/io.h>

#ifndef __ALTERA_UTILS_H__
#define __ALTERA_UTILS_H__

void tse_set_bit(void __iomem *ioaddr, size_t offs, u32 bit_mask);
void tse_clear_bit(void __iomem *ioaddr, size_t offs, u32 bit_mask);
int tse_bit_is_set(void __iomem *ioaddr, size_t offs, u32 bit_mask);
int tse_bit_is_clear(void __iomem *ioaddr, size_t offs, u32 bit_mask);
int request_and_map(struct platform_device *pdev, const char *name,
		    struct resource **res, void __iomem **ptr);

#ifndef CONFIG_INTEL_FPGA_ETILE_LITE
static inline
u32 csrrd32(void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);

	return readl(paddr);
}

static inline
u16 csrrd16(void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);

	return readw(paddr);
}

static inline
u8 csrrd8(void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);

	return readb(paddr);
}

static inline
void csrwr32(u32 val, void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);

	writel(val, paddr);
}

static inline
void csrwr16(u16 val, void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);

	writew(val, paddr);
}

static inline
void csrwr8(u8 val, void __iomem *mac, size_t offs)
{
	void __iomem *paddr = (void __iomem *)((uintptr_t)mac + offs);

	writeb(val, paddr);
}
#else
#include "intel_fpga_etile.h"

static inline
void __iomem *adi_kerberos_csr_prepare(void __iomem *mac, size_t offs)
{
	size_t offset;
	void __iomem *paddr;

	if ((uintptr_t)mac & ADI_KERBEROS_RSFEC_BIT)
		offset = ADI_KERBEROS_RSFEC_OFFSET + (offs << 2);
	else if ((uintptr_t)mac & ADI_KERBEROS_XCVR_BIT)
		offset = ADI_KERBEROS_XCVR_OFFSET + (offs << 2);
	else
		offset = offs;

	mac = (void __iomem *)((uintptr_t)mac & ~(uintptr_t)ADI_KERBEROS_ETILE_HACK_MASK);

	paddr = (void __iomem *)((uintptr_t)mac + offset);

	return paddr;
}

static inline
u32 csrrd32(void __iomem *mac, size_t offs)
{
	void __iomem *paddr = adi_kerberos_csr_prepare(mac, offs);

	return readl(paddr);
}

static inline
u16 csrrd16(void __iomem *mac, size_t offs)
{
	void __iomem *paddr = adi_kerberos_csr_prepare(mac, offs);

	return readw(paddr);
}

static inline
u8 csrrd8(void __iomem *mac, size_t offs)
{
	void __iomem *paddr = adi_kerberos_csr_prepare(mac, offs);

	return readb(paddr);
}

static inline
void csrwr32(u32 val, void __iomem *mac, size_t offs)
{
	void __iomem *paddr = adi_kerberos_csr_prepare(mac, offs);

	writel(val, paddr);
}

static inline
void csrwr16(u16 val, void __iomem *mac, size_t offs)
{
	void __iomem *paddr = adi_kerberos_csr_prepare(mac, offs);

	writew(val, paddr);
}

static inline
void csrwr8(u8 val, void __iomem *mac, size_t offs)
{
	void __iomem *paddr = adi_kerberos_csr_prepare(mac, offs);

	writeb(val, paddr);
}

#endif /* CONFIG_INTEL_FPGA_ETILE_LITE */
#endif /* __ALTERA_UTILS_H__*/
