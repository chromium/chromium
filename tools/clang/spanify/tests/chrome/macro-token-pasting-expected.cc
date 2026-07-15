// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

int UnsafeIndex();

// Expected rewrite:
// #define RETURN_PASTE(a, b) \
//   return a##b.subspan(base::checked_cast<size_t>(UnsafeIndex())).data()
#define RETURN_PASTE(a, b) \
  return a##b.subspan(base::checked_cast<size_t>(UnsafeIndex())).data()

// Tests that pointer arithmetic in a macro using token-pasting (##) triggers
// spanification without crashing on virtual <scratch space> tokens during AST
// resolution.
//
// Expected rewrite:
// int* test_token_pasting(base::span<int> ptr)
int* test_token_pasting(base::span<int> ptr) {
  RETURN_PASTE(p, tr);
}

#define PASTE(x) x##_ptr
#define MACRO_OP(a, idx) a + idx

// Tests that variables generated in virtual <scratch space> via token pasting
// (##) in a macro binary operation are safely excluded via EmitExclusion().
int* test_token_pasting_scratch_space() {
  int* PASTE(my);
  // No expected rewrite because it is in scratch space.
  return MACRO_OP(my_ptr, UnsafeIndex());
}

// Expected rewrite:
// void test_exclusion_propagation(base::span<int> valid_ptr) {
void test_exclusion_propagation(base::span<int> valid_ptr) {
  int* PASTE(my);  // Expands to: int* my_ptr;

  // Assignment where LHS is scratch space.
  // Expected rewrite:
  // my_ptr = valid_ptr.data();
  my_ptr = valid_ptr.data();

  // Unsafe access on valid_ptr to trigger its spanification.
  valid_ptr[1] = 0;
}

#define PASTE_INNER(x) x##_helper
#define GLUE(x, y) x##y
#define PASTE_OUTER_HELPER(x, y) GLUE(x, y)
#define PASTE_OUTER(x) PASTE_OUTER_HELPER(PASTE_INNER(x), _ptr)

void test_nested_token_pasting_crash() {
  // Generates 'my_var_helper_ptr'. The nested expansion forces the
  // expansion location itself to be in virtual scratch space.
  int* PASTE_OUTER(my_var) = nullptr;
}

// Tests that pointer arithmetic in a macro RHS using token pasting (##)
// triggers EmitExclusion() when GetReplacementDirective() returns empty,
// safely excluding valid_ptr from partial/corrupted spanification.
//
// Expected rewrite:
// No expected rewrite because valid_ptr is excluded due to RHS scratch space.
#define PASTE_NUM(x) x##_num
int* test_token_pasting_rhs(int* valid_ptr) {
  valid_ptr[1] = 0;                  // force spanification of valid_ptr
  int PASTE_NUM(my) = 1;             // expands to int my_num = 1;
  return valid_ptr + PASTE_NUM(my);  // expands to valid_ptr + my_num
}
