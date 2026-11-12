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

#pragma once

#include <stdint.h>

#include <unwindstack/MachineArm64.h>

namespace unwindstack {

struct arm64_stack_t {
  uint64_t ss_sp;    // void __user*
  int32_t ss_flags;  // int
  uint64_t ss_size;  // size_t
};

struct arm64_sigset_t {
  uint64_t sig;  // unsigned long
};

struct arm64_mcontext_t {
  uint64_t fault_address;         // __u64
  uint64_t regs[ARM64_REG_LAST];  // __u64
  uint64_t pstate;                // __u64
  // Nothing else is used, so don't define it.
};

struct arm64_ucontext_t {
  uint64_t uc_flags;  // unsigned long
  uint64_t uc_link;   // struct ucontext*
  arm64_stack_t uc_stack;
  arm64_sigset_t uc_sigmask;
  // The kernel adds extra padding after uc_sigmask to match glibc sigset_t on ARM64.
  char __padding[128 - sizeof(arm64_sigset_t)];
  // The full structure requires 16 byte alignment, but our partial structure
  // doesn't, so force the alignment.
  arm64_mcontext_t uc_mcontext __attribute__((aligned(16)));
};

}  // namespace unwindstack
