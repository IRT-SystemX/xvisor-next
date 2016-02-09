/**
 * Copyright (C) 2016 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file smp_scu.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief SMP Snoop Control Unit API header.
 */

#ifndef __SMP_SCU_H_
# define __SMP_SCU_H_

# include <vmm_devtree.h>

# ifdef CONFIG_SMP
u32 scu_get_core_count(void);
bool scu_cpu_core_is_smp(u32 cpu);
void scu_enable(void);
# endif /* CONFIG_SMP */

int scu_power_mode(u32 mode);

# if defined(CONFIG_ARM_SMP_OPS) && defined(CONFIG_ARM_GIC)
void imx_scu_standby_enable(void);
int __init scu_init(struct vmm_devtree_node *scu_node);
int __init scu_cpu_boot(unsigned int cpu);
# endif /* CONFIG_ARM_SMP_OPS && CONFIG_ARM_GIC */

#endif /* !__RESET_IMX_H_ */
