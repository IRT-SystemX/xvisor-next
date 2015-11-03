/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_devemu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for device emulation framework
 */
#ifndef _VMM_DEVEMU_H__
#define _VMM_DEVEMU_H__

#include <vmm_limits.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_manager.h>

struct vmm_emudev;
struct vmm_emulator;

enum vmm_devemu_endianness {
	VMM_DEVEMU_UNKNOWN_ENDIAN=0,
	VMM_DEVEMU_NATIVE_ENDIAN=1,
	VMM_DEVEMU_LITTLE_ENDIAN=2,
	VMM_DEVEMU_BIG_ENDIAN=3,
	VMM_DEVEMU_MAX_ENDIAN=4,
};

typedef int (*fn_probe) (struct vmm_guest *guest,
			 struct vmm_emudev *edev,
			 const struct vmm_devtree_nodeid *nodeid);

typedef int (*fn_remove) (struct vmm_emudev *edev);
typedef int (*fn_reset) (struct vmm_emudev *edev);
typedef	int (*fn_read) (void *arg,				\
			physical_addr_t offset,			\
			u32 *dst);
typedef	int (*fn_write) (void *arg,				\
			 physical_addr_t offset,		\
			 u32 regmask,				\
			 u32 regval);

#define FN_READ(X)						\
	typedef	int (*fn_read ## X) (struct vmm_emudev *edev,	\
				     physical_addr_t offset,	\
				     u ## X *dst)
#define FN_WRITE(X)						\
	typedef	int (*fn_write ## X) (struct vmm_emudev *edev,	\
				      physical_addr_t offset,	\
				      u ## X src)

FN_READ(8);
FN_WRITE(8);
FN_READ(16);
FN_WRITE(16);
FN_READ(32);
FN_WRITE(32);
FN_READ(64);
FN_WRITE(64);


struct vmm_emulator {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	const struct vmm_devtree_nodeid *match_table;
	enum vmm_devemu_endianness endian;
	fn_probe probe;
	fn_remove remove;
	fn_reset reset;
	fn_read8 read8;
	fn_write8 write8;
	fn_read16 read16;
	fn_write16 write16;
	fn_read32 read32;
	fn_write32 write32;
	union {
		struct {
			fn_read64 read64;
			fn_write64 write64;
		};
		struct {
			fn_read read_simple;
			fn_write write_simple;
		};
	};
};

int vmm_devemu_simple_read8(struct vmm_emudev *edev,
			    physical_addr_t offset,
			    u8 *dst);
int vmm_devemu_simple_read16(struct vmm_emudev *edev,
			     physical_addr_t offset,
			     u16 *dst);
int vmm_devemu_simple_read32(struct vmm_emudev *edev,
			     physical_addr_t offset,
			     u32 *dst);
int vmm_devemu_simple_write8(struct vmm_emudev *edev,
			     physical_addr_t offset,
			     u8 src);
int vmm_devemu_simple_write16(struct vmm_emudev *edev,
			      physical_addr_t offset,
			      u16 src);
int vmm_devemu_simple_write32(struct vmm_emudev *edev,
			      physical_addr_t offset,
			      u32 src);

#define VMM_DECLARE_EMULATOR_SIMPLE(NAME, MATCH, ENDIAN, PROBE, REMOVE,	\
				    RESET, READ, WRITE)			\
	static struct vmm_emulator NAME ## _emulator = {		\
		.name = #NAME,						\
		.match_table = MATCH,					\
		.endian = VMM_DEVEMU_LITTLE_ENDIAN,			\
		.probe = PROBE,						\
		.read8 = vmm_devemu_simple_read8,			\
		.write8 = vmm_devemu_simple_write8,			\
		.read16 = vmm_devemu_simple_read16,			\
		.write16 = vmm_devemu_simple_write16,			\
		.read32 = vmm_devemu_simple_read32,			\
		.write32 = vmm_devemu_simple_write32,			\
		.reset = RESET,						\
		.remove = REMOVE,					\
		.read_simple = READ,					\
		.write_simple = WRITE,					\
	};

