// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_MEMORY_PARTITION_ALLOCATOR_LOOKUP_SYMBOL_H_
#define TOOLS_MEMORY_PARTITION_ALLOCATOR_LOOKUP_SYMBOL_H_

#include <libdwfl.h>
#include <unistd.h>

Dwfl* AddressLookupInit(pid_t pid);
void AddressLookupFinish(Dwfl* dwfl);
Dwarf_Die* LookupCompilationUnit(Dwfl* dwfl,
                                 Dwfl_Module* mod,
                                 const char* expected_name,
                                 unsigned long* bias_out);
void* LookupVariable(Dwarf_Die* scope,
                     unsigned long bias,
                     const char** namespace_path,
                     size_t namespace_path_length,
                     const char* name);

#endif  // TOOLS_MEMORY_PARTITION_ALLOCATOR_LOOKUP_SYMBOL_H_
