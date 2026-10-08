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
  absl::optional<Base*> optional_field7_;
  std::optional<Base*> optional_field8_;
  absl::optional<Traceable*> optional_field9_;
  std::optional<Traceable*> optional_field10_;
  absl::optional<Member<Base>*> optional_field11_;
  std::optional<Member<Base>*> optional_field12_;
  absl::optional<Base&> optional_field13_;
  std::optional<Base&> optional_field14_;
  absl::optional<Traceable&> optional_field15_;
  std::optional<Traceable&> optional_field16_;
  absl::optional<Member<Base>&> optional_field17_;
  std::optional<Member<Base>&> optional_field18_;
  base::raw_ptr<Base> raw_ptr_field_;
  base::raw_ptr<Traceable> raw_ptr_field2_;
  base::raw_ptr<Member<Base>> raw_ptr_field3_;
  base::raw_ptr<Base*> raw_ptr_field4_;
  base::raw_ptr<Traceable*> raw_ptr_field5_;
  base::raw_ptr<Member<Base>*> raw_ptr_field6_;
  base::raw_ptr<Base&> raw_ptr_field7_;
  base::raw_ptr<Traceable&> raw_ptr_field8_;
  base::raw_ptr<Member<Base>&> raw_ptr_field9_;
  base::raw_ref<Base> raw_ref_field_;
  base::raw_ref<Traceable> raw_ref_field2_;
  base::raw_ref<Member<Base>> raw_ref_field3_;
  base::raw_ref<Base*> raw_ref_field4_;
  base::raw_ref<Traceable*> raw_ref_field5_;
  base::raw_ref<Member<Base>*> raw_ref_field6_;
  base::raw_ref<Base&> raw_ref_field7_;
  base::raw_ref<Traceable&> raw_ref_field8_;
  base::raw_ref<Member<Base>&> raw_ref_field9_;
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

    absl::optional<Base*> optional_base_ptr;  // Must be okay.
    (void)optional_base_ptr;

    absl::optional<Traceable*> optional_traceable_ptr;  // Must be okay.
    (void)optional_traceable_ptr;

    absl::optional<Member<Base>*> optional_member_ptr;  // Must be okay.
    (void)optional_member_ptr;

    absl::optional<Base&> optional_base_ref;  // Must be okay.
    (void)optional_base_ref;

    absl::optional<Traceable&> optional_traceable_ref;  // Must be okay.
    (void)optional_traceable_ref;

    absl::optional<Member<Base>&> optional_member_ref;  // Must be okay.
    (void)optional_member_ref;

    new absl::optional<Base>;  // New expression with gced optionals are not
                               // allowed.

    new absl::optional<Traceable>;  // New expression with traceable optionals
                                    // are not allowed.

    new absl::optional<Member<Base>>;

    new absl::optional<Base*>;

    new absl::optional<Traceable*>;

    new absl::optional<Member<Base>*>;

    new absl::optional<Base&>;

    new absl::optional<Traceable&>;

    new absl::optional<Member<Base>&>;
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

    std::optional<Base*> optional_base_ptr;  // Must be okay.
    (void)optional_base_ptr;

    std::optional<Traceable*> optional_traceable_ptr;  // Must be okay.
    (void)optional_traceable_ptr;

    std::optional<Member<Base>*> optional_member_ptr;  // Must be okay.
    (void)optional_member_ptr;

    std::optional<Base&> optional_base_ref;  // Must be okay.
    (void)optional_base_ref;

    std::optional<Traceable&> optional_traceable_ref;  // Must be okay.
    (void)optional_traceable_ref;

    std::optional<Member<Base>&> optional_member_ref;  // Must be okay.
    (void)optional_member_ref;

    new std::optional<Base>;  // New expression with gced optionals are not
                               // allowed.

    new std::optional<Traceable>;  // New expression with traceable optionals
                                   // are not allowed.

    new std::optional<Member<Base>>;

    new std::optional<Base*>;

    new std::optional<Traceable*>;

    new std::optional<Member<Base>*>;

    new std::optional<Base&>;

    new std::optional<Traceable&>;

    new std::optional<Member<Base>&>;
  }

  {
    base::raw_ptr<Base> raw_ptr_base;
    (void)raw_ptr_base;

    base::raw_ptr<Derived> raw_ptr_derived;
    (void)raw_ptr_derived;

    base::raw_ptr<Traceable> raw_ptr_traceable;
    (void)raw_ptr_traceable;

    base::raw_ptr<Member<Base>> raw_ptr_member;
    (void)raw_ptr_member;

    base::raw_ptr<Base*> raw_ptr_base_ptr;
    (void)raw_ptr_base_ptr;

    base::raw_ptr<Traceable*> raw_ptr_traceable_ptr;
    (void)raw_ptr_traceable_ptr;

    base::raw_ptr<Member<Base>*> raw_ptr_member_ptr;
    (void)raw_ptr_member_ptr;

    base::raw_ptr<Base&> raw_ptr_base_ref;
    (void)raw_ptr_base_ref;

    base::raw_ptr<Traceable&> raw_ptr_traceable_ref;
    (void)raw_ptr_traceable_ref;

    base::raw_ptr<Member<Base>&> raw_ptr_member_ref;
    (void)raw_ptr_member_ref;

    new base::raw_ptr<Base>;  // New expression with gced raw_ptrs are not
                              // allowed.

    new base::raw_ptr<Traceable>;  // New expression with traceable raw_ptrs
                                   // are not allowed.

    new base::raw_ptr<Member<Base>>;

    new base::raw_ptr<Base*>;

    new base::raw_ptr<Traceable*>;

    new base::raw_ptr<Member<Base>*>;

    new base::raw_ptr<Base&>;

    new base::raw_ptr<Traceable&>;

    new base::raw_ptr<Member<Base>&>;
  }

  {
    base::raw_ref<Base> raw_ref_base;
    (void)raw_ref_base;

    base::raw_ref<Derived> raw_ref_derived;
    (void)raw_ref_derived;

    base::raw_ref<Traceable> raw_ref_traceable;
    (void)raw_ref_traceable;

    base::raw_ref<Member<Base>> raw_ref_member;
    (void)raw_ref_member;

    base::raw_ref<Base*> raw_ref_base_ptr;
    (void)raw_ref_base_ptr;

    base::raw_ref<Traceable*> raw_ref_traceable_ptr;
    (void)raw_ref_traceable_ptr;

    base::raw_ref<Member<Base>*> raw_ref_member_ptr;
    (void)raw_ref_member_ptr;

    base::raw_ref<Base&> raw_ref_base_ref;
    (void)raw_ref_base_ref;

    base::raw_ref<Traceable&> raw_ref_traceable_ref;
    (void)raw_ref_traceable_ref;

    base::raw_ref<Member<Base>&> raw_ref_member_ref;
    (void)raw_ref_member_ref;

    new base::raw_ref<Base>;  // New expression with gced raw_refs are not
                              // allowed.

    new base::raw_ref<Traceable>;  // New expression with traceable raw_refs
                                   // are not allowed.

    new base::raw_ref<Member<Base>>;

    new base::raw_ref<Base*>;

    new base::raw_ref<Traceable*>;

    new base::raw_ref<Member<Base>*>;

    new base::raw_ref<Base&>;

    new base::raw_ref<Traceable&>;

    new base::raw_ref<Member<Base>&>;
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
    (void)optional_field7_;
    (void)optional_field8_;
    (void)optional_field9_;
    (void)optional_field10_;
    (void)optional_field11_;
    (void)optional_field12_;
    (void)optional_field13_;
    (void)optional_field14_;
    (void)optional_field15_;
    (void)optional_field16_;
    (void)optional_field17_;
    (void)optional_field18_;
    (void)raw_ptr_field_;
    (void)raw_ptr_field2_;
    (void)raw_ptr_field3_;
    (void)raw_ptr_field4_;
    (void)raw_ptr_field5_;
    (void)raw_ptr_field6_;
    (void)raw_ptr_field7_;
    (void)raw_ptr_field8_;
    (void)raw_ptr_field9_;
    (void)raw_ref_field_;
    (void)raw_ref_field2_;
    (void)raw_ref_field3_;
    (void)raw_ref_field4_;
    (void)raw_ref_field5_;
    (void)raw_ref_field6_;
    (void)raw_ref_field7_;
    (void)raw_ref_field8_;
    (void)raw_ref_field9_;
  }

 private:
  // Optional fields (except GCed by value) are ok since the class is stack
  // allocated.
  absl::optional<Base> optional_field_;
  std::optional<Base> optional_field2_;
  absl::optional<Traceable> optional_field3_;
  std::optional<Traceable> optional_field4_;
  absl::optional<Member<Base>> optional_field5_;
  std::optional<Member<Base>> optional_field6_;
  absl::optional<Base*> optional_field7_;
  std::optional<Base*> optional_field8_;
  absl::optional<Traceable*> optional_field9_;
  std::optional<Traceable*> optional_field10_;
  absl::optional<Member<Base>*> optional_field11_;
  std::optional<Member<Base>*> optional_field12_;
  absl::optional<Base&> optional_field13_;
  std::optional<Base&> optional_field14_;
  absl::optional<Traceable&> optional_field15_;
  std::optional<Traceable&> optional_field16_;
  absl::optional<Member<Base>&> optional_field17_;
  std::optional<Member<Base>&> optional_field18_;
  base::raw_ptr<Base> raw_ptr_field_;
  base::raw_ptr<Traceable> raw_ptr_field2_;
  base::raw_ptr<Member<Base>> raw_ptr_field3_;
  base::raw_ptr<Base*> raw_ptr_field4_;
  base::raw_ptr<Traceable*> raw_ptr_field5_;
  base::raw_ptr<Member<Base>*> raw_ptr_field6_;
  base::raw_ptr<Base&> raw_ptr_field7_;
  base::raw_ptr<Traceable&> raw_ptr_field8_;
  base::raw_ptr<Member<Base>&> raw_ptr_field9_;
  base::raw_ref<Base> raw_ref_field_;
  base::raw_ref<Traceable> raw_ref_field2_;
  base::raw_ref<Member<Base>> raw_ref_field3_;
  base::raw_ref<Base*> raw_ref_field4_;
  base::raw_ref<Traceable*> raw_ref_field5_;
  base::raw_ref<Member<Base>*> raw_ref_field6_;
  base::raw_ref<Base&> raw_ref_field7_;
  base::raw_ref<Traceable&> raw_ref_field8_;
  base::raw_ref<Member<Base>&> raw_ref_field9_;
};

}  // namespace blink
