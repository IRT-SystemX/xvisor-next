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
 * @file dedicated.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief Total Dedicated Pass-through Emulator. Hardware is dedicated to a guest.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>

#define MODULE_DESC			"Dedicated Pass-through Emulator"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			pt_dedicated_emulator_init
#define	MODULE_EXIT			pt_dedicated_emulator_exit

#define _pt_dedicated_WRITE(emudev_, offset_, value_)					\
	(vmm_host_memory_write((emudev_)->reg->hphys_addr + (offset_),			\
			       &(value_), sizeof(value_), false) == sizeof(value_))	\
	? VMM_OK : VMM_EFAIL								\

#define _pt_dedicated_READ(emudev_, offset_, ret_)					\
	(vmm_host_memory_read((emudev_)->reg->hphys_addr + (offset_),			\
			      (ret_), sizeof(*(ret_)), false) == sizeof(*(ret_)))	\
	? VMM_OK : VMM_EFAIL								\



static int pt_dedicated_emulator_read8(struct vmm_emudev	*edev,
		physical_addr_t				 offset,
		u8					*dst)
{
	return _pt_dedicated_READ(edev, offset, dst);
}

static int pt_dedicated_emulator_read16(struct vmm_emudev	*edev,
		physical_addr_t					 offset,
		u16						*dst)
{
	return _pt_dedicated_READ(edev, offset, dst);
}

static int pt_dedicated_emulator_read32(struct vmm_emudev *edev,
		physical_addr_t				  offset,
		u32					 *dst)
{
	return _pt_dedicated_READ(edev, offset, dst);
}

static int pt_dedicated_emulator_write8(struct vmm_emudev *edev,
		physical_addr_t				  offset,
		u8					  src)
{
	return _pt_dedicated_WRITE(edev, offset, src);
}

static int pt_dedicated_emulator_write16(struct vmm_emudev	*edev,
		physical_addr_t					 offset,
		u16						 src)
{
	return _pt_dedicated_WRITE(edev, offset, src);
}

static int pt_dedicated_emulator_write32(struct vmm_emudev	*edev,
		physical_addr_t					 offset,
		u32						 src)
{
	return _pt_dedicated_WRITE(edev, offset, src);
}

static int pt_dedicated_emulator_reset(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static int pt_dedicated_emulator_probe(struct vmm_guest *guest,
		struct vmm_emudev *edev,
		const struct vmm_devtree_nodeid *eid)
{
	edev->priv = NULL;
	vmm_printf("##### PROBING (%p)\n", edev);

	return VMM_OK;
}

static int pt_dedicated_emulator_remove(struct vmm_emudev *edev)
{
	return VMM_OK;
}

static struct vmm_devtree_nodeid pt_dedicated_emuid_table[] = {
	{
		.type = "pt",
		.compatible = "dedicated",
	},
	{ /* sentinel */ }
};

static struct vmm_emulator pt_dedicated_emulator = {
	.name = "dedicated",
	.match_table = pt_dedicated_emuid_table,
	.endian = VMM_DEVEMU_NATIVE_ENDIAN,
	.probe = pt_dedicated_emulator_probe,
	.read8 = pt_dedicated_emulator_read8,
	.write8 = pt_dedicated_emulator_write8,
	.read16 = pt_dedicated_emulator_read16,
	.write16 = pt_dedicated_emulator_write16,
	.read32 = pt_dedicated_emulator_read32,
	.write32 = pt_dedicated_emulator_write32,
	.reset = pt_dedicated_emulator_reset,
	.remove = pt_dedicated_emulator_remove
};

static int __init pt_dedicated_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pt_dedicated_emulator);
}

static void __exit pt_dedicated_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pt_dedicated_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		MODULE_AUTHOR,
		MODULE_LICENSE,
		MODULE_IPRIORITY,
		MODULE_INIT,
		MODULE_EXIT);
