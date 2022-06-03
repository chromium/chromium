// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if defined(__i386__) || defined(__x86_64__) || defined(__aarch64__)

#if defined(__i386__) || defined(__x86_64__)
// namespace crashpad {
// void CaptureContext(x86_thread_state_t* x86_thread_state);
// }  // namespace crashpad
#define CAPTURECONTEXT_SYMBOL __ZN8crashpad14CaptureContextEP16x86_thread_state
#elif defined(__aarch64__)
// namespace crashpad {
// void CaptureContext(arm_unified_thread_state_t* arm_unified_thread_state);
// }  // namespace crashpad
#define CAPTURECONTEXT_SYMBOL \
    __ZN8crashpad14CaptureContextEP24arm_unified_thread_state
#endif

  .section __TEXT,__text,regular,pure_instructions
  .private_extern CAPTURECONTEXT_SYMBOL
  .globl CAPTURECONTEXT_SYMBOL
#if defined(__i386__) || defined(__x86_64__)
  .p2align 4, 0x90
#elif defined(__aarch64__)
  .p2align 2
#endif
CAPTURECONTEXT_SYMBOL:

#if defined(__i386__)

  .cfi_startproc

  pushl %ebp
  .cfi_def_cfa_offset 8
  .cfi_offset %ebp, -8
  movl %esp, %ebp
  .cfi_def_cfa_register %ebp

  // Note that 16-byte stack alignment is not maintained because this function
  // does not call out to any other.

  // pushfl first, because some instructions (but probably none used here)
  // affect %eflags. %eflags will be in -4(%ebp).
  pushfl

  // Save the original value of %eax, and use %eax to hold the x86_thread_state*
  // argument. The original value of %eax will be in -8(%ebp).
  pushl %eax
  movl 8(%ebp), %eax

  // Initialize the header identifying the x86_thread_state_t structure as
  // carrying an x86_thread_state32_t (flavor x86_THREAD_STATE32) of size
  // x86_THREAD_STATE32_COUNT 32-bit values.
  movl $1, (%eax)  // x86_thread_state->tsh.flavor
  movl $16, 4(%eax)  // x86_thread_state->tsh.count

  // General-purpose registers whose values haven’t changed can be captured
  // directly.
  movl %ebx, 12(%eax)  // x86_thread_state->uts.ts32.__ebx
  movl %ecx, 16(%eax)  // x86_thread_state->uts.ts32.__ecx
  movl %edx, 20(%eax)  // x86_thread_state->uts.ts32.__edx
  movl %edi, 24(%eax)  // x86_thread_state->uts.ts32.__edi
  movl %esi, 28(%eax)  // x86_thread_state->uts.ts32.__esi

  // Now that the original value of %edx has been saved, it can be repurposed to
  // hold other registers’ values.

  // The original %eax was saved on the stack above.
  movl -8(%ebp), %edx
  movl %edx, 8(%eax)  // x86_thread_state->uts.ts32.__eax

  // The original %ebp was saved on the stack in this function’s prologue.
  movl (%ebp), %edx
  movl %edx, 32(%eax)  // x86_thread_state->uts.ts32.__ebp

  // %esp was saved in %ebp in this function’s prologue, but the caller’s %esp
  // is 8 more than this value: 4 for the original %ebp saved on the stack in
  // this function’s prologue, and 4 for the return address saved on the stack
  // by the call instruction that reached this function.
  leal 8(%ebp), %edx
  movl %edx, 36(%eax)  // x86_thread_state->uts.ts32.__esp

  // The original %eflags was saved on the stack above.
  movl -4(%ebp), %edx
  movl %edx, 44(%eax)  // x86_thread_state->uts.ts32.__eflags

  // %eip can’t be accessed directly, but the return address saved on the stack
  // by the call instruction that reached this function can be used.
  movl 4(%ebp), %edx
  movl %edx, 48(%eax)  // x86_thread_state->uts.ts32.__eip

  // The segment registers are 16 bits wide, but x86_thread_state declares them
  // as unsigned 32-bit values, so zero the top half.
  xorl %edx, %edx
  movw %ss, %dx
  movl %edx, 40(%eax)  // x86_thread_state->uts.ts32.__ss
  movw %cs, %dx
  movl %edx, 52(%eax)  // x86_thread_state->uts.ts32.__cs
  movw %ds, %dx
  movl %edx, 56(%eax)  // x86_thread_state->uts.ts32.__ds
  movw %es, %dx
  movl %edx, 60(%eax)  // x86_thread_state->uts.ts32.__es
  movw %fs, %dx
  movl %edx, 64(%eax)  // x86_thread_state->uts.ts32.__fs
  movw %gs, %dx
  movl %edx, 68(%eax)  // x86_thread_state->uts.ts32.__gs

  // Clean up by restoring clobbered registers, even those considered volatile
  // by the ABI, so that the captured context represents the state at this
  // function’s exit.
  movl 20(%eax), %edx  // x86_thread_state->uts.ts32.__edx
  popl %eax
  popfl

  popl %ebp

  ret

  .cfi_endproc

