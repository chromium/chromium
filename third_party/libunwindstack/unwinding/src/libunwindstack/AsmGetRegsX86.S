/*
 * Copyright (C) 2016 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

  .text
  .global AsmGetRegs
  .balign 16
  .type AsmGetRegs, @function
AsmGetRegs:
  .cfi_startproc
  mov 4(%esp), %eax
  movl $0, (%eax)
  movl %ecx, 4(%eax)
  movl %edx, 8(%eax)
  movl %ebx, 12(%eax)

  /* ESP */
  leal 4(%esp), %ecx
  movl %ecx, 16(%eax)

  movl %ebp, 20(%eax)
  movl %esi, 24(%eax)
  movl %edi, 28(%eax)

  /* EIP */
  movl (%esp), %ecx
  movl %ecx, 32(%eax)

  mov  %cs, 36(%eax)
  mov  %ss, 40(%eax)
  mov  %ds, 44(%eax)
  mov  %es, 48(%eax)
  mov  %fs, 52(%eax)
  mov  %gs, 56(%eax)
  ret

  .cfi_endproc
  .size AsmGetRegs, .-AsmGetRegs
