// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/partition_allocator/lookup_symbol.h"

#include "base/check_op.h"
#include "base/logging.h"

namespace {

// From dwarf.h:
/* DWARF tags.  */
enum {
  DW_TAG_class_type = 0x02,
  DW_TAG_structure_type = 0x13,
  DW_TAG_variable = 0x34,
  DW_TAG_namespace = 0x39,
};

/* Children determination encodings.  */
enum { DW_CHILDREN_no = 0, DW_CHILDREN_yes = 1 };

/* DWARF attributes encodings.  */
enum {
  DW_AT_location = 0x02,
};

/* DWARF location operation encodings.  */
enum {
  DW_OP_addr = 0x03, /* Constant address.  */
};

int LookupNamespacedName(Dwarf_Die* scope,
                         const char** namespaces,
                         size_t namespaces_len,
                         const char* expected_name,
                         unsigned int expected_tag,
                         Dwarf_Die* result) {
  Dwarf_Die child;
  int res = dwarf_child(scope, &child);
  if (res)
    return res;

  while (true) {
    unsigned int tag = dwarf_tag(&child);
    const char* name = dwarf_diename(&child);
    if (namespaces_len) {
      const char* ns = namespaces[0];
      if (tag == DW_TAG_namespace &&
          (ns ? (name && strcmp(name, ns) == 0) : (name == nullptr))) {
        res = LookupNamespacedName(&child, namespaces + 1, namespaces_len - 1,
                                   expected_name, expected_tag, result);
        if (res <= 0)
          return res;
      }
    } else {
      if (expected_name) {
        if ((tag == expected_tag || (expected_tag == DW_TAG_structure_type &&
                                     tag == DW_TAG_class_type)) &&
            name && strcmp(expected_name, name) == 0) {
          *result = child;
          return 0;
        }
      } else {
        LOG(INFO) << "Got child " << name << ", tag " << tag;
      }
    }
    res = dwarf_siblingof(&child, &child);
    if (res)
      return res;
  }
  return res;
}

uintptr_t GetDieAddress(Dwarf_Die* die, unsigned long cu_bias) {
  Dwarf_Attribute loc_attr;
  if (dwarf_attr(die, DW_AT_location, &loc_attr) == nullptr)
    return 0;
  Dwarf_Op* loc_expr;
  size_t loc_expr_len;
  if (dwarf_getlocation(&loc_attr, &loc_expr, &loc_expr_len) != 0)
    return 0;

  if (loc_expr_len == 1 && loc_expr[0].atom == DW_OP_addr) {
    return cu_bias + loc_expr[0].number;
  } else {
    return 0;
  }
}

}  // namespace

Dwfl* AddressLookupInit(pid_t pid) {
  static const Dwfl_Callbacks proc_callbacks = {
      .find_elf = dwfl_linux_proc_find_elf,
      .find_debuginfo = dwfl_standard_find_debuginfo};

  Dwfl* dwfl = dwfl_begin(&proc_callbacks);
  CHECK(dwfl);

  CHECK(!dwfl_linux_proc_report(dwfl, pid));
  CHECK(!dwfl_report_end(dwfl, nullptr, nullptr));

  return dwfl;
}

void AddressLookupFinish(Dwfl* dwfl) {
  dwfl_end(dwfl);
}

Dwarf_Die* LookupCompilationUnit(Dwfl* dwfl,
                                 Dwfl_Module* mod,
                                 const char* expected_name,
                                 unsigned long* bias_out) {
  Dwarf_Die* cu = nullptr;
  Dwarf_Addr bias;
  Dwarf_Die* result = NULL;
  while ((cu = (mod ? dwfl_module_nextcu(mod, cu, &bias)
                    : dwfl_nextcu(dwfl, cu, &bias))) != nullptr) {
    const char* name = dwarf_diename(cu);
    if (expected_name == NULL) {
      LOG(INFO) << "Compilation Unit = " << name;
      continue;
    }
    if (strcmp(name, expected_name) == 0) {
      CHECK(!result) << "duplicate CU " << expected_name;
      result = cu;
      *bias_out = bias;
    }
  }

  CHECK(result) << "Didn't find " << expected_name;
  return result;
}

void* LookupVariable(Dwarf_Die* scope,
                     unsigned long bias,
                     const char** namespace_path,
                     size_t namespace_path_length,
                     const char* name) {
  Dwarf_Die var_die;
  LookupNamespacedName(scope, namespace_path, namespace_path_length, name,
                       DW_TAG_variable, &var_die);
  return reinterpret_cast<void*>(GetDieAddress(&var_die, bias));
}
