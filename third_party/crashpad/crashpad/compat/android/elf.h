// Copyright 2017 The Crashpad Authors
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

#ifndef CRASHPAD_COMPAT_ANDROID_ELF_H_
#define CRASHPAD_COMPAT_ANDROID_ELF_H_

#include_next <elf.h>

// https://github.com/android/ndk/issues/2239
#if !defined(ELF32_ST_VISIBILITY)
#define ELF32_ST_VISIBILITY(other) ((other) & 0x3)
#endif

#if !defined(ELF64_ST_VISIBILITY)
#define ELF64_ST_VISIBILITY(other) ELF32_ST_VISIBILITY(other)
#endif

#endif  // CRASHPAD_COMPAT_ANDROID_ELF_H_
