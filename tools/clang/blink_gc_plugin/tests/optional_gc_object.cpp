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
  absl::optional<Traceable>
      optional_field3_;  // Optional fields are disallowed.
  std::optional<Traceable> optional_field4_;
  absl::optional<Member<Base>> optional_field5_;
  std::optional<Member<Base>> optional_field6_;
  base::raw_ptr<Base> raw_ptr_field_;
  base::raw_ptr<Traceable> raw_ptr_field2_;
  base::raw_ref<Base> raw_ref_field_;
  base::raw_ref<Traceable> raw_ref_field2_;
};

void DisallowedUseOfOptional() {
  {
    absl::optional<Base> optional_base;
    (void)optional_base;

    absl::optional<Derived> optional_derived;
    (void)optional_derived;

    absl::optional<Traceable> optional_traceable;  // Must be okay.
    (void)optional_traceable;

    absl::optional<Member<Base>> optional_member;  // Must be okay.
    (void)optional_member;

    new absl::optional<Base>;  // New expression with gced optionals are not
                               // allowed.

    new absl::optional<Traceable>;  // New expression with traceable optionals
                                    // are not allowed.

    new absl::optional<Member<Base>>;
  }

  {
    std::optional<Base> optional_base;
    (void)optional_base;

    std::optional<Derived> optional_derived;
    (void)optional_derived;

    std::optional<Traceable> optional_traceable;  // Must be okay.
    (void)optional_traceable;

    std::optional<Member<Base>> optional_member;  // Must be okay.
    (void)optional_member;

    new std::optional<Base>;  // New expression with gced optionals are not
                               // allowed.

    new std::optional<Traceable>;  // New expression with traceable optionals
                                   // are not allowed.

    new std::optional<Member<Base>>;
  }

  {
    base::raw_ptr<Base> raw_ptr_base;
    (void)raw_ptr_base;

    base::raw_ptr<Derived> raw_ptr_derived;
    (void)raw_ptr_derived;

    base::raw_ptr<Traceable> raw_ptr_traceable;
    (void)raw_ptr_traceable;

    new base::raw_ptr<Base>;  // New expression with gced raw_ptrs are not
                              // allowed.

    new base::raw_ptr<Traceable>;  // New expression with traceable raw_ptrs
                                   // are not allowed.
  }

  {
    base::raw_ref<Base> raw_ref_base;
    (void)raw_ref_base;

    base::raw_ref<Derived> raw_ref_derived;
    (void)raw_ref_derived;

    base::raw_ref<Traceable> raw_ref_traceable;
    (void)raw_ref_traceable;

    new base::raw_ref<Base>;  // New expression with gced raw_refs are not
                              // allowed.

    new base::raw_ref<Traceable>;  // New expression with traceable raw_refs
                                   // are not allowed.
  }
}

class OnStack {
  STACK_ALLOCATED();

 public:
  OnStack() {
    (void)optional_field_;
    (void)optional_field2_;
    (void)optional_field3_;
    (void)optional_field4_;
    (void)optional_field5_;
    (void)optional_field6_;
    (void)raw_ptr_field_;
    (void)raw_ptr_field2_;
    (void)raw_ref_field_;
    (void)raw_ref_field2_;
  }

 private:
  // All fields are ok since the class is stack allocated.
  absl::optional<Base> optional_field_;
  std::optional<Base> optional_field2_;
  absl::optional<Traceable> optional_field3_;
  std::optional<Traceable> optional_field4_;
  absl::optional<Member<Base>> optional_field5_;
  std::optional<Member<Base>> optional_field6_;
  base::raw_ptr<Base> raw_ptr_field_;
  base::raw_ptr<Traceable> raw_ptr_field2_;
  base::raw_ref<Base> raw_ref_field_;
  base::raw_ref<Traceable> raw_ref_field2_;
};

}  // namespace blink
