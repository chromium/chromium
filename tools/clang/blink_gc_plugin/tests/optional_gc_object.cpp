// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "optional_gc_object.h"

namespace blink {

class WithOpt : public GarbageCollected<WithOpt> {
 public:
  virtual void Trace(Visitor*) const {}

 private:
  base::Optional<Base> optional_field_;  // Optional fields are disallowed.
};

void DisallowedUseOfUniquePtr() {
  base::Optional<Base> optional_base;  // Must be okay.
  (void)optional_base;

  base::Optional<Derived> optional_derived;  // Must also be okay.
  (void)optional_derived;

  new base::Optional<Base>;  // New expression with gced optionals are not
                             // allowed.
}

}  // namespace blink
