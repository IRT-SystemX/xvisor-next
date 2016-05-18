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

#include <vmm_timer.h> // TEST

#include <emu/ipuv3h.h>

#define MODULE_DESC			"ipuv3 Pass-through Emulator"
#define MODULE_AUTHOR			"Jean Guyomarc'h"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			ipuv3_emulator_init
#define	MODULE_EXIT			ipuv3_emulator_exit

//#define BANK

#define DBG
#undef DBG

static const u64 _delay = 5ULL * 1000ULL * 1000ULL * 1000ULL;
static const u64 _delay_s = 50ULL * 1000ULL * 1000ULL * 1000ULL;

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

#define IRQ_SYNC_IDX	0
#define IRQ_ERR_IDX	1


static unsigned int handlers_count = 0;
static struct ipuv3_state *handlers[2]; /* FIXME */


#define OFFSET_IRQ_CTRL_BEGIN	0x20003C
#define OFFSET_IRQ_CTRL_END	0x200074
#define OFFSET_IRQ_STAT_BEGIN	0x200200
#define OFFSET_IRQ_STAT_END	0x200238

static inline bool offset_is_ctrl(physical_addr_t off)
{
	return ((off >= OFFSET_IRQ_CTRL_BEGIN) && (off <= OFFSET_IRQ_CTRL_END));
}

static inline bool offset_is_stat(physical_addr_t off)
{
	return ((off >= OFFSET_IRQ_STAT_BEGIN) && (off <= OFFSET_IRQ_STAT_END));
}

static void ipuv3_dump_stat(struct ipuv3_state *s)
{
	u32 buf[IPU_IRQ_REGS];
	unsigned int i;

	vmm_host_memory_read(s->base + OFFSET_IRQ_STAT_BEGIN, buf, sizeof(buf), false);
	vmm_printf("IPU STAT registers:\n");
	for (i = 0; i < IPU_IRQ_REGS; i++) {
		vmm_printf("0x%08"PRIx32" ", buf[i]);
		if ((i + 1)  % 6 == 0) {
			vmm_printf("\n");
		}
	}
	vmm_printf("\n");
}

static void ipuv3_dump_ctrl(struct ipuv3_state *s)
{
	u32 buf[IPU_IRQ_REGS];
	unsigned int i;

	vmm_host_memory_read(s->base + OFFSET_IRQ_CTRL_BEGIN, buf, sizeof(buf), false);
	vmm_printf("IPU CTRL registers:\n");
	for (i = 0; i < IPU_IRQ_REGS; i++) {
		vmm_printf("0x%08"PRIx32" ", buf[i]);
		if ((i + 1)  % 6 == 0) {
			vmm_printf("\n");
		}
	}
	vmm_printf("\n");
}

static void ipuv3_clear_stat(struct ipuv3_state *s)
{
	u32 buf[IPU_IRQ_REGS];

	memset(buf, 0xff, sizeof(buf));
	vmm_host_memory_write(s->base + OFFSET_IRQ_STAT_BEGIN,
			      buf, sizeof(buf), false);
}

static inline u32 ipuv3_ctrl_read(struct ipuv3_state *s, unsigned int reg)
{
	u32 val;

	vmm_host_memory_read(s->base + OFFSET_IRQ_CTRL_BEGIN + (reg * sizeof(u32)),
			     &val, sizeof(u32), false);
#ifdef DBG
	vmm_linfo("%s(%u): read 0x%"PRIx32"\n", __func__, reg, val);
#endif
	return val;
}

static inline void ipuv3_ctrl_write(struct ipuv3_state *s, unsigned int reg, u32 val)
{
	vmm_host_memory_write(s->base + OFFSET_IRQ_CTRL_BEGIN + (reg * sizeof(u32)),
			     &val, sizeof(u32), false);

#ifdef DBG
	vmm_linfo("%s(%u): write 0x%"PRIx32"\n", __func__, reg, val);
#endif
}

