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
 * @file gpc.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief Generic Power Controller Emulator
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <vmm_macros.h>
#include <vmm_devemu_debug.h>

#include <imx-common.h>

#define MODULE_DESC			"i.MX6 GPC Pass-Through Emulator"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define MODULE_INIT			gpc_emulator_init
#define MODULE_EXIT			gpc_emulator_exit

#define DT_CNTR_MASK_ATTR_NAME		"cntr-mask"
#define DT_IMR_MASKS_ATTR_NAME		"imr-masks"
#define DT_PGCR_MASK_ATTR_NAME		"pgcr-mask"
#define DT_PGSR_MASK_ATTR_NAME		"pgsr-mask"
#define DT_PUPSCR_MASK_ATTR_NAME	"pupscr-mask"
#define DT_PDNSCR_MASK_ATTR_NAME	"pdnscr-mask"

enum gpc_type {
	GPC_TYPE_GPC,
	GPC_TYPE_PGC_GPU,
	GPC_TYPE_PGC_CPU,
};

struct gpc_state {
	u32 gpc[10];
	u32 pgc_gpu[4];
	u32 pgc_cpu[4];
	vmm_spinlock_t lock;

	u32 cntr_mask;
	u32 imr_mask[4];
	u32 pgcr_mask;
	u32 pupscr_mask;
	u32 pdnscr_mask;
	u32 pgsr_mask;
};

static inline void _lock(struct gpc_state *s)
{
	vmm_spin_lock(&s->lock);
}

static inline void _unlock(struct gpc_state *s)
{
	vmm_spin_unlock(&s->lock);
}

static u32 *_regs_get(struct gpc_state *s, physical_addr_t offset,
			unsigned int *idx, enum gpc_type *type,
			u32 *mask)
{
	u32 *regs;
	u32 i = 0;
	u32 msk = 0;

	if (offset <= 0x024) {
		i = offset >> 2;
		regs = s->gpc;
		*type = GPC_TYPE_GPC;
		if (i == 0) {
			msk = s->cntr_mask;
		}  else if ((i >= 2) && (i <= 5)) {
			msk = s->imr_mask[i - 2];
		}
	} else if ((offset >= 0x260) && (offset <= 0x26C)) {
		i = (offset - 0x260) >> 2;
		regs = s->pgc_gpu;
		*type = GPC_TYPE_PGC_GPU;
		switch (i) {
		case 0: msk = s->pgcr_mask; break;
		case 1: msk = s->pupscr_mask; break;
		case 2: msk = s->pdnscr_mask; break;
		case 3: msk = s->pgsr_mask; break;
		}
	} else if ((offset >= 0x2A0) && (offset <= 0x2AC)) {
		i = (offset - 0x2A0) >> 2;
		regs = s->pgc_cpu;
		*type = GPC_TYPE_PGC_CPU;
	} else {
		regs = NULL;
	}

	*mask = msk;
	*idx = i;

	return regs;
}

static int gpc_emulator_read(struct vmm_emudev *edev,
				physical_addr_t offset,
				u32 *dst,
				u32 size)
{
	struct gpc_state *s = edev->priv;
	unsigned int idx;
	u32 *regs;
	u32 msk = 0, hw_reg;
	enum gpc_type type;

	regs = _regs_get(s, offset, &idx, &type, &msk);
	if (NULL == regs) {
		vmm_lwarning("GPC: unhandled memory region: 0x%"PRIPADDR"\n", offset);
		goto end;
	}

	if (msk != 0) {
		hw_reg = imx_gpc_read_reg(offset);
		*dst = hw_reg & msk;
	} else {
		_lock(s);
		*dst = regs[idx];
		_unlock(s);
	}

end:
	return VMM_OK;
}

static int gpc_emulator_write(struct vmm_emudev *edev,
				physical_addr_t offset,
				u32 regval,
				u32 mask,
				u32 size)
{
	struct gpc_state *s = edev->priv;
	u32 *regs;
	u32 hw_reg, msk = 0;
	enum gpc_type type;
	unsigned int idx;

	regs = _regs_get(s, offset, &idx, &type, &msk);
	if (NULL == regs)  {
		vmm_lwarning("GPC: unhandled memory region: 0x%"PRIPADDR"\n", offset);
		goto end;
	}

	/* Pass-through */
	if (msk != 0) {
		hw_reg = imx_gpc_read_reg(offset);
		hw_reg &= ~msk;
		hw_reg |= (regval & msk);
		imx_gpc_write_reg(offset, hw_reg);
		regval = hw_reg;

		if (vmm_devemu_debug_write(edev)) {
			vmm_linfo("gpc: pass-through: 0x%"PRIx32"\n", hw_reg);
		}
	}

	_lock(s);
	regs[idx] = regval;
	_unlock(s);

end:
	return VMM_OK;
}

