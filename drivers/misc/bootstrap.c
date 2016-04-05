/**
 * Copyright (c) 2016 Open Wide
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
 * @file bootstrap.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief bootstrap driver
 */

#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_devtree.h>
#include <vmm_devres.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <vmm_devtree.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqdomain.h>
#include <vmm_host_irq.h>
#include <drv/clk.h>
#include <linux/printk.h>

#define MODULE_AUTHOR           "Jean Guyomarc'h"
#define MODULE_LICENSE          "GPL"
#define MODULE_DESC             "Bootstrap"
#define MODULE_IPRIORITY        1
#define MODULE_INIT             bootstrap_init
#define MODULE_EXIT             bootstrap_exit

struct bootstrap_dev {
        u32 *irqs;
        u32 irqs_count;
};

/*
 * This is a mess. Duplicata of vmm_devvtree_irq_get()
 * WTF
 * Mine works.
 */
static int
_irq_get(struct vmm_devtree_node *node,
         u32                     *irq,
         int                     index)
{
        u32 alen;
        const char *aval;

        if (!node || !irq || index < 0) {
                return VMM_EFAIL;
        }

        aval = vmm_devtree_attrval(node, VMM_DEVTREE_INTERRUPTS_ATTR_NAME);
        if (!aval) {
                return VMM_ENOTAVAIL;
        }

        alen = vmm_devtree_attrlen(node, VMM_DEVTREE_INTERRUPTS_ATTR_NAME);
        if (alen <= (index * sizeof(u32))) {
                return VMM_ENOTAVAIL;
        }

        aval += (index * sizeof(u32) * 3) + sizeof(u32); // FIXME
        *irq  = vmm_be32_to_cpu(*((u32 *)aval));
        *irq += 32; // FIXME

        return VMM_OK;

}

static vmm_irq_return_t
_irq(int no, void *dev)
{
        return VMM_IRQ_HANDLED;
}

static int bootstrap_probe(struct vmm_device               *dev,
                           const struct vmm_devtree_nodeid *nid)
{
        int rc = VMM_OK;
        unsigned int i;
        u32 irqs, it;
        struct bootstrap_dev *b;
        char buf[16];

        dev_info(dev, "Probing...\n");
        b = vmm_malloc(sizeof(*b));
        if (!b) {
                dev_crit(dev, "Failed to allocate memory\n");
                return VMM_ENOMEM;
        }

        irqs = vmm_devtree_irq_count(dev->of_node);
        irqs /= 3; // FIXME Sucks
        dev_info(dev, "IRQs: %"PRIu32"\n", irqs);

        b->irqs_count = irqs;
        if (b->irqs_count > 0) {
                b->irqs = vmm_malloc(b->irqs_count * sizeof(*b->irqs));
                if (!b->irqs) {
                        dev_crit(dev, "Failed to allocate memory\n");
                        rc = VMM_ENOMEM;
                        goto fail_free;
                }

                for (it = 0; it < b->irqs_count; ++it) {
                        rc = _irq_get(dev->of_node, &(b->irqs[it]), it);
                        if (VMM_OK != rc) {
                                dev_err(dev, "Failed to get IRQ %"PRIu32"\n", it);
                                goto fail_free;
                        }
                        const u32 i = b->irqs[it];
                        dev_info(dev, "Enabling irq %"PRIu32"\n", i);
                        //snprintf(buf, sizeof(buf), "%s%"PRIu32, dev->name, it);
                        char *name = vmm_malloc(10);
                        name[0] = 'G';
                        name[1] = 'P';
                        name[2] = 'U';
                        name[3] = '\0';
                        rc = vmm_host_irq_register(i, name, _irq, NULL);
                        if (VMM_OK != rc) {
                                dev_crit(dev->of_node, "Failed to register IRQ %d\n", b->irqs[it]);
                                goto fail_free;
                        }
                }
        }
        u32 clocks;
        rc = vmm_devtree_count_clocks(dev->of_node, &clocks);
        if (rc != VMM_OK) {
                dev_err(dev->of_node, "*** no clock\n");
                goto fail_free;
        }
        struct clk *c;
        u32 x;
        char *clk[10];
        vmm_devtree_read_clock_names_array(dev->of_node, clk, clocks);

        for (x = 0; x < clocks; ++x) {
                vmm_printf("[%i/%"PRIu32"] => {%s}\n", x + 1, clocks, clk[x]);
                c = of_clk_get(dev->of_node, x);
                if (!VMM_IS_ERR_OR_NULL(c)) {
                        rc = clk_prepare_enable(c);
                        if (rc != VMM_OK) {
                                dev_crit(dev->of_node, "Fail");
                        }

                }
        }
        
    //    for (x = 0; x < clocks; ++x) {
    //            c = clk_get(dev, clk[x]);
    //            clk_enable(c);
    //    }



//        irqs_len = vmm_devtree_attrlen(dev->of_node, VMM_DEVTREE_INTERRUPTS_ATTR_NAME);
//        if (irqs_len > 0) {
//        }
//        dev_info(dev, "Len is %"PRIu32"\n", irqs_len);
//        rc = vmm_devtree_read_u32_array(dev->of_node,
//                                        VMM_DEVTREE_INTERRUPTS_ATTR_NAME,
//                                        b->irqs, array_size(b->irqs));
//

        return VMM_OK;

fail_free:
        vmm_free(b);
        return rc;
}

static int bootstrap_remove(struct vmm_device *dev)
{
        return VMM_OK;
}


static struct vmm_devtree_nodeid bootstrap_devid_table[] = {
        { .compatible = "bootstrap" },
        { /* end of list */ }
};

static struct vmm_driver bootstrap_driver = {
        .name = "bootstrap",
        .match_table = bootstrap_devid_table,
        .probe = bootstrap_probe,
        .remove = bootstrap_remove,
};

static int __init bootstrap_init(void)
{
        return vmm_devdrv_register_driver(&bootstrap_driver);
}

static void __exit bootstrap_exit(void)
{
        vmm_devdrv_unregister_driver(&bootstrap_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
