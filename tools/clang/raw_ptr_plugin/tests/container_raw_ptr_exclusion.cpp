// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/raw_ptr_exclusion.h"

#define ALLOW_DISCOURAGED_TYPE(type) \
  __attribute__((annotate("allow_discouraged_type"))) type

class SomeClass {};

// Type aliases with RAW_PTR_EXCLUSION.
// Note: RAW_PTR_EXCLUSION cannot be applied to `using` directives due to
// __attribute__ syntax limitations.
RAW_PTR_EXCLUSION typedef std::vector<SomeClass*> ExcludedTypedef;

// Control case: alias without exclusion (must still trigger an error).
using UnannotatedUsing = std::vector<SomeClass*>;

// Combined: multiple annotations on a type alias.
RAW_PTR_EXCLUSION ALLOW_DISCOURAGED_TYPE(typedef std::vector<SomeClass*>)
    ExcludedMultiAnnotatedTypedef;

// Chained alias pointing to an excluded typedef.
using ChainedExcludedUsing = ExcludedTypedef;

struct MyClass {
  // Baseline: field directly excluded.
  RAW_PTR_EXCLUSION std::vector<SomeClass*> excluded_field_;

  // Multiple annotations on field (order sensitivity check).
  RAW_PTR_EXCLUSION ALLOW_DISCOURAGED_TYPE(std::vector<SomeClass*>)
      excluded_multi_attr_first_;
  ALLOW_DISCOURAGED_TYPE(std::vector<SomeClass*>)
  RAW_PTR_EXCLUSION excluded_multi_attr_second_;

  // Excluded via type aliases.
  ExcludedTypedef excluded_typedef_;
  ExcludedMultiAnnotatedTypedef excluded_multi_annotated_typedef_;
  ChainedExcludedUsing excluded_chained_using_;

  // Errors expected:
  // 1. Direct container without exclusion.
  std::vector<SomeClass*> error_field_;

  // 2. Alias without exclusion (verifies aliases are not blanket-ignored).
  UnannotatedUsing error_unannotated_alias_;
};
