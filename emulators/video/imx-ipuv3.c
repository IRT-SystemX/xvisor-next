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
 * @file imx-ipuv3.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief IMX IPU Emulator
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_host_aspace.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>

#define MODULE_DESC			"Passthrough IMX IPU Emulator"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define MODULE_INIT			imx_ipuv3_emulator_init
#define MODULE_EXIT			imx_ipuv3_emulator_exit


static int imx_ipuv3_emulator_read8(struct vmm_emudev	*edev,
		physical_addr_t				 offset,
		u8					*dst)
{
	return VMM_ENOSYS;
}

static int imx_ipuv3_emulator_read16(struct vmm_emudev	*edev,
		physical_addr_t				 offset,
		u16					*dst)
{
	return VMM_ENOSYS;
}

static int imx_ipuv3_emulator_read32(struct vmm_emudev  *edev,
		physical_addr_t				 offset,
		u32					*dst)
{
	const unsigned int len = sizeof(*dst);
	return (vmm_host_memory_read(edev->reg->hphys_addr + offset,
			dst, len, false) == len)
		? VMM_OK
		: VMM_EFAIL;
}

static int imx_ipuv3_emulator_write8(struct vmm_emudev	*edev,
		physical_addr_t				 offset,
		u8					 src)
{
	return VMM_ENOSYS;
}

static int imx_ipuv3_emulator_write16(struct vmm_emudev	*edev,
		physical_addr_t				 offset,
		u16					 src)
{
	return VMM_ENOSYS;
}

static int imx_ipuv3_emulator_write32(struct vmm_emudev	*edev,
		physical_addr_t				 offset,
		u32					 src)
{
	const unsigned int len = sizeof(src);
	return (vmm_host_memory_write(edev->reg->hphys_addr + offset,
			&src, len, false) == len)
		? VMM_OK
		: VMM_EFAIL;
}

static int imx_ipuv3_emulator_reset(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static int imx_ipuv3_emulator_probe(struct vmm_guest *guest,
		struct vmm_emudev *edev,
		const struct vmm_devtree_nodeid *eid)
{
	edev->priv = NULL;
	vmm_printf("imx-ipuv3: probing ipu 0x%p by guest 0x%p\n", edev, guest);
	return VMM_OK;
}

static int imx_ipuv3_emulator_remove(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static struct vmm_devtree_nodeid imx_ipuv3_emuid_table[] = {
	{
		.type = "video",
		.compatible = "imx-ipuv3",
	},
	{ /* sentinel */ }
};

static struct vmm_emulator imx_ipuv3_emulator = {
	.name = "imx-ipuv3",
	.match_table = imx_ipuv3_emuid_table,
	.endian = VMM_DEVEMU_NATIVE_ENDIAN,
	.probe = imx_ipuv3_emulator_probe,
	.read8 = imx_ipuv3_emulator_read8,
	.write8 = imx_ipuv3_emulator_write8,
	.read16 = imx_ipuv3_emulator_read16,
	.write16 = imx_ipuv3_emulator_write16,
	.read32 = imx_ipuv3_emulator_read32,
	.write32 = imx_ipuv3_emulator_write32,
	.reset = imx_ipuv3_emulator_reset,
	.remove = imx_ipuv3_emulator_remove,
};

static int __init imx_ipuv3_emulator_init(void)
{
	return vmm_devemu_register_emulator(&imx_ipuv3_emulator);
}

static void __exit imx_ipuv3_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&imx_ipuv3_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		MODULE_AUTHOR,
		MODULE_LICENSE,
		MODULE_IPRIORITY,
		MODULE_INIT,
		MODULE_EXIT);
