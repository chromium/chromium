// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file (and other gen-*-test.cc files) tests generation of output for
// --field-filter-file and therefore the expectations file
// (gen-macros-expected.txt) needs to be compared against the raw output of the
// rewriter (rather than against the actual edits result).  This makes the test
// incompatible with other tests, which require passing --apply-edits switch to
// test_tool.py and so to disable the test it is named *-test.cc rather than
// *-original.cc.
//
// To run the test use tools/clang/rewrite_raw_ptr_fields/tests/run_all_tests.py

//////////////////////////////////////////////////////////////////////////////
// Based on build/linux/debian_bullseye_amd64-sysroot/usr/include/link.h
//
// We expect that |ptr_field| will be emitted as a candidate for
// --field-filter-file.
#include "base/containers/span.h"

struct Elf64_Dyn;

#define __ELF_NATIVE_CLASS 64
#define ElfW(type) _ElfW(Elf, __ELF_NATIVE_CLASS, type)
#define _ElfW(e, w, t) _ElfW_1(e, w, _##t)
#define _ElfW_1(e, w, t) e##w##t

struct MacroTest1 {
  ElfW(Dyn) * ptr_field;
};

//////////////////////////////////////////////////////////////////////////////
// Based on base/third_party/libevent/event.h
//
// We expect that |tqe_next| and |tqe_prev| fields below will both be emitted as
// candidates for --field-filter-file.
//
// This test is also interesting for noting that a fully-qualified name of a
// field decl is not sufficient to uniquely identify a field.  In the test below
// there are 3 anonymous structs and all 3 have the following fields:
//   MacroTest2::(anonymous struct)::tqe_next
//   MacroTest2::(anonymous struct)::tqe_prev

struct event;
struct event_base;

#define TAILQ_ENTRY(type)                                          \
  struct {                                                         \
    struct type* tqe_next;  /* next element */                     \
    struct type** tqe_prev; /* address of previous next element */ \
    base::span<struct type> span_field;                            \
  }

struct MacroTest2 {
  TAILQ_ENTRY(event) ev_next;
  TAILQ_ENTRY(event) ev_active_next;
  TAILQ_ENTRY(event) ev_signal_next;
};

namespace raw_ref_test {
struct Elf64_Dyn;

#define __ELF_NATIVE_CLASS 64
#define ElfW(type) _ElfW(Elf, __ELF_NATIVE_CLASS, type)
#define _ElfW(e, w, t) _ElfW_1(e, w, _##t)
#define _ElfW_1(e, w, t) e##w##t

struct MacroTest1 {
  ElfW(Dyn) & ref_field;
};

struct event;
struct event_base;
// anonymous structs are not matched because they don't compile when trying to
// set raw_ref members using initializer list.
#define TAILQ_ENTRY2(type) \
  struct {                 \
    struct type& tqe_next; \
    struct type& tqe_prev; \
  }

struct MacroTest2 {
  TAILQ_ENTRY2(event) ev_next;
  TAILQ_ENTRY2(event) ev_active_next;
  TAILQ_ENTRY2(event) ev_signal_next;
};

}  // namespace raw_ref_test