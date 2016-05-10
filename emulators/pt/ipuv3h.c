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
 * @file ipuv3.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief ipuv3 Pass-Through Emulator
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_host_io.h>
#include <vmm_host_irqdomain.h>
#include <vmm_modules.h>
#include <vmm_resource.h>
#include <vmm_host_aspace.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_devemu_debug.h>
#include <asm/sizes.h>

#define MODULE_DESC			"ipuv3 Pass-through Emulator"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			ipuv3_emulator_init
#define	MODULE_EXIT			ipuv3_emulator_exit


struct ipuv3_component {
	const char *		name;
	physical_addr_t		offset;
	virtual_size_t		size;
};

static const struct ipuv3_component comp[] = {
	{ "cm",      0x00200000, VMM_PAGE_SIZE },
	{ "idmac",   0x00208000, VMM_PAGE_SIZE },
	{ "dc",      0x00258000, VMM_PAGE_SIZE },
	{ "ic",      0x00220000, VMM_PAGE_SIZE },
	{ "dmfc",    0x00260000, VMM_PAGE_SIZE },
	{ "cpmem",   0x00300000, SZ_128K       },
	{ "srm",     0x00340000, VMM_PAGE_SIZE },
	{ "tpm",     0x00360000, SZ_64K        },
	{ "csi0",    0x00230000, VMM_PAGE_SIZE },
	{ "csi1",    0x00238000, VMM_PAGE_SIZE },
	{ "ic",      0x00220000, VMM_PAGE_SIZE },
	{ "disp0",   0x00240000, VMM_PAGE_SIZE },
	{ "disp1",   0x00248000, VMM_PAGE_SIZE },
	{ "dc_tmpl", 0x00380000, VMM_PAGE_SIZE },
	{ "vdi",     0x00268000, VMM_PAGE_SIZE },
};
#define COMP_COUNT array_size(comp)

struct ipuv3_state {
	virtual_addr_t		iomem[COMP_COUNT];
	u32 irq_count;
	u32 *host_irqs;
	u32 *guest_irqs;
	struct vmm_guest *guest;
};


/*============================================================================*
 *                         Pass-Through Memory Access                         *
 *============================================================================*/

static int ipuv3_iomem_bank_get(const struct ipuv3_state *s,
				physical_addr_t          offset)
{
	int i;
	const struct ipuv3_component *c;
	bool found = FALSE;

	for (i = 0; i < COMP_COUNT; i++) {
		c = &(comp[i]);
		if ((offset >= c->offset) && (offset < c->offset + c->size)) {
			found = TRUE;
			break;
		}
	}
	if (unlikely(!found)) {
		i = -1;
	}

	return i;
}

static int ipuv3_emulator_read(struct vmm_emudev *edev,
                                physical_addr_t offset,
                                u32 *dst,
                                u32 size)
{
        struct ipuv3_state const *const s = edev->priv;
	volatile void *addr;
	int bank;

	bank = ipuv3_iomem_bank_get(s, offset);
	if (bank < 0) {
		vmm_lcritical("%s: offset 0x%"PRIPADDR" is not mapped\n",
			      edev->node->name, offset);
		return VMM_EFAIL;
	}

	addr = (void *)(s->iomem[bank] + offset - comp[bank].offset);
	vmm_lnotice("%s: About to read at offset %"PRIPADDR" (bank %s). Addr: 0x%p\n",
		    edev->node->name, offset, comp[bank].name, addr);
	*dst = vmm_readl(addr);

        return VMM_OK;
}

static int ipuv3_emulator_write(struct vmm_emudev *edev,
                                 physical_addr_t offset,
                                 u32 mask,
                                 u32 val,
                                 u32 size)
{
        struct ipuv3_state const *const s = edev->priv;
	volatile void *addr;
	int bank;

	bank = ipuv3_iomem_bank_get(s, offset);
	if (bank < 0) {
		vmm_lcritical("%s: offset 0x%"PRIPADDR" is not mapped\n",
			      edev->node->name, offset);
		return VMM_EFAIL;
	}

	addr = (void *)(s->iomem[bank] + offset - comp[bank].offset);
	vmm_lnotice("%s: About to write 0x%"PRIx32" at offset %"PRIPADDR" (bank %s). Addr: 0x%p\n",
		    edev->node->name, val, offset, comp[bank].name, addr);
	vmm_writel(val, addr);
        return VMM_OK;
}

