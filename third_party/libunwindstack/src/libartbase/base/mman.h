/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBARTBASE_BASE_MMAN_H_
#define ART_LIBARTBASE_BASE_MMAN_H_

#ifdef _WIN32

// There is no sys/mman.h in mingw.

#define PROT_READ      0x1
#define PROT_WRITE     0x2
#define PROT_EXEC      0x4
#define PROT_NONE      0x0

#define MAP_SHARED     0x01
#define MAP_PRIVATE    0x02

#define MAP_FAILED     ((void*) -1)
#define MAP_FIXED      0x10
#define MAP_ANONYMOUS  0x20

#else

#include <sys/mman.h>

#endif


#endif  // ART_LIBARTBASE_BASE_MMAN_H_