static inline void ipuv3_stat_write(struct ipuv3_state *s, unsigned int reg, u32 val)
{
	vmm_host_memory_write(s->base + OFFSET_IRQ_STAT_BEGIN + (reg * sizeof(u32)),
			     &val, sizeof(u32), false);

#ifdef DBG
	vmm_linfo("%s(%u): write 0x%"PRIx32"\n", __func__, reg, val);
#endif
}

static inline u32 ipuv3_stat_read(struct ipuv3_state *s, unsigned int reg)
{
	u32 val;

	vmm_host_memory_read(s->base + OFFSET_IRQ_STAT_BEGIN + (reg * sizeof(u32)),
			     &val, sizeof(u32), false);

#ifdef DBG
	vmm_linfo("%s(%u): read 0x%"PRIx32"\n", __func__, reg, val);
#endif
	return val;
}

static inline void ipuv3_ack(struct ipuv3_state *s,
			     unsigned int reg, u32 mask)
{
	ipuv3_stat_write(s, reg, mask);
}


static inline void ipuv3_mask(struct ipuv3_state *s,
			      unsigned int reg, u32 mask)
{
	u32 old;

	old = ipuv3_ctrl_read(s, reg);
	old &= ~mask;
	ipuv3_ctrl_write(s, reg, old);
}

static inline void ipuv3_unmask(struct ipuv3_state *s,
			      unsigned int reg, u32 mask)
{
	u32 old;

	old = ipuv3_ctrl_read(s, reg);
	old |= mask;
	ipuv3_ctrl_write(s, reg, old);
}

static void ipuv3_mask_all(struct ipuv3_state *s)
{
	u32 buf[IPU_IRQ_REGS];

	memset(buf, 0, sizeof(buf));
	vmm_host_memory_write(s->base + OFFSET_IRQ_CTRL_BEGIN,
			      buf, sizeof(buf), false);

#if 0
	memset(buf, 0xff, sizeof(buf));
	vmm_host_memory_write(s->base + OFFSET_IRQ_STAT_BEGIN,
			      buf, sizeof(buf), false);
#endif
}

static bool offset_irq_get(physical_addr_t offset,
			   unsigned int *reg, unsigned int *bit)
{
	physical_addr_t diff;
	unsigned int shift;

	if (offset_is_ctrl(offset)) {
		diff = OFFSET_IRQ_CTRL_BEGIN;
		shift = 0;
	} else if (offset_is_stat(offset)) {
		diff = OFFSET_IRQ_STAT_BEGIN;
		shift = IPU_IRQ_REGS;
	} else {
		vmm_lerror("Offset 0x%"PRIPADDR" is invalid\n", offset);
		return FALSE;
	}

	offset -= diff;
	if (reg) {
		*reg = (offset / sizeof(u32)) + shift;
	}
	if (bit) {
		*bit = offset % 2;
	}

//	vmm_lwarning("old_offset=0x%"PRIPADDR", offset=0x%"PRIPADDR", reg=%u, bit=%u\n",
//		     old_offset, offset, reg?*reg:-1, bit?*bit:-1);
	return TRUE;
}

struct ipuv3_state *
ipuv3_state_for_irq(u32 irq)
{
	unsigned int i, j;
	struct ipuv3_state *s;

	for (i = 0; i < handlers_count; i++) {
		s = handlers[i];
		for (j = 0; j < array_size(s->irq_stat); j++) {
			if (s->irq_stat[j].irq_no == irq) {
				return s;
			}
		}
	}
	return NULL;
}

void ipuv3_irq_ignore_next(u32 irq, u32 ignore)
{
	unsigned int i, j;
	struct ipuv3_state *s;

	for (i = 0; i < handlers_count; i++) {
		s = handlers[i];
		for (j = 0; j < array_size(s->irq_stat); j++) {
			if (s->irq_stat[j].irq_no == irq) {
				s->irq_stat[j].ignore_next = ignore;
#ifdef DBG
				vmm_linfo("IRQ %"PRIu32" will ignore the %"PRIu32" next\n",
					  irq, ignore);
#endif
			}
		}
	}
}

