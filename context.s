	.syntax unified
	.thumb

	.thumb_func
	.func 	irq_pendsv
	.globl	irq_pendsv
irq_pendsv:
	/* Save the context on the current PSP, passing it into
	pick_new_task. */
	mrs	r0, PSP
	cmp	r0, #0
	beq	1f
	stmfd	r0!, {r4 - r11}
1:	blx	pick_new_task

	/* Restore the context of PSP stack pointed at by r0. */
	ldmia	r0!, {r4 - r11}
	msr	psp, r0
	mov	lr, #0xFFFFFFFD
	bx	lr
	.endfunc
