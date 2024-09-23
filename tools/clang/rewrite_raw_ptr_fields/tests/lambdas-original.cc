// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

class MyClass {
  // Lambdas are backed by a class that may have (depending on what the lambda
  // captures) pointer fields.  The rewriter should ignore such fields (since
  // they don't have an equivalent in source code).
  void foo() {
    int x = 123;

    // No rewrite expected in the two lamdas below.
    auto lambda1 = [this]() -> int { return 123; };
    auto lambda2 = [&]() -> int { return x; };

    // Nested structs defined within a lambda should be rewritten.  This test
    // helps ensure that |implicit_field_decl_matcher| uses |hasParent| to match
    // |isLambda|, rather than |hasAncestor|.
    auto lambda = [&]() -> int {
      struct NestedStruct {
        NestedStruct(int& n) : ref_field(n) {}
        // Expected rewrite: raw_ptr<int> ptr_field;
        int* ptr_field;
        // Expected rewrite: const raw_ref<int> ref_field;
        int& ref_field;
        // Expected rewrite: base::raw_span<int> span_field;
        base::span<int> span_field;
      } var(x);
      var.ptr_field = &x;

      return x;
    };
  }
};