void ipuv3_take_lock(struct ipuv3_state *s)
{
	vmm_spin_lock_irqsave(&s->irqlock, s->flags);
}

void ipuv3_release_lock(struct ipuv3_state *s)
{
	vmm_spin_unlock_irqrestore(&s->irqlock, s->flags);
}

bool ipuv3_cache_load(struct ipuv3_state *s, enum ipuv3_cache caches)
{
	const u32 len = IPU_IRQ_REGS * sizeof(u32);

	if (caches & IPUV3_CACHE_CTRL) {
#ifdef DBG
		vmm_linfo("Loading CTRL cache: 0x%"PRIPADDR"\n", s->base + OFFSET_IRQ_CTRL_BEGIN);
#endif
		vmm_host_memory_read(s->base + OFFSET_IRQ_CTRL_BEGIN,
				      &(s->irq_regs[0]), len, false);
	}
	if (caches & IPUV3_CACHE_STAT) {
#ifdef DBG
		vmm_linfo("Loading STAT cache: 0x%"PRIPADDR"\n", s->base + OFFSET_IRQ_STAT_BEGIN);
#endif
		vmm_host_memory_read(s->base + OFFSET_IRQ_STAT_BEGIN,
				      &(s->irq_regs[IPU_IRQ_REGS]), len, false);
	}

	return TRUE;

}

/* MUST be called with irqlock held */
bool ipuv3_cache_sync(struct ipuv3_state *s, enum ipuv3_cache caches)
{
	const u32 len = IPU_IRQ_REGS * sizeof(u32);

	if (caches & IPUV3_CACHE_STAT) {
#ifdef DBG
		vmm_linfo("Syncing STAT cache: 0x%"PRIPADDR"\n", s->base + OFFSET_IRQ_STAT_BEGIN);
#endif
		vmm_host_memory_write(s->base + OFFSET_IRQ_STAT_BEGIN,
				      &(s->irq_regs[IPU_IRQ_REGS]), len, false);
	}
	if (caches & IPUV3_CACHE_CTRL) {
#ifdef DBG
		vmm_linfo("Syncing CTRL cache: 0x%"PRIPADDR"\n", s->base + OFFSET_IRQ_CTRL_BEGIN);
#endif
		vmm_host_memory_write(s->base + OFFSET_IRQ_CTRL_BEGIN,
				      &(s->irq_regs[0]), len, false);
	}


	return TRUE;
}

void ipuv3_dump_cache(struct ipuv3_state *s, enum ipuv3_cache caches)
{
	unsigned int i;

	if (caches & IPUV3_CACHE_CTRL) {
		vmm_printf("CTRL cache:\n");
		for (i = 0; i < IPU_IRQ_REGS; i++) {
			vmm_printf("0x%08"PRIx32" ", s->irq_regs[i]);
			if ((i + 1) % 6 == 0) {
				vmm_printf("\n");
			}
		}
		vmm_printf("\n");
	}
	if (caches & IPUV3_CACHE_STAT) {
		vmm_printf("STAT cache:\n");
		for (i = IPU_IRQ_REGS; i < 2 * IPU_IRQ_REGS; i++) {
			vmm_printf("0x%08"PRIx32" ", s->irq_regs[i]);
			if ((i - IPU_IRQ_REGS + 1) % 6 == 0) {
				vmm_printf("\n");
			}
		}
		vmm_printf("\n");
	}
}

/* MUST be called with irqlock held */
u32 ipuv3_in_irq_count(const struct ipuv3_state *s)
{
	u32 in_irq = 0;
	unsigned int i;

	for (i = 0; i < array_size(s->irq_stat); i++) {
		in_irq += s->irq_stat[i].in_irq;
	}

	return in_irq;
}

/* MUST be called with irqlock held */
void ipuv3_reset_irq_count(struct ipuv3_state *s)
{
	unsigned int i;

	for (i = 0; i < array_size(s->irq_stat); i++) {

#ifdef DBG
	vmm_lwarning("irq %"PRIu32" count was %"PRIu32". Resetting...\n",
		     s->irq_stat[i].irq_no, s->irq_stat[i].in_irq);
#endif
		s->irq_stat[i].in_irq = 0;
//		vmm_host_irq_unmask(s->irq_stat[i].irq_no);
	}
}