static vmm_irq_return_t simple_routed_irq(int irq, void *dev)
{
	int rc;

	struct vmm_emudev *edev = dev;
	struct ipuv3_state *s = edev->priv;

	//	if (vmm_devemu_debug_irq(edev)) {
//	vmm_lwarning("pt/: re-routing IRQ %i\n", irq);
	//	}

	/* Find guest irq */
//	static int yes = 0;
//	if (!yes) {
//		vmm_lemergency("%s() IRQ %u  ",
//			       __func__, irq);
//	}

	rc = vmm_devemu_emulate_irq(s->guest, irq, 0);
	if (rc) {
		vmm_lerror("Failed to emulate IRQ \n");
	}

	rc = vmm_devemu_emulate_irq(s->guest, irq, 1);
	if (rc) {
		vmm_lerror("Failed to emulate IRQ \n");
	}
	vmm_printf("Irq %i handled\n", irq);
//	vmm_printf("... yes\n");


//	vmm_lerror("guest irq is %i\n", guest_irq);

	/* Lower the interrupt level.
	 * This will clear previous interrupt state.
	 */
//	rc = vmm_devemu_emulate_irq(s->guest, guest_irq, 0);
//	if (rc) {
//		vmm_printf("%s: Emulate Guest=%s irq=%d level=0 failed\n",
//			   __func__, s->guest->name, guest_irq);
//	}
//
//	/* Elevate the interrupt level.
//	 * This will force interrupt triggering.
//	 */
//	rc = vmm_devemu_emulate_irq(s->guest, guest_irq, 1);
//	if (rc) {
//		vmm_printf("%s: Emulate Guest=%s irq=%d level=1 failed\n",
//			   __func__, s->guest->name, guest_irq);
//	}
	//return VMM_IRQ_HANDLED;
	return VMM_IRQ_NONE;
}


static int ipuv3_emulator_reset(struct vmm_emudev *edev)
{
	u32 i;
	struct ipuv3_state *s = edev->priv;

//	for (i = 0; i < s->irq_count; i++) {
//		vmm_devemu_map_host2guest_irq(s->guest,
//					      s->guest_irqs[i],
//					      s->host_irqs[i]);
//	}

	return VMM_OK;
}

static int ipuv3_emulator_probe(struct vmm_guest *guest,
				 struct vmm_emudev *edev,
				 const struct vmm_devtree_nodeid *eid)
{

	u32 i, irq_count, irq;
	int rc = VMM_OK;

	struct ipuv3_state *s;
	const physical_addr_t base = edev->reg->gphys_addr;

	s = vmm_zalloc(sizeof(*s));
	if (!s) {
		rc = VMM_ENOMEM;
		return rc;
	}

	s->guest = guest;
	irq_count = vmm_devtree_irq_count(edev->node);
	if (irq_count != 2) {
		vmm_lerror("Two interrupts  are expected\n");
		/* FIXME Dealloc */
		return VMM_EFAIL;
	}

	for (i = 0; i < COMP_COUNT; i++) {
		const physical_addr_t pa = base + comp[i].offset;
		const physical_size_t sz = comp[i].size;

		vmm_request_mem_region(pa, sz, edev->node->name);
		s->iomem[i] = vmm_host_iomap(pa, sz);
		if (!s->iomem[i]) {
			vmm_lerror("%s.%"PRIPADDR": failed to map region 0x%"PRIPADDR" - %"PRISIZE"\n",
				   edev->node->name, base,
				   comp[i].offset, comp[i].size);
			/*
			 * FIXME Dealloc
			 */
			return VMM_EFAIL;
		}
		vmm_lnotice("%s: remapping bank %s: 0x%"PRIPADDR": 0x%p\n",
			    edev->node->name, comp[i].name, pa,
			    (void *)(s->iomem[i]));
	}

	for (i = 0; i < irq_count; i++) {
		rc = vmm_devtree_irq_get(edev->node, &irq, i);
		if (rc) {
			vmm_lerror("Failed to retrieve irq #%"PRIu32"\n", i);
			/* FIXME Dealloc */
			return VMM_EFAIL;
		}
		rc = vmm_host_irq_mark_routed(irq);
		if (rc) {
			vmm_lerror("Failed to mark irq %"PRIu32" as routed\n", irq);
			/* FIXME Dealloc */
			return VMM_EFAIL;
		}
		rc = vmm_host_irq_register(irq, edev->node->name,
					   simple_routed_irq, edev);
		if (rc) {
			vmm_lerror("Failed to register irq %"PRIu32"\n", irq);
			/* FIXME Dealloc */
			return VMM_EFAIL;
		}
	}
	edev->priv = s;

	return rc;
}

static int ipuv3_emulator_remove(struct vmm_emudev *edev)
{
	struct ipuv3_state *s = edev->priv;

	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid ipuv3_emuid_table[] = {
		{ .type = "pt", .compatible = "ipuv3", },
		{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(ipuv3_emulator,
			    "ipuv3",
			    ipuv3_emuid_table,
			    VMM_DEVEMU_NATIVE_ENDIAN,
			    ipuv3_emulator_probe,
			    ipuv3_emulator_remove,
			    ipuv3_emulator_reset,
			    ipuv3_emulator_read,
			    ipuv3_emulator_write);

static int __init ipuv3_emulator_init(void)
{
	return vmm_devemu_register_emulator(&ipuv3_emulator);
}

static void __exit ipuv3_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&ipuv3_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
