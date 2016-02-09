	.syntax unified
	.thumb

	.thumb_func
	.func 	get_lock

	/* Get a lock, pointed at by r0 (an unsigned byte).  Spin
    	   until we succesfully acquire the lock. */
get_lock:
	ldrexb	r1, [r0]
	cmp	r1, #0
	bne	get_lock
	mov	r1, #1
	strexb	r2, r1, [r0]
	cmp	r2, #0
	bne	get_lock
	bx	lr
	.endfunc
	.global	get_lock

	.thumb_func
	.func	try_lock

	/* Try and acquire a lock pointed to by r0.  Returns '1' if
	   lock was acuqired, '0' otherwise. This function does not
	   block. */
try_lock:
	ldrexb	r1, [r0]
	cmp	r1, #0
	bne	1f
	mov	r1, #1
	strexb	r2, r1, [r0]
	cmp	r2, #0
	bne	1f
	mov	r0, #1
	bx	lr
1:	mov	r0, #0
	bx	lr
	.endfunc
	.global	try_lock

	.thumb_func
	.func	release_lock

	/* Release the lock pointed to by r0.  This function assumes
	   that the lock has already been acquired and makes no effort to
	   verify this. */
release_lock:	
	mov	r1, #0
	strb	r1, [r0]
	bx	lr
	.endfunc
	.global	release_lock
