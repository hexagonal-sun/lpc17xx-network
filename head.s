	.syntax unified
	.arm

	.section	.start_text
	.align	2
	.func	head
head:
	mov	r1, pc
	ldr	r0, =head
	ldr	r2, =_stext
	ldr	r3, =_sstack
	sub	r2, r3, r2		// Calculate the size
					// of the kernel text.

	mrc	p15, 0, r3, c1, c0, 0	// Read SCTLR
	bic	r3, #0x3800		// Clear VIZ
	bic	r3, #0x07       	// Clear CAM
	mcr	p15, 0, r3, c1, c0, 0	// Save SCTLR

	// Align the two addresses.
	bic	r1, r1, #0xff
	bic	r0, r0, #0xff

	bl	__write_pg_tables
	bl	__enable_mmu

	mov	r2, r3
	ldr	sp, =_sstack
	ldr	r4, =_start
	mov	pc, r4
	.endfunc

	/* Should be called with:
	 * r0: VA of the kernel head.
	 * r1: Corresponding PA of the kernel head.
	 * r2: The size of the kernel text.
	 *
	 * The translation tables are written after _sstack.
	 */
	.func __write_pg_tables
__write_pg_tables:
	// Calculate the physical address that the page tables should
	// be written to (the end of the kernel text).
	add	r3, r1, r2
	bic	r3, r3, #0xff		// Align the address.
	bic	r3, r3, #0x3f00

	// Table index of PA (which we will have to map so we can jump
	// into virtual memory).
	lsr	r4, r0, #20

	// Write out a section ID map.
	mov	r5, r3			// Start address.
	add	r6, r5, #(0x1000 * 4)	// Calculate end address.
	mov	r10, #0x2		// Section Mapping
	orr	r10, r10, #0xc00	// Set AP[1:0]
1:	str	r10, [r5], #4		// Store section descriptor.
	add	r10, r10, #0x100000	// Increment section addr.
	cmp	r5, r6			// Finished?
	blt	1b

	// Create the 'small page' table at the end of the section ID
	// mapping.
	bic	r5, r5, #0xff		// Align current pointer.
	bic	r5, r5, #0x300
	add	r5, r5, #0x800		// Don't trample on the ID map.

	// Set all pages to invalid.
	mov	r10, #0
	mov	r7, r5			// Start Address
	add	r6, r7, #0x3fc		// Calculate end address.
1:	str	r10, [r6], #4		// Store the invalid page.
	cmp	r6, r7			// Finished?
	blt	1b

	// Calculate the kernel mapping address into the table.
	lsr	r6, r0, #12		// shift kernel VA to find index.
	and	r6, r6, #0xff		// Mask off the section part.
	lsl	r6, r6, #2		// Multiply by 4.
	add	r6, r5, r6		// Add onto small page base addr.

	// Calculate the end adress of the kernel mapping.
	lsr	r7, r2, #12		// Divide by 4k to find no. pages.
	add	r7, r7, #1		// +1, ensure we map something.
	lsl	r7, r7, #2		// Multiply by 4.
	add	r7, r6, r7		// Add to mapping start addr.

	// Page align the PA of the kernel.
	bic	r9, r1, #0xff		// Mask off any non-page addr
	bic	r9, r9, #0xf00		// bits on the kernel's PA.

	// Write out the kernel map.
	mov	r10, #0x3e		// Set AP[1:0], C, B and small pg.
	orr	r10, r10, r9		// Set page PA.
1:	str	r10, [r6], #4		// Store the page.
	add	r10, r10, #(1 << 12)	// Next PA of the page.
	cmp	r6, r7			// Finished?
	blt	1b

	// Write out the page table map into the ID mapping.
	mov	r10, #0x1		// Page table mapping
	orr	r10, r10, r5		// Set page table address
	str	r10, [r3, r4, lsl #2]	// Store into the ID map.
	mov	pc, lr
	.endfunc

	/* Should be called with:
	 * r3: address of translation tables.
	 *
	 * After returning the MMU & caches will be on.
	 */
	.func __enable_mmu
__enable_mmu:
	mov	r4, #0
	mcr	p15, 0, r4, c7, c10, 4	// drain write buffer
	mcr	p15, 0, r4, c8, c7, 0	// flush I,D TLBs
	mrc	p15, 0, r5, c1, c0, 0	// read control reg
	bic	r5, r5, #1 << 28	// clear TRE
	orr	r5, r5, #0x5800		// RR I Z
	orr	r5, r5, #0x0005		// C M
	mov	r6, #0x1		// domain 0 = client
	mcr	p15, 0, r3, c2, c0, 0	// load page table pointer
	mcr	p15, 0, r6, c3, c0, 0	// load domain access control
	mcr	p15, 0, r4, c2, c0, 2	// Set TTBCR to always use TTBR0
	mcr	p15, 0, r4, c7, c5, 4	// ISB
	mcr	p15, 0, r5, c1, c0, 0	// load control register
	mrc	p15, 0, r5, c1, c0, 0	// and read it back
	mcr	p15, 0, r4, c7, c5, 4	// ISB
	mov	pc, lr
	.endfunc
