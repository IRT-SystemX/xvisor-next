/**
 * Copyright (c) 2015 Anup Patel.
 *               2016 Open Wide
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
 * @file simple.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Simple pass-through emulator.
 *
 * This emulator to should be use for pass-through access to non-DMA
 * capable device which do not require IOMMU, CLK, and PINMUX configuration.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_host_io.h>
#include <vmm_host_irqdomain.h>
#include <vmm_modules.h>
#include <vmm_host_aspace.h>
#include <vmm_resource.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_devemu_debug.h>

#define MODULE_DESC			"Simple Pass-through Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			simple_emulator_init
#define	MODULE_EXIT			simple_emulator_exit

#define WHITELIST_ATTR		"whitelist"
#define MEMORY_PASSTHROUGH_ATTR "memory-passthrough"

enum memory_pt {
        MEMORY_PT_NONE       = 0,
        MEMORY_PT_READ       = (1 << 0),
        MEMORY_PT_WRITE      = (1 << 1),
        MEMORY_PT_READ_WRITE = (MEMORY_PT_READ | MEMORY_PT_WRITE),

        __MEMORY_PT_LAST /* Sentinel */
};

struct xlist_item {
	u32		offset;
	u32		mask;
};

struct simple_state {
	char name[64];
	struct vmm_guest *guest;
	virtual_addr_t iomem;
	u32 irq_count;
	u32 *host_irqs;
	u32 *guest_irqs;
        u32 memory_pt;

	struct xlist_item *xlist;
	u32 xlist_len;
	u8 whitelist;
};


/*============================================================================*
 *                         Pass-Through Memory Access                         *
 *============================================================================*/

static int simple_emulator_read(struct vmm_emudev *edev,
                                physical_addr_t offset,
                                u32 *dst,
                                u32 size)
{
        struct simple_state const *const s = edev->priv;
	void *addr = (void *)(s->iomem + offset);

        /* Ignore if memoty pt READ not enabled */
        if (!(s->memory_pt & MEMORY_PT_READ)) {
                return VMM_OK;
        }

	switch (size) {
	case 1:
		*dst = vmm_readb(addr);
		break;
	case 2:
		*dst = vmm_readw(addr);
		break;
	case 4:
		*dst = vmm_readl(addr);
		break;
	default:
		vmm_lerror("Unhandled size %"PRIu32"\n", size);
		return VMM_EFAIL;
	}
        return VMM_OK;
}

static int simple_emulator_write(struct vmm_emudev *edev,
                                 physical_addr_t offset,
                                 u32 mask,
                                 u32 val,
                                 u32 size)
{
        const struct simple_state *s = edev->priv;
	void *addr = (void *)(s->iomem + offset);
        int ret = VMM_OK;
	u32 oldval;
	u32 i;
	bool write = (s->whitelist) ? FALSE : TRUE;
	u32 reg;

        /* Ignore if memoty pt WRITE not enabled */
        if (!(s->memory_pt & MEMORY_PT_WRITE)) {
                return VMM_OK;
        }

	oldval = val;
	if (s->xlist && s->whitelist) {
		for (i = 0; i < s->xlist_len; i++) {
			if (offset == s->xlist[i].offset) {
				write = TRUE;
				ret = simple_emulator_read(edev, offset, &reg, size);
				if (ret != VMM_OK) {
					goto end;
				}
				val = (reg & ~s->xlist[i].mask) | (val & s->xlist[i].mask);

				if (vmm_devemu_get_debug_info(edev) & (1 << 8)) {
					vmm_lnotice("%s (pass-through): 0x%"PRIx32" (query=0x%"PRIx32", oldreal=0x%"PRIx32", mask=0x%"PRIx32") => %"PRIPADDR"\n"
						  ,
						  edev->node->name,
						  val, oldval, reg, s->xlist[i].mask,
						  offset);
				}
				break;
			}
		}
	}

	if (write) {
		if (val != oldval) {
			vmm_lalert("%s (pass-through): 0x%"PRIx32" (query=0x%"PRIx32", oldreal=0x%"PRIx32", mask=0x%"PRIx32") => %"PRIPADDR"\n"
				   ,
				   edev->node->name,
				   val, oldval, reg, s->xlist[i].mask,
				   offset);
		}
		//vmm_lwarning("%s: About to write (PT) to offset %"PRIPADDR"\n", edev->node->name, offset);
		switch (size) {
			case 1:
				vmm_writeb(val & 0xff, addr);
				break;
			case 2:
				vmm_writew(val & 0xffff, addr);
				break;
			case 4:
				vmm_writel(val & 0xffffffff, addr);
				break;
			default:
				vmm_lerror("Unhandled size %"PRIu32"\n", size);
				return VMM_EFAIL;
		}

		return VMM_OK;
//		ret = vmm_host_memory_write(edev->reg->hphys_addr + offset,
//					    &val, size, false);
//		return (ret == size) ? VMM_OK : VMM_EFAIL;
	} else {
		//	if (vmm_devemu_debug_write(edev)) {
		vmm_lerror("%s (pt) write denied at %"PRIPADDR" (0x%"PRIx32")\n",
			   edev->node->name, offset, oldval);
		//	}
	}
end:
	return ret;
}

