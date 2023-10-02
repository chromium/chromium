// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "optional_gc_object.h"

namespace blink {

class WithOpt : public GarbageCollected<WithOpt> {
 public:
  virtual void Trace(Visitor*) const {}

 private:
  absl::optional<Base> optional_field_;  // Optional fields are disallowed.
  std::optional<Base> optional_field2_;
};

void DisallowedUseOfOptional() {
  {
    absl::optional<Base> optional_base;  // Must be okay.
    (void)optional_base;

    absl::optional<Derived> optional_derived;  // Must also be okay.
    (void)optional_derived;

    new absl::optional<Base>;  // New expression with gced optionals are not
                               // allowed.
  }

  {
    std::optional<Base> optional_base;  // Must be okay.
    (void)optional_base;

    std::optional<Derived> optional_derived;  // Must also be okay.
    (void)optional_derived;

    new std::optional<Base>;  // New expression with gced optionals are not
                               // allowed.
  }
}

}  // namespace blink