#elif defined(__x86_64__)

  .cfi_startproc

  pushq %rbp
  .cfi_def_cfa_offset 16
  .cfi_offset %rbp, -16
  movq %rsp, %rbp
  .cfi_def_cfa_register %rbp

  // Note that 16-byte stack alignment is not maintained because this function
  // does not call out to any other.

  // pushfq first, because some instructions (but probably none used here)
  // affect %rflags. %rflags will be in -8(%rbp).
  pushfq

  // Initialize the header identifying the x86_thread_state_t structure as
  // carrying an x86_thread_state64_t (flavor x86_THREAD_STATE64) of size
  // x86_THREAD_STATE64_COUNT 32-bit values.
  movl $4, (%rdi)  // x86_thread_state->tsh.flavor
  movl $42, 4(%rdi)  // x86_thread_state->tsh.count

  // General-purpose registers whose values haven’t changed can be captured
  // directly.
  movq %rax, 8(%rdi)  // x86_thread_state->uts.ts64.__rax
  movq %rbx, 16(%rdi)  // x86_thread_state->uts.ts64.__rbx
  movq %rcx, 24(%rdi)  // x86_thread_state->uts.ts64.__rcx
  movq %rdx, 32(%rdi)  // x86_thread_state->uts.ts64.__rdx
  movq %rsi, 48(%rdi)  // x86_thread_state->uts.ts64.__rsi
  movq %r8, 72(%rdi)  // x86_thread_state->uts.ts64.__r8
  movq %r9, 80(%rdi)  // x86_thread_state->uts.ts64.__r9
  movq %r10, 88(%rdi)  // x86_thread_state->uts.ts64.__r10
  movq %r11, 96(%rdi)  // x86_thread_state->uts.ts64.__r11
  movq %r12, 104(%rdi)  // x86_thread_state->uts.ts64.__r12
  movq %r13, 112(%rdi)  // x86_thread_state->uts.ts64.__r13
  movq %r14, 120(%rdi)  // x86_thread_state->uts.ts64.__r14
  movq %r15, 128(%rdi)  // x86_thread_state->uts.ts64.__r15

  // Because of the calling convention, there’s no way to recover the value of
  // the caller’s %rdi as it existed prior to calling this function. This
  // function captures a snapshot of the register state at its return, which
  // involves %rdi containing a pointer to its first argument. Callers that
  // require the value of %rdi prior to calling this function should obtain it
  // separately. For example:
  //   uint64_t rdi;
  //   asm("movq %%rdi, %0" : "=m"(rdi));
  movq %rdi, 40(%rdi)  // x86_thread_state->uts.ts64.__rdi

  // Now that the original value of %rax has been saved, it can be repurposed to
  // hold other registers’ values.

  // The original %rbp was saved on the stack in this function’s prologue.
  movq (%rbp), %rax
  movq %rax, 56(%rdi)  // x86_thread_state->uts.ts64.__rbp

  // %rsp was saved in %rbp in this function’s prologue, but the caller’s %rsp
  // is 16 more than this value: 8 for the original %rbp saved on the stack in
  // this function’s prologue, and 8 for the return address saved on the stack
  // by the call instruction that reached this function.
  leaq 16(%rbp), %rax
  movq %rax, 64(%rdi)  // x86_thread_state->uts.ts64.__rsp

  // %rip can’t be accessed directly, but the return address saved on the stack
  // by the call instruction that reached this function can be used.
  movq 8(%rbp), %rax
  movq %rax, 136(%rdi)  // x86_thread_state->uts.ts64.__rip

  // The original %rflags was saved on the stack above.
  movq -8(%rbp), %rax
  movq %rax, 144(%rdi)  // x86_thread_state->uts.ts64.__rflags

  // The segment registers are 16 bits wide, but x86_thread_state declares them
  // as unsigned 64-bit values, so zero the top portion.
  xorq %rax, %rax
  movw %cs, %ax
  movq %rax, 152(%rdi)  // x86_thread_state->uts.ts64.__cs
  movw %fs, %ax
  movq %rax, 160(%rdi)  // x86_thread_state->uts.ts64.__fs
  movw %gs, %ax
  movq %rax, 168(%rdi)  // x86_thread_state->uts.ts64.__gs

  // Clean up by restoring clobbered registers, even those considered volatile
  // by the ABI, so that the captured context represents the state at this
  // function’s exit.
  movq 8(%rdi), %rax
  popfq

  popq %rbp

  ret

  .cfi_endproc

