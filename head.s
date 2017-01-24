	.syntax unified
	.arm

	.section .start_text
	.func head
head:
	ldr	sp, =_sstack
	b	_start
	.endfunc