struct vmm_emudev {
	vmm_spinlock_t lock;
	struct vmm_devtree_node *node;
	struct vmm_region *reg;
	struct vmm_emulator *emu;
	void *priv;
};

/** Emulate memory read to virtual device for given VCPU */
int vmm_devemu_emulate_read(struct vmm_vcpu *vcpu, 
			    physical_addr_t gphys_addr,
			    void *dst, u32 dst_len,
			    enum vmm_devemu_endianness dst_endian);

/** Emulate memory write to virtual device for given VCPU */
int vmm_devemu_emulate_write(struct vmm_vcpu *vcpu, 
			     physical_addr_t gphys_addr,
			     void *src, u32 src_len,
			     enum vmm_devemu_endianness src_endian);

/** Emulate IO read to virtual device for given VCPU */
int vmm_devemu_emulate_ioread(struct vmm_vcpu *vcpu, 
			      physical_addr_t gphys_addr,
			      void *dst, u32 dst_len,
			      enum vmm_devemu_endianness dst_endian);

/** Emulate IO write to virtual device for given VCPU */
int vmm_devemu_emulate_iowrite(struct vmm_vcpu *vcpu, 
			       physical_addr_t gphys_addr,
			       void *src, u32 src_len,
			       enum vmm_devemu_endianness src_endian);

/** Internal function to emulate irq (should not be called directly) */
extern int __vmm_devemu_emulate_irq(struct vmm_guest *guest, 
				    u32 irq, int cpu, int level);

/** Emulate shared irq for guest */
#define vmm_devemu_emulate_irq(guest, irq, level)	\
		__vmm_devemu_emulate_irq(guest, irq, -1, level) 

/** Emulate percpu irq for guest */
#define vmm_devemu_emulate_percpu_irq(guest, irq, cpu, level)	\
		__vmm_devemu_emulate_irq(guest, irq, cpu, level) 

/** Register guest irq handler */
int vmm_devemu_register_irq_handler(struct vmm_guest *guest, u32 irq,
		const char *name,
		void (*handle) (u32 irq, int cpu, int level, void *opaque),
		void *opaque);

/** Unregister guest irq handler */
int vmm_devemu_unregister_irq_handler(struct vmm_guest *guest, u32 irq,
		void (*handle) (u32 irq, int cpu, int level, void *opaque),
		void *opaque);

/** Count available irqs of a guest */
u32 vmm_devemu_count_irqs(struct vmm_guest *guest);

/** Register emulator */
int vmm_devemu_register_emulator(struct vmm_emulator *emu);

/** Unregister emulator */
int vmm_devemu_unregister_emulator(struct vmm_emulator *emu);

/** Find a registered emulator */
struct vmm_emulator *vmm_devemu_find_emulator(const char *name);

/** Get a registered emulator */
struct vmm_emulator *vmm_devemu_emulator(int index);

/** Count available emulators */
u32 vmm_devemu_emulator_count(void);

/** Reset context for given guest */
int vmm_devemu_reset_context(struct vmm_guest *guest);

/** Reset emulators for given region */
int vmm_devemu_reset_region(struct vmm_guest *guest, struct vmm_region *reg);

/** Probe emulators for given region */
int vmm_devemu_probe_region(struct vmm_guest *guest, struct vmm_region *reg);

/** Remove emulator for given region */
int vmm_devemu_remove_region(struct vmm_guest *guest, struct vmm_region *reg);

/** Initialize context for given guest */
int vmm_devemu_init_context(struct vmm_guest *guest);

/** DeInitialize context for given guest */
int vmm_devemu_deinit_context(struct vmm_guest *guest);

/** Initialize device emulation framework */
int vmm_devemu_init(void);

#endif
