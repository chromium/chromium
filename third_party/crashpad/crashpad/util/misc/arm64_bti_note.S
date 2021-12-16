// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_MISC_ARM64_BTI_NOTE_S
#define CRASHPAD_UTIL_MISC_ARM64_BTI_NOTE_S

/* Support macros for the Armv8.5-A Branch Target Identification feature which
 * requires emitting a .note.gnu.property section with the appropriate
 * architecture-dependent feature bits set.
 * Read more: "ELF for the Arm® 64-bit Architecture"
 */
#if defined(__ARM_FEATURE_BTI_DEFAULT) && (__ARM_FEATURE_BTI_DEFAULT == 1)
#define GNU_PROPERTY_AARCH64_BTI (1 << 0)  // Has BTI
  .pushsection .note.gnu.property, "a"
  .balign 4
  .long 0x4  /* size of field "GNU" */
  .long 0x10 /* note descriptor size */
  .long 0x5  /* type of note descriptor: NT_GNU_PROPERTY_TYPE_0 */
  .asciz "GNU"
  .long 0xc0000000 /* GNU_PROPERTY_AARCH64_FEATURE_1_AND */
  .long 0x4
  .long GNU_PROPERTY_AARCH64_BTI
  .long 0x0
  .popsection
#define CRASHPAD_AARCH64_VALID_JUMP_CALL_TARGET bti jc
#define CRASHPAD_AARCH64_VALID_CALL_TARGET      bti c
#define CRASHPAD_AARCH64_VALID_JUMP_TARGET      bti j
#undef GNU_PROPERTY_AARCH64_BTI
#else
#define CRASHPAD_AARCH64_VALID_JUMP_CALL_TARGET
#define CRASHPAD_AARCH64_VALID_CALL_TARGET
#define CRASHPAD_AARCH64_VALID_JUMP_TARGET
#endif

#endif /* CRASHPAD_UTIL_MISC_ARM64_BTI_NOTE_S */
