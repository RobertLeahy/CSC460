SREG=0x3F
SPH=0x3E
SPL=0x3D
RAMPZ=0x3B
EIND=0x3C


.macro	SAVECTX
	push	r0
	push	r1
	push	r2
	push	r3
	push	r4
	push	r5
	push	r6
	push	r7
	push	r8
	push	r9
	push	r10
	push	r11
	push	r12
	push	r13
	push	r14
	push	r15
	push	r16
	push	r17
	push	r18
	push	r19
	push	r20
	push	r21
	push	r22
	push	r23
	push	r24
	push	r25
	push	r26
	push	r27
	push	r28
	push	r29
	push	r30
	push	r31
	in	r16, SREG
	push	r16
	in	r16, RAMPZ
	push	r16
	in	r16, EIND
	push	r16
.endm

.macro	RESTORECTX
	pop	r16
	out	EIND,r16
	pop	r16
	out	RAMPZ,r16
	pop	r16
	out	SREG,r16
	pop	r31
	pop	r30
	pop	r29
	pop	r28
	pop	r27
	pop	r26
	pop	r25
	pop	r24
	pop	r23
	pop	r22
	pop	r21
	pop	r20
	pop	r19
	pop	r18
	pop	r17
	pop	r16
	pop	r15
	pop	r14
	pop	r13
	pop	r12
	pop	r11
	pop	r10
	pop	r9
	pop	r8
	pop	r7
	pop	r6
	pop	r5
	pop	r4
	pop	r3
	pop	r2
	pop	r1
	pop	r0
.endm

.macro CURRENTSP
	lds	r30, current_thread
	lds	r31, current_thread+1
.endm

.macro SAVESP
	in	r28, SPL
	in	r29, SPH
	st	Z+, r28
	st	Z+, r29
.endm

.macro RESTORESP
	ld	r28, Z+
	ld	r29, Z+
	out	SPL, r28
	out	SPH, r29
.endm


	.section .text
	.global kswitch
	.global kexit
	.extern kdispatch
	.extern current_thread


kswitch:
	SAVECTX
	CURRENTSP
	SAVESP
kexit:
	call kdispatch
	CURRENTSP
	RESTORESP
	RESTORECTX
	ret
