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
#include <vmm_host_irq.h>
#include <vmm_devemu.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_mutex.h>
#include <vmm_manager.h>
#include <libs/list.h>
#include <libs/stringlib.h>

#define MODULE_DESC                     "Dedicated Pass-through Emulator"
#define MODULE_AUTHOR                   "Jean Guyomarc'h"
#define MODULE_LICENSE                  "GPL"
#define MODULE_IPRIORITY                0
#define MODULE_INIT                     pt_dedicated_emulator_init
#define MODULE_EXIT                     pt_dedicated_emulator_exit


struct pt_emu_state {
        char                     name[64];
        struct dlist             node;
        struct vmm_emudev       *edev;
        struct vmm_guest        *guest;
        u32                     *irqs;
        u32                      irq_count;
};

static LIST_HEAD(_pt_emu_list);
static DEFINE_MUTEX(_pt_emu_lock);


/*============================================================================*
 *                         Pass-Through Memory Access                         *
 *============================================================================*/

#define _PT_DEDICATED_WRITE(emudev_, offset_, value_)                                   \
        (vmm_host_memory_write((emudev_)->reg->hphys_addr + (offset_),                  \
                               &(value_), sizeof(value_), false) == sizeof(value_))     \
        ? VMM_OK : VMM_EFAIL                                                            \

#define _PT_DEDICATED_READ(emudev_, offset_, ret_)                                      \
        (vmm_host_memory_read((emudev_)->reg->hphys_addr + (offset_),                   \
                              (ret_), sizeof(*(ret_)), false) == sizeof(*(ret_)))       \
        ? VMM_OK : VMM_EFAIL                                                            \

static int pt_dedicated_emulator_read8(struct vmm_emudev *edev,
                                       physical_addr_t    offset,
                                       u8                *dst)
{
        return _PT_DEDICATED_READ(edev, offset, dst);
}

static int pt_dedicated_emulator_read16(struct vmm_emudev *edev,
                                        physical_addr_t    offset,
                                        u16               *dst)
{
        return _PT_DEDICATED_READ(edev, offset, dst);
}

static int pt_dedicated_emulator_read32(struct vmm_emudev *edev,
                                        physical_addr_t    offset,
                                        u32               *dst)
{
        return _PT_DEDICATED_READ(edev, offset, dst);
}

static int pt_dedicated_emulator_write8(struct vmm_emudev *edev,
                                        physical_addr_t    offset,
                                        u8                 src)
{
        return _PT_DEDICATED_WRITE(edev, offset, src);
}

static int pt_dedicated_emulator_write16(struct vmm_emudev *edev,
                                         physical_addr_t    offset,
                                         u16                src)
{
        return _PT_DEDICATED_WRITE(edev, offset, src);
}

static int pt_dedicated_emulator_write32(struct vmm_emudev *edev,
                                         physical_addr_t    offset,
                                         u32                src)
{
        return _PT_DEDICATED_WRITE(edev, offset, src);
}


static int pt_dedicated_emulator_reset(struct vmm_emudev *edev)
{
        int ret = VMM_OK;
        u32 i;
        struct pt_emu_state *s = edev->priv;

        for (i = 0; i < s->irq_count; ++i) {
                ret = vmm_devemu_map_host2guest_irq(s->guest,
                                                    s->irqs[i],
                                                    s->irqs[i]);
                if (VMM_OK != ret) {
                        break;
                }
        }

        return ret;
}

static vmm_irq_return_t _routed_irq(int irq, void *priv)
{
        const struct pt_emu_state *s = priv;
        int ret;

        vmm_printf("-- Emulating IRQ %i\n", irq);
	ret = vmm_devemu_emulate_irq(s->guest, irq, 0);
	if (VMM_OK != ret) {
		vmm_printf("*** Fail to emulate irq %i (LOW)\n", irq);
                goto done;
        }

	ret = vmm_devemu_emulate_irq(s->guest, irq, 1);
	if (VMM_OK != ret) {
		vmm_printf("*** Fail to emulate irq %i (HIGH)\n", irq);
        }

done:
	return VMM_IRQ_HANDLED;
}

static int pt_dedicated_emulator_probe(struct vmm_guest                *guest,
                                       struct vmm_emudev               *edev,
                                       const struct vmm_devtree_nodeid *eid)
{
        struct pt_emu_state *s, *s_it;
        //const struct vmm_emudev *edev_it;
        int ret = VMM_OK;
        u32 i;

        vmm_printf("##### PROBING (%p)\n", edev);

        s = vmm_zalloc(sizeof(*s));
        if (NULL == s) {
                ret = VMM_ENOMEM;
                goto end;
        }
        INIT_LIST_HEAD(&s->node);

        vmm_snprintf(s->name, sizeof(s->name),
                     "%s/%s", guest->name, edev->node->name);
        vmm_printf("name is \"%s\"\n", s->name);

        vmm_mutex_lock(&_pt_emu_lock);
        list_for_each_entry(s_it, &_pt_emu_list, node) {
                //edev_it = s_it->edev;
                // TODO Check it is not already registered
                // ERR: vmm_mutex_unlock(&_pt_emu_lock);
                // ERR: goto fail;
        }
        list_add_tail(&s->node, &_pt_emu_list);
        vmm_mutex_unlock(&_pt_emu_lock);

        s->edev = edev;
        s->guest = guest;
        s->irq_count = vmm_devtree_irq_count(edev->node);
        vmm_printf("[%s] IRQ Count: %i\n", edev->emu->name, s->irq_count);

        if (s->irq_count) {
                s->irqs = vmm_malloc(sizeof(u32) * s->irq_count);
                // TODO Check returns
        }

        for (i = 0; i < s->irq_count; ++i) {
                ret = vmm_devtree_irq_get(edev->node, &(s->irqs[i]), i);
                vmm_printf("[%s] get IRQ: %i\n", edev->emu->name, s->irqs[i]);
                if (VMM_OK != ret) {
                        vmm_printf("*** Failed to get interrupt %i\n", i);
                }
                ret = vmm_host_irq_mark_routed(s->irqs[i]);
                if (VMM_OK != ret) {
                        vmm_printf("*** Failed to get mark IRQ %i as routed\n", i);
                }
                ret = vmm_host_irq_register(s->irqs[i], "passthrough",
                                            _routed_irq, s);
                if (VMM_OK != ret) {
                        vmm_printf("*** Failed to register IRQ %i\n", i);
                }
        }


        edev->priv = s;
        return ret;
//fail:
//        vmm_free(s);
end:
        return ret;
}

static int pt_dedicated_emulator_remove(struct vmm_emudev *edev)
{
        struct pt_emu_state *s = edev->priv;
        u32 i;

        vmm_mutex_lock(&_pt_emu_lock);
        list_del(&s->node);
        vmm_mutex_unlock(&_pt_emu_lock);

        for (i = 0; i < s->irq_count; i++) {
		vmm_host_irq_unregister(s->irqs[i], s);
		vmm_host_irq_unmark_routed(s->irqs[i]);
        }
        if (s->irqs) {
                vmm_free(s->irqs);
        }
        vmm_free(s);
        edev->priv = NULL;

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
