	.globl g_diag_reg

diag_reg_offset:
	.word	g_diag_reg - .

	.macro	set_diag_reg
	adr	r0, diag_reg_offset
	ldr	r1, [r0]
	add	r1, r1, r0		@ r1 = physical &g_diag_reg
	ldr	r0, [r1]
	mcr	p15, 0, r0, c15, c0, 1	@ write diagnostic register
	.endm


v7_invalidate_l1:
       mov     r0, #0
       mcr     p15, 2, r0, c0, c0, 0
       mrc     p15, 1, r0, c0, c0, 0

       ldr     r1, =0x7fff
       and     r2, r1, r0, lsr #13

       ldr     r1, =0x3ff

       and     r3, r1, r0, lsr #3      @ NumWays - 1
       add     r2, r2, #1              @ NumSets

       and     r0, r0, #0x7
       add     r0, r0, #4      @ SetShift

       clz     r1, r3          @ WayShift
       add     r4, r3, #1      @ NumWays
1:     sub     r2, r2, #1      @ NumSets--
       mov     r3, r4          @ Temp = NumWays
2:     subs    r3, r3, #1      @ Temp--
       mov     r5, r3, lsl r1
       mov     r6, r2, lsl r0
       orr     r5, r5, r6      @ Reg = (Temp<<WayShift)|(NumSets<<SetShift)
       mcr     p15, 0, r5, c7, c6, 2
       bgt     2b
       cmp     r2, #0
       bgt     1b
       dsb     st
       isb
       mov     pc, lr


	.globl v7_secondary_startup
	.globl _start_secondary_nopen
v7_secondary_startup:
	bl	v7_invalidate_l1
#	mrc	p15, 0, r0, c1, c0, 1
#	orr	r0, r0, #(1 << 0)
#	orr	r0, r0, #(1 << 6)
#	mcr	p15, 0, r0, c1, c0, 1
	set_diag_reg
	b	_start_secondary_nopen


 /*
 *     v7_flush_dcache_louis()
 *
 *     Flush the D-cache up to the Level of Unification Inner Shareable
 *
 *     Corrupted registers: r0-r7, r9-r11 (r6 only in Thumb mode)
 */

	.globl v7_flush_dcache_louis
v7_flush_dcache_louis:
	dmb					@ ensure ordering with previous memory accesses
	mrc	p15, 1, r0, c0, c0, 1		@ read clidr, r0 = clidr
	mov	r3, r0, lsr #20			@ move LoUIS into position
	ands	r3, r3, #7 << 1 		@ extract LoU*2 field from clidr
	bne	start_flush_levels		@ LoU != 0, start flushing
#ifdef CONFIG_ARM_ERRATA_643719
	mrc	p15, 0, r2, c0, c0, 0		@ read main ID register
	movw	r1, #:lower16:(0x410fc090 >> 4)	@ ID of ARM Cortex A9 r0p?
	movt	r1, #:upper16:(0x410fc090 >> 4)
	teq	r1, r2, lsr #4			@ test for errata affected core and if so...
	moveq	r3, #1 << 1			@   fix LoUIS value
	beq	start_flush_levels		@   start flushing cache levels
#endif
	mov	pc, lr
	.type v7_flush_dcache_louis, %function;
	.size v7_flush_dcache_louis, .-v7_flush_dcache_louis

/*
 *	v7_flush_dcache_all()
 *
 *	Flush the whole D-cache.
 *
 *	Corrupted registers: r0-r7, r9-r11 (r6 only in Thumb mode)
 *
 *	- mm    - mm_struct describing address space
 */
	.globl v7_flush_dcache_all
v7_flush_dcache_all:
	dmb					@ ensure ordering with previous memory accesses
	mrc	p15, 1, r0, c0, c0, 1		@ read clidr
	mov	r3, r0, lsr #23			@ move LoC into position
	ands	r3, r3, #7 << 1			@ extract LoC*2 from clidr
	beq	finished			@ if loc is 0, then no need to clean
start_flush_levels:
	mov	r10, #0				@ start clean at cache level 0
flush_levels:
	add	r2, r10, r10, lsr #1		@ work out 3x current cache level
	mov	r1, r0, lsr r2			@ extract cache type bits from clidr
	and	r1, r1, #7			@ mask of the bits for current cache only
	cmp	r1, #2				@ see what cache we have at this level
	blt	skip				@ skip if no cache, or just i-cache
#ifdef CONFIG_PREEMPT
	save_and_disable_irqs_notrace r9	@ make cssr&csidr read atomic
#endif
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	isb					@ isb to sych the new cssr&csidr
	mrc	p15, 1, r1, c0, c0, 0		@ read the new csidr
#ifdef CONFIG_PREEMPT
	restore_irqs_notrace r9
#endif
	and	r2, r1, #7			@ extract the length of the cache lines
	add	r2, r2, #4			@ add 4 (line length offset)
	movw	r4, #0x3ff
	ands	r4, r4, r1, lsr #3		@ find maximum number on the way size
	clz	r5, r4				@ find bit position of way size increment
	movw	r7, #0x7fff
	ands	r7, r7, r1, lsr #13		@ extract max number of the index size
loop1:
	mov	r9, r7				@ create working copy of max index
loop2:
	orr	r11, r10, r4, lsl r5		@ factor way and cache number into r11
	orr	r11, r11, r9, lsl r2		@ factor index number into r11
	mcr	p15, 0, r11, c7, c14, 2		@ clean & invalidate by set/way
	subs	r9, r9, #1			@ decrement the index
	bge	loop2
	subs	r4, r4, #1			@ decrement the way
	bge	loop1
skip:
	add	r10, r10, #2			@ increment cache number
	cmp	r3, r10
	bgt	flush_levels
finished:
	mov	r10, #0				@ swith back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	dsb	st
	isb
	mov	pc, lr
	.type v7_flush_dcache_all, %function;
	.size v7_flush_dcache_all, .-v7_flush_dcache_all