vmm_irq_return_t ipuv3_trigger_sw_interrupt(struct ipuv3_state *s, u32 irq)
{
	int rc;

	//ipuv3_dump_cache(s, IPUV3_CACHE_CTRL | IPUV3_CACHE_STAT);
	rc = vmm_devemu_emulate_irq(s->guest, irq, 1);
	if (rc) {
		vmm_lerror("Failed to emulate IRQ \n");
		goto fail;
	}
	rc = vmm_devemu_emulate_irq(s->guest, irq, 0);
	if (rc) {
		vmm_lerror("Failed to emulate IRQ \n");
		goto fail;
	}

fail:
	return VMM_IRQ_HANDLED;
}


/*============================================================================*
 *                         Pass-Through Memory Access                         *
 *============================================================================*/

#ifdef BANK
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
#endif

static int ipuv3_emulator_read(struct vmm_emudev *edev,
                                physical_addr_t offset,
                                u32 *dst,
                                u32 size)
{
        struct ipuv3_state const *const s = edev->priv;

#ifdef BANK
	volatile void *addr;
	int bank;

	bank = ipuv3_iomem_bank_get(s, offset);
	if (bank < 0) {
		vmm_lcritical("%s: offset 0x%"PRIPADDR" is not mapped\n",
			      edev->node->name, offset);
		return VMM_EFAIL;
	}

	addr = (void *)(s->iomem[bank] + offset - comp[bank].offset);
	*dst = vmm_readl(addr);

	#ifdef DBG
		vmm_lnotice("%s: About to read at offset %"PRIPADDR" (bank %s). Addr: 0x%p. => 0x%"PRIx32"\n",
			    edev->node->name, offset, comp[bank].name, addr, *dst);
	#endif
#else
	unsigned int reg;
	const char *str = "";

	if (offset_is_ctrl(offset) || offset_is_stat(offset)) {
		offset_irq_get(offset, &reg, NULL);
		*dst = s->irq_regs[reg];
		str  = "(cache) ";
	} else {
		vmm_host_memory_read(s->base + offset,
				     dst, size, false);
	}
	#ifdef DBG
		vmm_lnotice("%s: %sRead 0x%"PRIx32" at 0x%"PRIx32"\n", edev->node->name,
			    str,
			    *dst, edev->reg->hphys_addr + offset);
	#endif

#endif



        return VMM_OK;
}

static int ipuv3_emulator_write(struct vmm_emudev *edev,
                                 physical_addr_t offset,
                                 u32 mask,
                                 u32 val,
                                 u32 size)
{
        struct ipuv3_state *const s = edev->priv;

#ifdef BANK
	u32 old;
	int bank;
	volatile void *addr;
	physical_addr_t off;

	bank = ipuv3_iomem_bank_get(s, offset);
	if (bank < 0) {
		vmm_lcritical("%s: offset 0x%"PRIPADDR" is not mapped\n",
			      edev->node->name, offset);
		return VMM_EFAIL;
	}
	off = offset - comp[bank].offset;
	addr = (void *)(s->iomem[bank] + off);
	old = vmm_readl(addr);

	vmm_writel(val, addr);
#ifdef DBG
	vmm_lnotice("%s: About to write 0x%"PRIx32" at offset %"PRIPADDR" (bank %s). Addr: 0x%p."
		    " Was 0x%"PRIx32"\n",
		    edev->node->name, val, offset, comp[bank].name, addr,
		    old);
#endif
#else
	const char *str = "";
	unsigned int reg;

	if (offset_is_stat(offset) || offset_is_ctrl(offset)) {
		offset_irq_get(offset, &reg, NULL);

		str = "(cache) ";
		s->irq_regs[reg] = val;

		if (offset_is_ctrl(offset)) {
			if ((s->irq_stat[IRQ_SYNC_IDX].in_irq == 0) &&
			    (s->irq_stat[IRQ_ERR_IDX].in_irq == 0)) {
				ipuv3_take_lock(s);
				ipuv3_cache_sync(s, IPUV3_CACHE_CTRL);
				ipuv3_release_lock(s);
				str = "(cache+sync) ";
			}
		}
	} else {
		vmm_host_memory_write(s->base + offset,
				      &val, size, false);
	}

#ifdef DBG
	vmm_lnotice("%s: %sWrote 0x%"PRIx32" at 0x%"PRIx32"\n", edev->node->name,
		    str,
		    val, edev->reg->hphys_addr + offset);
#endif
#endif


        return VMM_OK;
}

