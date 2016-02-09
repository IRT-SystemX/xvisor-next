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
 * @file reset-imx.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief iMX reset driver header.
 */

#ifndef __RESET_IMX_H_
# define __RESET_IMX_H_

# include <vmm_devtree.h>
# include <asm/io.h>

void imx_enable_cpu(int cpu, bool enable);
void imx_set_cpu_jump(int cpu, void *jump_addr);
u32 imx_get_cpu_arg(int cpu);
void imx_set_cpu_arg(int cpu, u32 arg);
int __init imx_src_init(struct vmm_devtree_node *src_node);

#endif /* !__RESET_IMX_H_ */
