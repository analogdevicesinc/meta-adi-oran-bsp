# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the Altera device drivers.
#

ifneq ($(CONFIG_INTEL_FPGA_ETILE_LITE),y)
obj-$(CONFIG_NET_ALTERA_ETH) += altera_eth.o
altera_eth-objs := altera_utils.o intel_fpga_tod.o altera_eth_dma.o \
		   altera_sgdma.o altera_msgdma.o \
		   altera_msgdma_prefetcher.o
endif
ifeq ($(CONFIG_ALTERA_TSE),y)
	obj-$(CONFIG_ALTERA_TSE) += altera_tse.o
	altera_tse-objs := altera_tse_main.o altera_tse_ethtool.o
endif
ifeq ($(CONFIG_INTEL_FPGA_QSE_LL),y)
	obj-$(CONFIG_INTEL_FPGA_QSE_LL) += intel_fpga_qse_ll.o
	intel_fpga_qse_ll-objs := intel_fpga_qse_ll_main.o intel_fpga_qse_ll_ethtool.o
endif
ifneq ($(CONFIG_INTEL_FPGA_ETILE),)
	obj-$(CONFIG_INTEL_FPGA_ETILE) += intel_fpga_etile.o
	intel_fpga_etile-objs := intel_fpga_etile_main.o intel_fpga_etile_fec.o \
				 intel_fpga_etile_ethtool.o
ifeq ($(CONFIG_INTEL_FPGA_ETILE_LITE),y)
	intel_fpga_etile-objs += altera_utils.o
endif
endif
