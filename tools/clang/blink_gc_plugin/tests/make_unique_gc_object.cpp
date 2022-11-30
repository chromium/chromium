// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "make_unique_gc_object.h"

namespace blink {

void DisallowedUseOfUniquePtr() {
  auto owned_base = std::make_unique<Base>();
  auto owned_base_array = std::make_unique<Base[]>(1);
  auto owned_derived = std::make_unique<Derived>();
  auto owned_mixin = base::WrapUnique(static_cast<Mixin*>(nullptr));
}

}  // namespace blink
