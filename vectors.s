	.syntax 	unified
	.thumb
	.section	.vectors

	.macro irq_handler name
		.word \name
		.weak \name
		.set  \name, default_irq_handler
	.endm

vectors:
	/* ARM Core interrupt vectors. */
	.word		_sstack
	.word		_start
	irq_handler	irq_nmi
	irq_handler	irq_hardfault
	irq_handler	irq_memmanage
	irq_handler	irq_busfault
	irq_handler	irq_usagefault
	.word		program_checksum
	.word		0
	.word		0
	.word		0
	irq_handler	irq_svc
	irq_handler	irq_debugmon
	.word		0
	irq_handler	irq_pendsv
	irq_handler	irq_systick

	/* LPC peripheral interrupt vectors. */
	irq_handler	irq_wdt
	irq_handler	irq_timer0
	irq_handler	irq_timer1
	irq_handler	irq_timer2
	irq_handler	irq_timer3
	irq_handler	irq_uart0
	irq_handler	irq_uart1
	irq_handler	irq_uart2
	irq_handler	irq_uart3
	irq_handler	irq_pwm1
	irq_handler	irq_i2c0
	irq_handler	irq_i2c1
	irq_handler	irq_i2c2
	irq_handler	irq_spi
	irq_handler	irq_ssp0
	irq_handler	irq_ssp1
	irq_handler	irq_pll0
	irq_handler	irq_rtc
	irq_handler	irq_eint0
	irq_handler	irq_eint1
	irq_handler	irq_eint2
	irq_handler	irq_eint3
	irq_handler	irq_adc
	irq_handler	irq_bod
	irq_handler	irq_usb
	irq_handler	irq_can
	irq_handler	irq_dma
	irq_handler	irq_i2s
	irq_handler	irq_enet
	irq_handler	irq_rit
	irq_handler	irq_mcpwm
	irq_handler	irq_qei
	irq_handler	irq_pll1
	irq_handler	irq_usbactivity
	irq_handler     irq_canactivity

	.section .text
	.func default_irq_handler
	.thumb_func
default_irq_handler:
	bx	lr
	.endfunc
