// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "variant_of_gced_type.h"

namespace blink {

class WithVariant : public GarbageCollected<WithVariant> {
 public:
  virtual void Trace(Visitor*) const {}

 private:
  absl::variant<Base> variant_field_;
  std::variant<Base> variant_field2_;
  absl::variant<Traceable> variant_field3_;
  std::variant<Traceable> variant_field4_;
  absl::variant<Member<Base>> variant_field5_;
  std::variant<Member<Base>> variant_field6_;
  absl::variant<Base*> variant_field7_;
  std::variant<Base*> variant_field8_;
  absl::variant<Traceable*> variant_field9_;
  std::variant<Traceable*> variant_field10_;
  absl::variant<Member<Base>*> variant_field11_;
  std::variant<Member<Base>*> variant_field12_;
  absl::variant<Base&> variant_field13_;
  std::variant<Base&> variant_field14_;
  absl::variant<Traceable&> variant_field15_;
  std::variant<Traceable&> variant_field16_;
  absl::variant<Member<Base>&> variant_field17_;
  std::variant<Member<Base>&> variant_field18_;
};

void ForbidsVariantsOfGcedTypes() {
  {
    absl::variant<Base> not_ok;
    (void)not_ok;

    absl::variant<Base, Base> similarly_not_ok;
    (void)similarly_not_ok;

    absl::variant<int, Base> not_ok_either;
    (void)not_ok_either;

    absl::variant<int, Derived> ditto;
    (void)ditto;

    absl::variant<int, Traceable> ok_traceable;
    (void)ok_traceable;

    absl::variant<int, Member<Base>> ok_member;
    (void)ok_member;

    absl::variant<int, Base*> ok_base_ptr;
    (void)ok_base_ptr;

    absl::variant<int, Traceable*> ok_traceable_ptr;
    (void)ok_traceable_ptr;

    absl::variant<int, Member<Base>*> ok_member_ptr;
    (void)ok_member_ptr;

    absl::variant<int, Base&> ok_base_ref;
    (void)ok_base_ref;

    absl::variant<int, Traceable&> ok_traceable_ref;
    (void)ok_traceable_ref;

    absl::variant<int, Member<Base>&> ok_member_ref;
    (void)ok_member_ref;

    new absl::variant<Mixin>;
    new absl::variant<Traceable>;
    new absl::variant<Member<Base>>;
    new absl::variant<Base*>;
    new absl::variant<Traceable*>;
    new absl::variant<Member<Base>*>;
    new absl::variant<Base&>;
    new absl::variant<Traceable&>;
    new absl::variant<Member<Base>&>;
  }

  {
    std::variant<Base> not_ok;
    (void)not_ok;

    std::variant<Base, Base> similarly_not_ok;
    (void)similarly_not_ok;

    std::variant<int, Base> not_ok_either;
    (void)not_ok_either;

    std::variant<int, Derived> ditto;
    (void)ditto;

    std::variant<int, Traceable> ok_traceable;
    (void)ok_traceable;

    std::variant<int, Member<Base>> ok_member;
    (void)ok_member;

    std::variant<int, Base*> ok_base_ptr;
    (void)ok_base_ptr;

    std::variant<int, Traceable*> ok_traceable_ptr;
    (void)ok_traceable_ptr;

    std::variant<int, Member<Base>*> ok_member_ptr;
    (void)ok_member_ptr;

    std::variant<int, Base&> ok_base_ref;
    (void)ok_base_ref;

    std::variant<int, Traceable&> ok_traceable_ref;
    (void)ok_traceable_ref;

    std::variant<int, Member<Base>&> ok_member_ref;
    (void)ok_member_ref;

    new std::variant<Mixin>;
    new std::variant<Traceable>;
    new std::variant<Member<Base>>;
    new std::variant<Base*>;
    new std::variant<Traceable*>;
    new std::variant<Member<Base>*>;
    new std::variant<Base&>;
    new std::variant<Traceable&>;
    new std::variant<Member<Base>&>;
  }
}

class OnStack {
  STACK_ALLOCATED();

 public:
  OnStack() {
    (void)variant_field_;
    (void)variant_field2_;
    (void)variant_field3_;
    (void)variant_field4_;
    (void)variant_field5_;
    (void)variant_field6_;
    (void)variant_field7_;
    (void)variant_field8_;
    (void)variant_field9_;
    (void)variant_field10_;
    (void)variant_field11_;
    (void)variant_field12_;
    (void)variant_field13_;
    (void)variant_field14_;
    (void)variant_field15_;
    (void)variant_field16_;
    (void)variant_field17_;
    (void)variant_field18_;
  }

 private:
  // Variant fields (except GCed by value) are ok since the class is stack
  // allocated.
  absl::variant<Base> variant_field_;
  std::variant<Base> variant_field2_;
  absl::variant<Traceable> variant_field3_;
  std::variant<Traceable> variant_field4_;
  absl::variant<Member<Base>> variant_field5_;
  std::variant<Member<Base>> variant_field6_;
  absl::variant<Base*> variant_field7_;
  std::variant<Base*> variant_field8_;
  absl::variant<Traceable*> variant_field9_;
  std::variant<Traceable*> variant_field10_;
  absl::variant<Member<Base>*> variant_field11_;
  std::variant<Member<Base>*> variant_field12_;
  absl::variant<Base&> variant_field13_;
  std::variant<Base&> variant_field14_;
  absl::variant<Traceable&> variant_field15_;
  std::variant<Traceable&> variant_field16_;
  absl::variant<Member<Base>&> variant_field17_;
  std::variant<Member<Base>&> variant_field18_;
};

}  // namespace blink
