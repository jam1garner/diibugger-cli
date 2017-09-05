/*	Cafe OS Substrate

	kernel.S - Miscellaneous kernel-related functions, such as kern_read/write.
	Paired with kernel.h.

	https://github.com/QuarkTheAwesome/COSSubstrate

	Copyright (c) 2016 Ash (QuarkTheAwesome)
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to
	deal in the Software without restriction, including without limitation the
	rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
	sell copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

#include "ppc_asm.h"

.section ".text"
.align 4

stack_pointer_save:
	.int 0x00000000

/*	The project I took this code from uses its own kern_write syscall,
	but we don't get such luxuries here.
*/
.globl old_kern_write
old_kern_write:
	mflr r0
	stw r0, 8(r1)

	mr r5, r4
	mr r9, r3

	li r3, 1
	li r4, 0
	li r6, 0
	li r7, 0
	lis r8, 1

	lis r10, stack_pointer_save@ha
	stw r1, stack_pointer_save@l(r10)

	li r0, 0x3500
	sc
	nop

	lis r10, stack_pointer_save@ha
	lwz r1, stack_pointer_save@l(r10)

	lwz r0, 8(r1)
	mtlr r0

	blr

/*	kern_write for physical addresses. Also gets around
	0x01000000 weirdness.
	Maps phys to itself and writes.

	_kernelmode_ void KernWritePhys(void* phys, unsigned int val);
*/
.globl KernWritePhys
KernWritePhys:
	//Back up existing DBAT0
	mfdbatu r5, 0
	mfdbatl r6, 0

	//Mask out phys
	andis. r9, r3, 0xFFFC

	//Generate DBAT0U
	ori r7, r9, 0xF
	//Generate DBAT0L
	ori r8, r9, 0x32

	//Write in our new DBAT0
	mtdbatu 0, r7
	mtdbatl 0, r8

	//Because PowerPC
	eieio
	isync

	//The physical address $(PHYS) is now mapped to the virual address $(PHYS)
	stw r4, 0(r3)

	//Flush caches
	dcbf r3, 0
	icbi r3, 0

	//Restore original DBAT0
	mtdbatu 0, r5
	mtdbatl 0, r6

	//Because POWERFUL
	eieio
	isync

	//Return
	rfi
