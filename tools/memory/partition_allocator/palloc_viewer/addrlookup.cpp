// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <dwarf.h>
#include <elfutils/libdwfl.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include "common.h"

static int lookup_namespaced_name_(Dwarf_Die* scope,
                                   const char** namespaces,
                                   size_t namespaces_len,
                                   const char* expected_name,
                                   unsigned int expected_tag,
                                   Dwarf_Die* result) {
  Dwarf_Die child;
  int res = dwarf_child(scope, &child);
  if (res)
    return res;
  while (1) {
    unsigned int tag = dwarf_tag(&child);
    const char* name = dwarf_diename(&child);
    if (namespaces_len) {
      const char* ns = namespaces[0];
      if (tag == DW_TAG_namespace &&
          (ns ? (name && strcmp(name, ns) == 0) : (name == NULL))) {
        res =
            lookup_namespaced_name_(&child, namespaces + 1, namespaces_len - 1,
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
        // for debugging
        printf("got child '%s', tag 0x%x\n", name, tag);
      }
    }
    res = dwarf_siblingof(&child, &child);
    if (res)
      return res;
  }
  return 1;
}

static void lookup_namespaced_name(Dwarf_Die* scope,
                                   const char** namespaces,
                                   size_t namespaces_len,
                                   const char* expected_name,
                                   unsigned int expected_tag,
                                   Dwarf_Die* result) {
  if (lookup_namespaced_name_(scope, namespaces, namespaces_len, expected_name,
                              expected_tag, result) != 0)
    errx(1, "lookup of '%s' failed", expected_name);
}

static void lookup_name(Dwarf_Die* scope,
                        const char* expected_name,
                        unsigned int tag,
                        Dwarf_Die* result) {
  lookup_namespaced_name(scope, NULL, 0, expected_name, tag, result);
}

void* lookup_cu(Dwfl* dwfl,
                Dwfl_Module* mod,
                const char* expected_name,
                unsigned long* bias_out) {
  fprintf(stderr, "looking up CU '%s'...\n", expected_name);
  Dwarf_Die* cu = NULL;
  Dwarf_Addr bias;
  Dwarf_Die* result = NULL;
  while ((cu = (mod ? dwfl_module_nextcu(mod, cu, &bias)
                    : dwfl_nextcu(dwfl, cu, &bias))) != NULL) {
    const char* name = dwarf_diename(cu);
    if (expected_name == NULL) {
      // for debugging
      printf("CU: %s\n", name);
      continue;
    }
    if (strcmp(name, expected_name) == 0) {
      if (result)
        errx(1, "duplicate CU '%s'", expected_name);
      result = cu;
      *bias_out = bias;
    }
  }
  if (!result)
    errx(1, "unable to find CU '%s'", expected_name);
  fprintf(stderr, "CU lookup complete\n");
  return (void*)result;
}

static unsigned long get_die_address(Dwarf_Die* die, unsigned long cu_bias) {
  Dwarf_Attribute loc_attr;
  if (dwarf_attr(die, DW_AT_location, &loc_attr) == NULL)
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

#if 0
// for debug
static int print_attrs_cb(Dwarf_Attribute *attr, void *arg) {
  printf("attribute: 0x%x\n", attr->code);
  if (attr->code == DW_AT_data_member_location)
    printf("  DW_AT_data_member_location\n");
  Dwarf_Word uval;
  if (dwarf_formudata(attr, &uval) == 0)
    printf("  unsigned value: 0x%lx\n", uval);
  return 0;
}

// for debug
static int print_modules_cb(Dwfl_Module *mod, void **mod_userdata, const char *name, Dwarf_Addr low_addr, void *arg) {
  printf("module: %s\n", name);
  return 0;
}
#endif

struct find_lib_data {
  Dwfl_Module* res;
  const char* name;
};
static int find_lib_cb(Dwfl_Module* mod,
                       void** mod_userdata,
                       const char* name,
                       Dwarf_Addr low_addr,
                       void* arg) {
  struct find_lib_data* data = (struct find_lib_data*)arg;
  if ((strncmp(name, "/lib/", 5) == 0 || strncmp(name, "/usr/", 5) == 0) &&
      strstr(name, data->name)) {
    if (data->res)
      errx(1, "two %s mappings?", data->name);
    data->res = mod;
  }
  return 0;
}
Dwfl_Module* addrlookup_find_lib(Dwfl* dwfl, const char* name) {
  struct find_lib_data data = {.res = NULL, .name = name};
  if (dwfl_getmodules(dwfl, find_lib_cb, &data, 0))
    return NULL;
  if (!data.res)
    errx(1, "no %s found", name);
  return data.res;
}

static unsigned long read_udata_dwarf_attr(Dwarf_Die* die, unsigned int name) {
  Dwarf_Attribute attr;
  if (dwarf_attr(die, name, &attr) == NULL)
    err(1, "unable to find requested attr 0x%x", name);
  Dwarf_Word value;
  if (dwarf_formudata(&attr, &value))
    err(1, "requested attr 0x%x is not a constant?", name);
  return value;
}

unsigned long addrlookup_get_struct_offset(void* scope,
                                           const char** namespaces,
                                           size_t namespaces_len,
                                           const char* struct_name,
                                           const char* member_name) {
  Dwarf_Die struct_die;
  lookup_namespaced_name((Dwarf_Die*)scope, namespaces, namespaces_len,
                         struct_name, DW_TAG_structure_type, &struct_die);
  Dwarf_Die member_die;
  lookup_name(&struct_die, member_name, DW_TAG_member, &member_die);
  return read_udata_dwarf_attr(&member_die, DW_AT_data_member_location);
}

unsigned long addrlookup_get_variable_address(void* scope,
                                              unsigned long cu_bias,
                                              const char** namespaces,
                                              size_t namespaces_len,
                                              const char* name) {
  Dwarf_Die var_die;
  lookup_namespaced_name((Dwarf_Die*)scope, namespaces, namespaces_len, name,
                         DW_TAG_variable, &var_die);
  return get_die_address(&var_die, cu_bias);
}

Dwfl* addrlookup_init(pid_t pid) {
  fprintf(stderr, "initializing DWFL for pid %d\n", pid);
  static const Dwfl_Callbacks proc_callbacks = {
      .find_elf = dwfl_linux_proc_find_elf,
      .find_debuginfo = dwfl_standard_find_debuginfo};
  Dwfl* dwfl = dwfl_begin(&proc_callbacks);
  if (!dwfl)
    err(1, "dwfl_begin");

  if (dwfl_linux_proc_report(dwfl, pid))
    errx(1, "proc_report");
  if (dwfl_report_end(dwfl, NULL, NULL))
    errx(1, "report_end");
  fprintf(stderr, "DWFL init complete\n");
  return dwfl;
}

void addrlookup_finish(Dwfl* dwfl) {
  dwfl_end(dwfl);
}

#if 0
int main(int argc, char **argv) {
  if (argc != 2)
    errx(1, "args");
  int pid = atoi(argv[1]);

  addrlookup_init(pid);

  Dwfl_Module *libpthread_module = addrlookup_find_lib("/libpthread-");
  Dwarf_Addr pthread_bias;
  Dwarf_Die *pthread_cu = lookup_cu(libpthread_module, "pthread_getspecific.c", &pthread_bias);

  unsigned long pthread_block_offset = addrlookup_get_struct_offset(pthread_cu, NULL, 0, "pthread", "specific_1stblock");
  printf("pthread_block_offset=0x%lx\n", pthread_block_offset);

  Dwarf_Addr bias;
  Dwarf_Die *cu = lookup_cu(NULL, "../../base/allocator/partition_allocator/src/partition_alloc/thread_cache.cc", &bias);

  const char *nspath[] = { "base", "internal", NULL };
  unsigned long g_instance_addr = addrlookup_get_variable_address(cu, bias, nspath, 3, "g_instance");
  printf("g_instance at 0x%lx\n", g_instance_addr);

  printf("end\n");
  return 0;
}
#endif