static void _reset(struct gpc_state *s)
{
	unsigned int i;

	s->gpc[0] = 0x00100000;
	for (i = 1; i < array_size(s->gpc); ++i) {
		s->gpc[i] = 0x00000000;
	}
}

static int gpc_emulator_reset(struct vmm_emudev *edev)
{
	struct gpc_state *s = edev->priv;

	_lock(s);
	_reset(s);
	_unlock(s);

	return VMM_OK;
}

static int gpc_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	struct gpc_state *s;
	int rc = VMM_OK, ret;

	s = vmm_zalloc(sizeof(*s));
	if (NULL == s) {
		vmm_lerror("gpc: failed to allocate memory");
		rc = VMM_ENOMEM;
		goto end;
	}

	/* Get CNTR mask */
	ret = vmm_devtree_read_u32_atindex(edev->node, DT_CNTR_MASK_ATTR_NAME,
						&s->cntr_mask, 0);
	if (VMM_OK != ret) {
		s->cntr_mask = 0;
	}

	/* Get IMR masks */
	ret = vmm_devtree_read_u32_array(edev->node, DT_IMR_MASKS_ATTR_NAME,
						s->imr_mask, array_size(s->imr_mask));
	if (VMM_OK != ret) {
		memset(s->imr_mask, 0, sizeof(s->imr_mask));
	}

	/* Get PGCR mask */
	ret = vmm_devtree_read_u32_atindex(edev->node, DT_PGCR_MASK_ATTR_NAME,
						&s->pgcr_mask, 0);
	if (VMM_OK != ret) {
		s->pgcr_mask = 0;
	}

	/* Get PGSR mask */
	ret = vmm_devtree_read_u32_atindex(edev->node, DT_PGSR_MASK_ATTR_NAME,
						&s->pgsr_mask, 0);
	if (VMM_OK != ret) {
		s->pgsr_mask = 0;
	}

	/* Get PUP_SCR mask */
	ret = vmm_devtree_read_u32_atindex(edev->node, DT_PUPSCR_MASK_ATTR_NAME,
						&s->pupscr_mask, 0);
	if (VMM_OK != ret) {
		s->pupscr_mask = 0;
	}

	/* Get PND_SCR mask */
	ret = vmm_devtree_read_u32_atindex(edev->node, DT_PDNSCR_MASK_ATTR_NAME,
						&s->pdnscr_mask, 0);
	if (VMM_OK != ret) {
		s->pdnscr_mask = 0;
	}

	_reset(s);
	INIT_SPIN_LOCK(&(s->lock));
	edev->priv = s;

	if (vmm_devemu_debug_parsed_params(edev)) {
		vmm_linfo("imx-gpc: cntr-mask = 0x%"PRIx32", imr-masks = {0x%"
			PRIx32", 0x%"PRIx32", 0x%"PRIx32", 0x%"PRIx32"}, "
			"pgcr-mask = 0x%"PRIx32", pupscr-mask = 0x%"PRIx32"\n",
			s->cntr_mask, s->imr_mask[0], s->imr_mask[1],
			s->imr_mask[2], s->imr_mask[3],
			s->pgcr_mask, s->pupscr_mask);
	}
end:
	return rc;
}

static int gpc_emulator_remove(struct vmm_emudev *edev)
{
	struct gpc_state *s = edev->priv;
	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid gpc_emuid_table[] = {
	{ .type = "pt",
	  .compatible = "imx6q-gpc",
	},
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(gpc_emulator,
				"imx6q-gpc",
				gpc_emuid_table,
				VMM_DEVEMU_NATIVE_ENDIAN,
				gpc_emulator_probe,
				gpc_emulator_remove,
				gpc_emulator_reset,
				gpc_emulator_read,
				gpc_emulator_write);

static int __init gpc_emulator_init(void)
{
	return vmm_devemu_register_emulator(&gpc_emulator);
}

static void __exit gpc_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&gpc_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);