#if 0
static vmm_irq_return_t simple_routed_irq(int irq, void *dev)
{
	struct vmm_emudev *edev = dev;
	struct ipuv3_state *s = edev->priv;
	const unsigned int idx_max = array_size(s->irq_stat);
	unsigned int idx;
	vmm_irq_return_t ret = VMM_IRQ_HANDLED;


	for (idx = 0; idx < idx_max; idx++) {
		if (irq == s->irq_stat[idx].irq_no) {
			break;
		}
	}
	if (unlikely(idx >= idx_max)) {
		vmm_lcritical("Invalid IRQ number: %i\n", irq);
		goto end;
	}

#ifdef DBG
	vmm_lalert("HW handling of irq %i. Count = %"PRIu32"\n",
		   irq, s->irq_stat[idx].in_irq);
#endif

	if (s->irq_stat[idx].ignore_next != 0) {
#ifdef DBG
		vmm_lalert("IGNORING IRQ!\n");
		ipuv3_clear_stat(s);
		ipuv3_dump_stat(s);
#endif
		s->irq_stat[idx].ignore_next--;
		goto end;
	}

#ifdef DBG
	ipuv3_dump_stat(s);
#endif

	if (s->irq_stat[idx].in_irq == 0) {
		ipuv3_cache_load(s, IPUV3_CACHE_STAT);
		ret = ipuv3_trigger_sw_interrupt(s, irq);
		ipuv3_mask_all(s);
	}
	s->irq_stat[idx].in_irq++;

end:
	return ret;
}
#else

static inline void irq_send(struct ipuv3_state *s, int irq)
{
	vmm_lcritical("Sending IRQ %i\n", irq);
	vmm_devemu_emulate_irq(s->guest, irq, 1);
	vmm_devemu_emulate_irq(s->guest, irq, 0);
}

static void _cb(struct vmm_timer_event *evt)
{
	struct ipuv3_state *s = evt->priv;

	s->cache_irq = 129; // XXX XXX
	vmm_linfo("Timer event. cache irq = %"PRIu32", in_irq = %"PRIu32"\n",
		  s->cache_irq, s->in_irq);
	if (s->cache_irq && !s->in_irq) {
		vmm_lwarning("emulating irq %"PRIu32"\n", s->cache_irq);
		s->in_irq++;
		irq_send(s, s->cache_irq);
		s->cache_irq = 0;
	}
//	vmm_linfo("Restart event...\n");
	//vmm_timer_event_restart(evt);
	vmm_timer_event_start(evt, _delay);
}

static void ipu_irq_handle(struct ipuv3_state *s, const int *regs, unsigned int num_regs)
{
	unsigned int i;
	unsigned long status;
	int bit;
	u32 mask;

	for (i = 0; i < num_regs; i++) {
		status = ipuv3_stat_read(s, regs[i]);
		status &= ipuv3_ctrl_read(s, regs[i]);

		for_each_set_bit(bit, &status, 32) {
			mask = (1 << bit);
			ipuv3_mask(s, i, mask);
			ipuv3_ack(s, i, mask);

			if (i == 0) {
				if (bit == 23) {
					s->cache_irq = 129; // XXX
				//	irq_send(s, 129);
				} else if (bit == 28) {
					irq_send(s, 130);
				}
			} else if (i == 14) {
				if (bit == 3) {
					/*
					 * Ignore.
					 * This is for when we want
					 * to END.
					 */
					irq_send(s, 146);
				} else if (bit == 9) {
					/*
					 * Ignore.
					 * This is for when we want
					 * to END.
					 */
					irq_send(s, 145);
				}
			} else {
				vmm_linfo("Reg %i, bit %i\n", i + 1, bit);
			}
			
			ipuv3_unmask(s, i, mask);
		}
	}
	ipuv3_clear_stat(s);
}