#elif defined(__aarch64__)

  .cfi_startproc

  // Save general-purpose registers in arm_unified_thread_state->ts_64.__x[0].
  // The original x0 can't be recovered.
  stp x0, x1, [x0, #0x8]
  stp x2, x3, [x0, #0x18]
  stp x4, x5, [x0, #0x28]
  stp x6, x7, [x0, #0x38]
  stp x8, x9, [x0, #0x48]
  stp x10, x11, [x0, #0x58]
  stp x12, x13, [x0, #0x68]
  stp x14, x15, [x0, #0x78]
  stp x16, x17, [x0, #0x88]
  stp x18, x19, [x0, #0x98]
  stp x20, x21, [x0, #0xa8]
  stp x22, x23, [x0, #0xb8]
  stp x24, x25, [x0, #0xc8]
  stp x26, x27, [x0, #0xd8]

  // Save the last general-purpose register (x28) and the frame pointer (x29).
  stp x28, x29, [x0, #0xe8]  // __x[28] and __fp

  // Save the link register (x30) and the stack pointer (using x1 as a scratch
  // register)
  mov x1, sp
  stp x30, x1, [x0, #0xf8]  // __lr and __sp

  // The link register (x30) holds the return address for this function.
  // __cpsr should hold current program status register, but nzcv are the only
  // bits we know about (saved using x1 as a scratch register). The 64-bit x1
  // covers both the 32-bit __cpsr (which receives the nzcv bits) and __pad
  // (which will be zeroed).
  mrs x1, nzcv
  stp x30, x1, [x0, #0x108]  // __pc and __cpsr and __pad

  // Initialize the header identifying the arm_unified_thread_state structure as
  // carrying an arm_thread_state64_t (flavor ARM_THREAD_STATE64) of size
  // ARM_THREAD_STATE64_COUNT 32-bit values.
  mov x1, #6
  movk x1, #68, lsl #32
  str x1, [x0, #0x0]  // arm_thread_state->ash.flavor and count

  // Restore x1 from the saved context.
  ldr x1, [x0, #0x10]

  // TODO(justincohen): Consider saving floating-point registers into
  // arm_neon_state64_t as second parameter, or as a a second function call
  // after all of the general-purpose state is captured, or as a new struct that
  // has both arm_unified_state_t and arm_neon_state64_t members. That may be
  // better than a second parameter (which occupies another register) and better
  // than a second function call.

  ret

  .cfi_endproc

#endif

.subsections_via_symbols

#endif
