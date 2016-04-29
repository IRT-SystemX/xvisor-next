/**
 * Copyright (c) 2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
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
 * @file memsmart.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief Smart Memory Passthrough Emulator
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_modules.h>
#include <vmm_host_aspace.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_devemu_debug.h>

#define MODULE_DESC			"Memsmart Passthrough Emulator"
#define MODULE_AUTHOR			"Memsmart"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define MODULE_INIT			memsmart_emulator_init
#define MODULE_EXIT			memsmart_emulator_exit

struct memsmart_state {
	char name[64];
	struct vmm_guest *guest;
	u32 irq_count;
	u32 *host_irqs;
	u32 *guest_irqs;
        u32 memory_pt;
};


/*============================================================================*
 *                         Pass-Through Memory Access                         *
 *============================================================================*/

static int memsmart_emulator_read(struct vmm_emudev *edev,
                                physical_addr_t offset,
                                u32 *dst,
                                u32 size)
{
        const struct memsmart_state *s = edev->priv;
        int ret;

        /* Ignore if memoty pt READ not enabled */
        if (!(s->memory_pt & MEMORY_PT_READ)) {
                return VMM_OK;
        }

        ret = vmm_host_memory_read(edev->reg->hphys_addr + offset,
                                   dst, size, false);

        return (ret == size) ? VMM_OK : VMM_EFAIL;
}

static int memsmart_emulator_write(struct vmm_emudev *edev,
                                 physical_addr_t offset,
                                 u32 val,
                                 u32 mask,
                                 u32 size)
{
        const struct memsmart_state *s = edev->priv;
        int ret;

        /* Ignore if memoty pt WRITE not enabled */
        if (!(s->memory_pt & MEMORY_PT_WRITE)) {
                return VMM_OK;
        }

        ret = vmm_host_memory_write(edev->reg->hphys_addr + offset,
                                    &val, size, false);

        return (ret == size) ? VMM_OK : VMM_EFAIL;
}

static int memsmart_emulator_reset(struct vmm_emudev *edev)
{
	struct memsmart_state *s = edev->priv;
	return VMM_OK;
}

static int memsmart_emulator_probe(struct vmm_guest *guest,
				 struct vmm_emudev *edev,
				 const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	struct memsmart_state *s;

	s = vmm_zalloc(sizeof(struct memsmart_state));
	if (!s) {
		rc = VMM_ENOMEM;
		goto end;
	}

	edev->priv = s;
end:
	return rc;
}

static int memsmart_emulator_remove(struct vmm_emudev *edev)
{
	struct memsmart_state *s = edev->priv;
	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid memsmart_emuid_table[] = {
	{ .type = "pt", .compatible = "memsmart", },
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_memsmart(memsmart_emulator,
                            "memsmart",
                            memsmart_emuid_table,
                            VMM_DEVEMU_NATIVE_ENDIAN,
                            memsmart_emulator_probe,
                            memsmart_emulator_remove,
                            memsmart_emulator_reset,
                            memsmart_emulator_read,
                            memsmart_emulator_write);

static int __init memsmart_emulator_init(void)
{
	return vmm_devemu_register_emulator(&memsmart_emulator);
}

static void __exit memsmart_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&memsmart_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
