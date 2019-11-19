// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
.text

// This function is assumed to be called only from within InitGC. The value
// pointed to by InitGCs frame pointer is loaded as the stack sentinal value.
.globl InitTopOfStack
InitTopOfStack:
    mov (%rbp), %rdi
    mov %rdi, (TopOfStack)
    ret

.extern StackWalkAndMoveObjects
.globl GC

// We place the frame pointer in the first arg slot register and call
// StackWalkAndMoveObjects. The function epilogue needs to be hand-written
// afterwards so as to not corrupt the stack.
GC:
    pushq %rbp
    movq %rsp, %rbp
    subq $8, %rsp
    mov %rbp, %rdi
    call StackWalkAndMoveObjects
    addq $8, %rsp
    popq %rbp
    ret

.bss
.globl TopOfStack
TopOfStack:
    .quad 0
    .size TopOfStack, 8
