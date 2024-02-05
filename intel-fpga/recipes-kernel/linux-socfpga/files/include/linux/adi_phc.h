#ifndef __ADI_PHC_H__
#define __ADI_PHC_H__

#include <linux/device.h>
#include <linux/ptp_clock_kernel.h>

struct adi_phc {
	struct device *dev;
	struct ptp_clock *ptp_clk;
};

#endif /* __ADI_PHC_H__ */
