#ifndef __IPUV3_INCLUDE_H__
#define __IPUV3_INCLUDE_H__

#include <vmm_types.h>
#include <vmm_host_irq.h>

struct irq_status {
	u32 irq_no;
	u32 in_irq;
	u32 ignore_next;
};

enum ipuv3_cache {
	IPUV3_CACHE_NONE = 0,
	IPUV3_CACHE_CTRL = (1 << 0),
	IPUV3_CACHE_STAT = (1 << 1)
};

#include <vmm_timer.h>

#define IPU_IRQ_REGS 15
struct ipuv3_state {
//	virtual_addr_t		iomem[COMP_COUNT];
	struct vmm_guest	*guest;
	physical_addr_t		base;

	irq_flags_t flags;
	vmm_spinlock_t		irqlock;
	u32			irq_regs[IPU_IRQ_REGS * 2];

	struct irq_status       irq_stat[4];
	u32 cache_irq;
	u32 in_irq;

	struct vmm_timer_event 	evt; // XXX
};

vmm_irq_return_t ipuv3_trigger_sw_interrupt(struct ipuv3_state *s, u32 irq);
void ipuv3_reset_irq_count(struct ipuv3_state *s);
u32 ipuv3_in_irq_count(const struct ipuv3_state *s);
bool ipuv3_cache_sync(struct ipuv3_state *s, enum ipuv3_cache caches);
bool ipuv3_cache_load(struct ipuv3_state *s, enum ipuv3_cache caches);
void ipuv3_take_lock(struct ipuv3_state *s);
void ipuv3_release_lock(struct ipuv3_state *s);
void ipuv3_dump_cache(struct ipuv3_state *s, enum ipuv3_cache caches);
struct ipuv3_state *ipuv3_state_for_irq(u32 irq);
void ipuv3_irq_ignore_next(u32 irq, u32 ignore);


#endif /* ! __IPUV3_INCLUDE_H__ */
