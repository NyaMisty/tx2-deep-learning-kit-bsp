/*
 * MC error interrupt handling header file. Various defines and declarations
 * across tegra chips.
 *
 * Copyright (c) 2010-2014, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MCERR_H
#define __MCERR_H

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <linux/platform/tegra/mc.h>

#define MAX_PRINTS			5

/*
 * Lagacy usage of these registers. FIXME.
 */
#define MC_INT_STATUS			0x0
#define MC_INT_MASK			0x4
#define MC_ERR_BBC_STATUS		0x84
#define MC_ERR_BBC_ADR			0x88

#define MC_ERR_SMMU_MASK		(0x7 << 25)
#define MC_ERR_SMMU_BITS(err)		(((err) & MC_ERR_SMMU_MASK) >> 25)
#define MC_ERR_STATUS_WRITE		(1 << 16)
#define MC_ERR_STATUS_SECURE		(1 << 17)
#define MC_ERR_STATUS_ADR_HI		(3 << 20)

#define MC_INT_DECERR_EMEM			(1<<6)
#define MC_INT_SECURITY_VIOLATION		(1<<8)
#define MC_INT_ARBITRATION_EMEM			(1<<9)
#define MC_INT_INVALID_SMMU_PAGE		(1<<10)
#define MC_INT_INVALID_APB_ASID_UPDATE		(1<<11)
#define MC_INT_DECERR_VPR			(1<<12)
#define MC_INT_SECERR_SEC			(1<<13)
#define MC_INT_BBC_PRIVATE_MEM_VIOLATION	(1<<14)
#define MC_INT_DECERR_BBC			(1<<15)
#define MC_INT_DECERR_MTS			(1<<16)
#define MC_INT_DECERR_GENERALIZED_CARVEOUT	(1<<17)
#define MC_INT_WCAM_ERR				(1<<19)

/* hub common int status */
#define MC_HUBC_INT_SCRUB_ECC_WR_ACK		(1 << 0)

#define MC_ERR_DECERR_EMEM		(2)
#define MC_ERR_SECURITY_TRUSTZONE	(3)
#define MC_ERR_SECURITY_CARVEOUT	(4)
#define MC_ERR_INVALID_SMMU_PAGE	(6)

struct platform_device;
int tegra_mcerr_init(struct dentry *mc_paren, struct platform_device *pdev);
irqreturn_t tegra_mc_handle_general_fault(int src_chan, int intstatus);

/*
 * This describes errors that can be generated by the MC. One is defined for
 * each possibility.
 *
 * @sig Interrupt signiture for the error.
 * @msg Error description.
 * @flags Relevant flags for the error.
 * @stat_reg Register offset that holds the status of the error.
 * @addr_reg Register offset that holds the faulting address.
 */
struct mc_error {
	const char *msg;
	u32         sig;
	int         flags;
	u32         stat_reg;
	u32         addr_reg;
};

#define E_SMMU       (1<<0)
#define E_NO_STATUS  (1<<1) /* No status/addr */
#define E_TWO_STATUS (1<<2) /* Two status registers, no addr */

extern int mc_client_last;
extern u32 mc_int_mask;

struct mcerr_chip_specific {

	/*
	 * Return a pointer to the relevant mc_error struct for the passed
	 * interrupt signature.
	 *
	 * Called in interrupt context - no sleeping, etc.
	 */
	const struct mc_error *(*mcerr_info)(u32 intr);

	/*
	 * Provide actual user feed back to the kernel log. The passed data is
	 * everything that could be determined about the fault.
	 *
	 * Note: @smmu_info may be NULL if the error occured without involving
	 * the SMMU. This is something @mcerr_print must handle gracefully.
	 *
	 * Called in interrupt context - no sleeping, etc.
	 */
	void (*mcerr_print)(const struct mc_error *err,
			    const struct mc_client *client,
			    u32 status, phys_addr_t addr,
			    int secure, int rw, const char *smmu_info);

	/*
	 * Show the statistics for each client. This is called from a debugfs
	 * context - that means you can sleep and do general kernel stuff here.
	 */
	int (*mcerr_debugfs_show)(struct seq_file *s, void *v);

	/* Disable MC Error interrupt.
	 *
	 * Called in hard irq context to disable interrupt till
	 * soft irq handler logs the MC Error.
	 */
	void (*disable_interrupt)(unsigned int irq);

	/* Enable MC Error interrupt.
	 *
	 * Called from soft irq context after MC Error is logged.
	 */
	void (*enable_interrupt)(unsigned int irq);

	/* Clear MC Error interrupt.
	 *
	 * Called from soft irq context during MC Error print throttle.
	 */
	void (*clear_interrupt)(unsigned int irq);

	/* Log MC Error fault and clear interrupt source
	 *
	 * Called in soft irq context.
	 * As soon as interrupt status is cleared MC would be ready to
	 * hold next MC Error info.
	 */
	void (*log_mcerr_fault)(unsigned int irq);

	/* Numeric fields that must be set by the different chips. */
	unsigned int nr_clients;

	/*
	 * This array lists a string description of each valid interrupt bit.
	 * It must be at least 32 entries long. Entries that are not valid
	 * interrupts should be left as NULL. Each entry should be at most 12
	 * characters long.
	 */
	const char **intr_descriptions;
};

#define client(_swgroup, _name, _swgid)					\
	{ .swgroup = _swgroup, .name = _name, .swgid = TEGRA_SWGROUP_##_swgid, }

#define dummy_client   client("dummy", "dummy", INVALID)

#define MC_ERR(_sig, _msg, _flags, _stat_reg, _addr_reg)		\
	{ .sig = _sig, .msg = _msg, .flags = _flags,			\
			.stat_reg = _stat_reg, .addr_reg = _addr_reg }

#define mcerr_pr(fmt, ...)					\
	do {							\
		if (!mcerr_silenced) {				\
			trace_printk(fmt, ##__VA_ARGS__);	\
			pr_err(fmt, ##__VA_ARGS__);		\
		}						\
	} while (0)

/*
 * Error MMA tracking.
 */
#define MMA_HISTORY_SAMPLES 20
struct arb_emem_intr_info {
	int arb_intr_mma;
	u64 time;
	spinlock_t lock;
};

/*
 * Externs that get defined by the chip specific code. This way the generic
 * T3x/T11x/T12x can handle a much as possible.
 */
extern struct mc_client mc_clients[];
extern void mcerr_chip_specific_setup(struct mcerr_chip_specific *spec);
extern u32  mcerr_silenced;

#endif /* __MCERR_H */