static vmm_irq_return_t ipu_sync_irq(int irq, void *dev)
{
	const int int_reg[] = { 0, 1, 2, 3, 10, 11, 12, 13, 14};
	struct vmm_emudev *edev = dev;
	struct ipuv3_state *s = edev->priv;

//	vmm_host_irq_mask(irq);
	//vmm_lalert("%s()\n", __func__);
	//ipuv3_dump_ctrl(s);
#if 1
	ipu_irq_handle(s, int_reg, array_size(int_reg));
#else
	vmm_host_irq_mask(irq);
	irq_send(s, 38);
#endif
	//vmm_host_irq_unmask(irq);

	return VMM_IRQ_HANDLED;
}
static vmm_irq_return_t ipu_err_irq(int irq, void *dev)
{
	vmm_lalert("%s(%i) is not implemented\n", __func__, irq);
	return VMM_IRQ_HANDLED;
}
#endif


static int ipuv3_emulator_reset(struct vmm_emudev *edev)
{
	//struct ipuv3_state *s = edev->priv;
//	ipuv3_irqs_unmask(s);
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

	INIT_TIMER_EVENT(&s->evt, _cb, s);

	s->guest = guest;
	s->base = base;
	irq_count = vmm_devtree_irq_count(edev->node);
	if (irq_count != 2) {
		vmm_lerror("Two interrupts  are expected\n");
		/* FIXME Dealloc */
		return VMM_EFAIL;
	}
#ifdef BANK

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

#endif

	for (i = 0; i < irq_count; i++) {
		rc = vmm_devtree_irq_get(edev->node, &irq, i);
		if (rc) {
			vmm_lerror("Failed to retrieve irq #%"PRIu32"\n", i);
			/* FIXME Dealloc */
			return VMM_EFAIL;
		}
		s->irq_stat[i].irq_no = irq;
		s->irq_stat[i].in_irq = 0;
		s->irq_stat[i].ignore_next = 0;
//		rc = vmm_host_irq_register(irq, edev->node->name,
//					   simple_routed_irq, edev);
		if (rc) {
			vmm_lerror("Failed to register irq %"PRIu32"\n", irq);
			/* FIXME Dealloc */
			return VMM_EFAIL;
		}
	}
	s->irq_stat[2].irq_no = 129;
	s->irq_stat[2].in_irq = 0;

	s->irq_stat[3].irq_no = 130;
	s->irq_stat[3].in_irq = 0;

	vmm_devtree_irq_get(edev->node, &irq, 0);
	vmm_lalert("registering IRQ %"PRIu32"\n", irq);
	rc = vmm_host_irq_register(irq, edev->node->name, ipu_sync_irq, edev);
	if (rc) {
		vmm_lerror("Failed to register irq %"PRIu32"\n", irq);
		/* FIXME Dealloc */
		return VMM_EFAIL;
	}

	vmm_devtree_irq_get(edev->node, &irq, 1);
	vmm_lalert("registering IRQ %"PRIu32"\n", irq);
	rc = vmm_host_irq_register(irq, edev->node->name, ipu_err_irq, edev);
	if (rc) {
		vmm_lerror("Failed to register irq %"PRIu32"\n", irq);
		/* FIXME Dealloc */
		return VMM_EFAIL;
	}

	INIT_SPIN_LOCK(&s->irqlock);
	edev->priv = s;

	s->cache_irq = 0;
	s->in_irq = 0;

	handlers[handlers_count++] = s;


	vmm_lwarning("Delay = %"PRIu64"\n", _delay);
	vmm_timer_event_start(&s->evt, _delay_s);

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