/* Handle host-to-guest routed IRQ generated by device */
static vmm_irq_return_t simple_routed_irq(int irq, void *dev)
{
	int rc;

	bool found = FALSE;
	u32 i, host_irq = irq, guest_irq = 0;
	struct vmm_emudev *edev = dev;
	struct simple_state *s = edev->priv;

	vmm_host_irq_mask(irq);
	vmm_lcritical("(pt/simple) mask %i\n", irq);

	/* Lower the interrupt level.
	 * This will clear previous interrupt state.
	 */
	rc = vmm_devemu_emulate_irq(s->guest, irq, 0);
	if (rc) {
		vmm_lerror("Failed to emulate IRQ \n");
		goto err;
	}

	/* Elevate the interrupt level.
	 * This will force interrupt triggering.
	 */
	rc = vmm_devemu_emulate_irq(s->guest, guest_irq, 1);
	if (rc) {
		vmm_lerror("Failed to emulate IRQ \n");
		goto err;
	}

done:
	return VMM_IRQ_HANDLED;
err:
	vmm_host_irq_unmask(irq);
	return VMM_IRQ_HANDLED;
}

static int simple_emulator_reset(struct vmm_emudev *edev)
{
	u32 i;
	struct simple_state *s = edev->priv;

#if 0
	for (i = 0; i < s->irq_count; i++) {
		vmm_devemu_map_host2guest_irq(s->guest,
					      s->guest_irqs[i],
					      s->host_irqs[i]);
	}
#endif

	return VMM_OK;
}

