/*
 * Copyright (C) 2014 The Android Open Source Project
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

#pragma once

namespace unwindstack {

#include <sys/cdefs.h>

typedef uint64_t __riscv_mc_gp_state[32];  // unsigned long

struct __riscv_mc_f_ext_state {
  uint32_t __f[32];
  uint32_t __fcsr;
};

struct __riscv_mc_d_ext_state {
  uint64_t __f[32];
  uint32_t __fcsr;
};

struct __riscv_mc_q_ext_state {
  uint64_t __f[64] __attribute__((__aligned__(16)));
  uint32_t __fcsr;
  uint32_t __reserved[3];
};

union __riscv_mc_fp_state {
  struct __riscv_mc_f_ext_state __f;
  struct __riscv_mc_d_ext_state __d;
  struct __riscv_mc_q_ext_state __q;
};

struct __riscv_stack_t {
  uint64_t ss_sp;
  int32_t ss_flags;
  uint64_t ss_size;
};

struct riscv64_sigset_t {
  uint64_t sig;  // unsigned long
};

struct riscv64_mcontext_t {
  __riscv_mc_gp_state __gregs;
  union __riscv_mc_fp_state __fpregs;
};

struct riscv64_ucontext_t {
  uint64_t uc_flags;  // unsigned long
  struct riscv64_ucontext_t* uc_link;
  __riscv_stack_t uc_stack;
  riscv64_sigset_t uc_sigmask;
  /* The kernel adds extra padding here to allow sigset_t to grow. */
  int8_t __padding[128 - sizeof(riscv64_sigset_t)];  // char
  riscv64_mcontext_t uc_mcontext;
};

}  // namespace unwindstack
