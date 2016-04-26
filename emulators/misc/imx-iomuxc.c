/**
 * Copyright (C) 2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
 *
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
 * @file imx-iomuxc.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief i.MX6 iomuxc emulator
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>

#define MODULE_DESC			"i.MX IOMUXC Emulator"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			imx_iomuxc_emulator_init
#define	MODULE_EXIT			imx_iomuxc_emulator_exit

static int imx_iomuxc_emulator_read(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 *dst,
				 u32 size)
{
	if (offset == 0x004) {
		*dst = 0x48400005;
	} else if (offset == 0x00C) {
		*dst = 0x01E00000;
	} else if ((offset == 0x018) || (offset == 0x01C)) {
		*dst = 0x22222222;
	} else if ((offset == 0x028) || (offset == 0x02C)) {
		*dst = 0x00003800;
	} else if (offset == 0x030) {
		*dst = 0x0F000000;
	} else if (offset == 0x034) {
		*dst = 0x059124C4;
	} else if (
		(((offset >= 0x04C) && (offset <= 0x0D0)) && (offset != 0x084)) ||
		(((offset >= 0x15C) && (offset <= 0x35C)))
		) {
		*dst = 0x00000005;
	} else if (offset > 0x35C) {
		/*
		 * FIXME
		 *
		 * offset from 0x35C are NOT handled!
		 *
		 * FIXME
		 */
		vmm_lerror("Offset 0x%"PRIPADDR" is not handled\n", offset);
		*dst = 0x00000000;
	} else {
		*dst = 0x00000000;
	}

	return VMM_OK;
}

static int imx_iomuxc_emulator_write(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 regmask,
				  u32 regval,
				  u32 size)
{
	/*
	 * Do nothing. The emulator is mostly there to provide default
	 * reset values.
	 */
	return VMM_OK;
}

static int imx_iomuxc_emulator_reset(struct vmm_emudev *edev)
{
	/*
	 * Do nothing. The emulator is mostly there to provide default
	 * reset values.
	 */
	return VMM_OK;
}

static int imx_iomuxc_emulator_probe(struct vmm_guest *guest,
				  struct vmm_emudev *edev,
				  const struct vmm_devtree_nodeid *eid)
{
	/*
	 * Do nothing. The emulator is mostly there to provide default
	 * reset values.
	 */
	return VMM_OK;
}

static int imx_iomuxc_emulator_remove(struct vmm_emudev *edev)
{
	/*
	 * Do nothing. The emulator is mostly there to provide default
	 * reset values.
	 */
	return VMM_OK;
}

static struct vmm_devtree_nodeid imx_iomuxc_emuid_table[] = {
	{ .type = "misc",
	  .compatible = "fsl,imx6q-iomuxc",
	},
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(imx_iomuxc_emulator,
				"imx_iomuxc",
				imx_iomuxc_emuid_table,
				VMM_DEVEMU_LITTLE_ENDIAN,
				imx_iomuxc_emulator_probe,
				imx_iomuxc_emulator_remove,
				imx_iomuxc_emulator_reset,
				imx_iomuxc_emulator_read,
				imx_iomuxc_emulator_write);

static int __init imx_iomuxc_emulator_init(void)
{
	return vmm_devemu_register_emulator(&imx_iomuxc_emulator);
}

static void __exit imx_iomuxc_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&imx_iomuxc_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