static int simple_emulator_probe(struct vmm_guest *guest,
				 struct vmm_emudev *edev,
				 const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	u32 i, irq_reg_count = 0;
	struct simple_state *s;

	s = vmm_zalloc(sizeof(struct simple_state));
	if (!s) {
		rc = VMM_ENOMEM;
		goto simple_emulator_probe_fail;
	}

	strlcpy(s->name, guest->name, sizeof(s->name));
	strlcat(s->name, "/", sizeof(s->name));
	if (strlcat(s->name, edev->node->name, sizeof(s->name)) >=
	    sizeof(s->name)) {
		rc = VMM_EOVERFLOW;
		goto simple_emulator_probe_freestate_fail;
	}

	s->guest = guest;
	s->irq_count = vmm_devtree_irq_count(edev->node);
	s->guest_irqs = NULL;
	s->host_irqs = NULL;
	s->memory_pt = MEMORY_PT_NONE;

	/* Is it a memory pass-though? */
	i = vmm_devtree_attrlen(edev->node, MEMORY_PASSTHROUGH_ATTR) / sizeof(u32);
	if (i > 0) {
		rc = vmm_devtree_read_u32_atindex(edev->node,
						  MEMORY_PASSTHROUGH_ATTR,
						  &s->memory_pt, 0);
		if (s->memory_pt >= __MEMORY_PT_LAST) {
			rc = VMM_EINVALID;
			vmm_printf("*** Invalid "MEMORY_PASSTHROUGH_ATTR" value: "
				   "0x%"PRIx32"\n", s->memory_pt);
			goto simple_emulator_probe_freestate_fail;
		}
	}


	const physical_size_t sz = VMM_PAGE_SIZE;
	const physical_addr_t pa = edev->reg->gphys_addr;

		vmm_lwarning("iomap(0x%"PRIPADDR" - %"PRISIZE")\n",
			   pa, sz);
	vmm_request_mem_region(pa, sz, edev->node->name);	
	s->iomem = vmm_host_iomap(pa, sz);
	if (!s->iomem) {
		vmm_lerror("Failed to iomap(0x%"PRIPADDR" - %"PRISIZE")\n",
			   pa, sz);
		goto simple_emulator_probe_freestate_fail;
	}


	s->whitelist = 0;
	i = vmm_devtree_attrlen(edev->node, WHITELIST_ATTR) / (sizeof(u32) * 2);
	if (i > 0) {
		s->whitelist = 1;
		s->xlist_len = i;
		s->xlist = vmm_malloc(sizeof(struct xlist_item) * s->xlist_len);
		rc = vmm_devtree_read_u32_array(edev->node, WHITELIST_ATTR,
						(u32 *)s->xlist, s->xlist_len * 2);
//		for (i = 0; i < s->xlist_len; i++) {
//			vmm_lnotice("iomuxc: %x -> %x\n", s->xlist[i].offset, s->xlist[i].mask);
//		}
	}


#if 0

	i = vmm_devtree_attrlen(edev->node, "host-interrupts") / sizeof(u32);
	if (s->irq_count != i) {
		rc = VMM_EINVALID;
		goto simple_emulator_probe_freestate_fail;
	}

	if (s->irq_count) {
		s->host_irqs = vmm_zalloc(sizeof(u32) * s->irq_count);
		if (!s->host_irqs) {
			rc = VMM_ENOMEM;
			goto simple_emulator_probe_freestate_fail;
		}

		s->guest_irqs = vmm_zalloc(sizeof(u32) * s->irq_count);
		if (!s->guest_irqs) {
			rc = VMM_ENOMEM;
			goto simple_emulator_probe_freehirqs_fail;
		}
	}

	for (i = 0; i < s->irq_count; i++) {
		rc = vmm_devtree_read_u32_atindex(edev->node,
						  "host-interrupts",
						  &s->host_irqs[i], i);
		if (rc) {
			goto simple_emulator_probe_cleanupirqs_fail;
		}

		rc = vmm_devtree_irq_get(edev->node, &s->guest_irqs[i], i);
		if (rc) {
			goto simple_emulator_probe_cleanupirqs_fail;
		}

		vmm_printf("%s: binding guest IRQ %"PRIu32" to host IRQ %"PRIu32"\n",
			   s->name, s->guest_irqs[i], s->host_irqs[i]);

		//    vmm_host_irq_enable(s->host_irqs[i]);

		//		rc = vmm_host_irq_mark_routed(s->host_irqs[i]);
		//		if (rc) {
		//			goto simple_emulator_probe_cleanupirqs_fail;
		//		}

		struct vmm_host_irqdomain *dom = vmm_host_irqdomain_get(s->guest_irqs[i]);
		const u32 tab[3] = { 0x0, s->host_irqs[i], 0x4 };
		unsigned int type;
		unsigned long hwirq;

		rc = vmm_host_irqdomain_xlate(dom, tab, 3, &hwirq, &type);
		if (rc) {
			vmm_lalert("Fail to xlate\n");
		}
		vmm_lwarning("hwirq = %lu\n", hwirq);
		rc = vmm_host_irqdomain_create_mapping(dom, hwirq);
		if (rc < 0) {
			vmm_lalert("Fail to create mappping (%i)\n", rc);
		}
		u32 hirq = rc;
		vmm_host_irq_set_type(hirq, type);

		rc = vmm_host_irq_register(hirq, s->name,
					   simple_routed_irq, edev);
		if (rc) {
			//			vmm_host_irq_unmark_routed(s->host_irqs[i]);
			goto simple_emulator_probe_cleanupirqs_fail;
		}

		irq_reg_count++;
	}
#endif

#if 1

	u32 irq;
	for (i = 0; i < s->irq_count; i++) {
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

		vmm_lalert("IRQ %u enabled\n", irq);
	}
#endif


	vmm_printf("%s: simple/pt. Memory flags: 0x%"PRIx32"\n",
		   s->name, s->memory_pt);
	edev->priv = s;

	return VMM_OK;

simple_emulator_probe_cleanupirqs_fail:
	for (i = 0; i < irq_reg_count; i++) {
		vmm_host_irq_unregister(s->host_irqs[i], s);
		vmm_host_irq_unmark_routed(s->host_irqs[i]);
	}
	if (s->guest_irqs) {
		vmm_free(s->guest_irqs);
	}
simple_emulator_probe_freehirqs_fail:
	if (s->host_irqs) {
		vmm_free(s->host_irqs);
	}
simple_emulator_probe_freestate_fail:
	vmm_free(s);
simple_emulator_probe_fail:
	return rc;
}

static int simple_emulator_remove(struct vmm_emudev *edev)
{
	u32 i;
	struct simple_state *s = edev->priv;

	if (!s) {
		return VMM_EFAIL;
	}

	for (i = 0; i < s->irq_count; i++) {
		vmm_host_irq_unregister(s->host_irqs[i], s);
		vmm_host_irq_unmark_routed(s->host_irqs[i]);
	}
	if (s->guest_irqs) {
		vmm_free(s->guest_irqs);
	}
	if (s->host_irqs) {
		vmm_free(s->host_irqs);
	}
	vmm_free(s);

	vmm_printf("======= REMOVING %p %p\n", edev, edev->priv);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid simple_emuid_table[] = {
		{ .type = "pt", .compatible = "simple", },
		{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(simple_emulator,
			    "simple",
			    simple_emuid_table,
			    VMM_DEVEMU_NATIVE_ENDIAN,
			    simple_emulator_probe,
			    simple_emulator_remove,
			    simple_emulator_reset,
			    simple_emulator_read,
			    simple_emulator_write);

static int __init simple_emulator_init(void)
{
	return vmm_devemu_register_emulator(&simple_emulator);
}

static void __exit simple_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&simple_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
