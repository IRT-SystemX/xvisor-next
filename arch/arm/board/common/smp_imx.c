/**
 * Copyright (c) 2016 Jean-Christophe Dubois.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file smp_imx.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief i.MX SMP boot support
 *
 * Adapted from arch/arm/mach-imx/platsmp.c
 *
 *  Copyright 2011 Freescale Semiconductor, Inc.
 *  All Rights Reserved
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_cache.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <arch_barrier.h>

#include <smp_ops.h>
#include <smp_scu.h>
#include <reset-imx.h>


extern u8 v7_secondary_startup;
static physical_addr_t v7_secondary_startup_paddr;

u32 g_diag_reg;

static int __init smp_imx_init(struct vmm_devtree_node *node,
				unsigned int cpu)
{
	int rc;
	int ncores;
	static int initialized = 0;
	physical_addr_t g_diag_reg_paddr;

	if (initialized) {
		return VMM_OK;
	}

	/* Map SCU base */
	rc = scu_init(NULL);
	if (VMM_OK != rc) {
		return rc;
	}

	/* Check core count from SCU */
	ncores = scu_get_core_count();
	if (ncores <= cpu) {
		return VMM_ENOSYS;
	}

	rc = vmm_host_va2pa((virtual_addr_t)&v7_secondary_startup,
			    &v7_secondary_startup_paddr);
	if (VMM_OK != rc) {
		vmm_printf("Failed to get cpu jump physical address "
			   "(0x%p)\n", v7_secondary_startup);
		return rc;
	}

	vmm_printf("Secondary CPU jump address set to 0x%08x\n",
		  v7_secondary_startup_paddr);

	scu_enable();

	/* Need to enable SCU standby for entering WAIT mode */
	imx_scu_standby_enable();

	/*
	 * The diagnostic register holds the errata bits.  Mostly bootloader
	 * does not bring up secondary cores, so that when errata bits are set
	 * in bootloader, they are set only for boot cpu.  But on a SMP
	 * configuration, it should be equally done on every single core.
	 * Read the register from boot cpu here, and will replicate it into
	 * secondary cores when booting them.
	 */
	asm("mrc p15, 0, %0, c15, c0, 1" : "=r" (g_diag_reg) : : "cc");
	rc = vmm_host_va2pa((virtual_addr_t)&g_diag_reg, &g_diag_reg_paddr);
	if (VMM_OK != rc) {
		vmm_printf("Failed to get cpu jump physical address (0x%p)\n",
			   &g_diag_reg);
		return rc;
	}

	vmm_flush_dcache_range((virtual_addr_t)&g_diag_reg,
			       (virtual_addr_t)
			       (&g_diag_reg + sizeof(g_diag_reg)));
	vmm_clean_outer_cache_range(g_diag_reg_paddr,
				    g_diag_reg_paddr + 1);

	rc = imx_src_init(NULL);
	if (VMM_OK != rc) {
		return rc;
	}

	initialized = 1;

	return VMM_OK;
}

static int __init smp_imx_prepare(unsigned int cpu)
{
	imx_set_cpu_jump(cpu, (void *)v7_secondary_startup_paddr);
	imx_set_cpu_arg(cpu, 0);

	return VMM_OK;
}

static int __init smp_imx_boot(unsigned int cpu)
{
	/* Wake up the core through the SRC device */
	scu_cpu_boot(cpu);
	imx_enable_cpu(cpu, true);

	return VMM_OK;
}

static struct smp_operations smp_imx_ops = {
	.name = "smp-imx",
	.cpu_init = smp_imx_init,
	.cpu_prepare = smp_imx_prepare,
	.cpu_boot = smp_imx_boot,
};

SMP_OPS_DECLARE(smp_imx, &smp_imx_ops);
