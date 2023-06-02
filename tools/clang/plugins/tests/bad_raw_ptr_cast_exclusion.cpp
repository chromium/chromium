// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_cast.h"

struct RawPtrWrapper {
  raw_ptr<int> ptr;
};

struct RawPtrWrapperWrapper {
  RawPtrWrapper* ptr;
};

class RawPtrWrapperWrapperWrapper {
 public:
  explicit RawPtrWrapperWrapperWrapper(RawPtrWrapperWrapper& ptr) : ptr_(ptr) {}

  RawPtrWrapperWrapper& ptr_;
};

struct RawPtrWrapperSub : RawPtrWrapper {};
struct RawPtrWrapperVirtualSub : virtual RawPtrWrapper {};

void MyFunc(void* p) {}

// 'unsafe_raw_ptr_*_cast' should not emit errors.
void CastFromCastingUnsafeExclusion() {
  // "Casting unsafe" type variables;
  raw_ptr<int> p0;
  RawPtrWrapper p1;
  RawPtrWrapperWrapper p2;
  RawPtrWrapperWrapperWrapper p3(p2);
  RawPtrWrapper p4[10];
  RawPtrWrapperSub p5;
  RawPtrWrapperVirtualSub p6;

  (void)base::unsafe_raw_ptr_static_cast<void*>(&p0);
  (void)base::unsafe_raw_ptr_static_cast<void*>(&p1);
  (void)base::unsafe_raw_ptr_static_cast<void*>(&p2);
  (void)base::unsafe_raw_ptr_static_cast<void*>(&p3);
  (void)base::unsafe_raw_ptr_static_cast<void*>(&p4);
  (void)base::unsafe_raw_ptr_static_cast<void*>(&p5);
  (void)base::unsafe_raw_ptr_static_cast<void*>(&p6);

  (void)base::unsafe_raw_ptr_reinterpret_cast<void*>(&p0);
  (void)base::unsafe_raw_ptr_reinterpret_cast<void*>(&p1);
  (void)base::unsafe_raw_ptr_reinterpret_cast<void*>(&p2);
  (void)base::unsafe_raw_ptr_reinterpret_cast<void*>(&p3);
  (void)base::unsafe_raw_ptr_reinterpret_cast<void*>(&p4);
  (void)base::unsafe_raw_ptr_reinterpret_cast<void*>(&p5);
  (void)base::unsafe_raw_ptr_reinterpret_cast<void*>(&p6);

  (void)base::unsafe_raw_ptr_bit_cast<void*>(&p0);
  (void)base::unsafe_raw_ptr_bit_cast<void*>(&p1);
  (void)base::unsafe_raw_ptr_bit_cast<void*>(&p2);
  (void)base::unsafe_raw_ptr_bit_cast<void*>(&p3);
  (void)base::unsafe_raw_ptr_bit_cast<void*>(&p4);
  (void)base::unsafe_raw_ptr_bit_cast<void*>(&p5);
  (void)base::unsafe_raw_ptr_bit_cast<void*>(&p6);
}

// 'unsafe_raw_ptr_*_cast' should not emit errors.
void CastToCastingUnsafeExclusion() {
  void* p = nullptr;

  // CK_BitCast via |base::unsafe_raw_ptr_static_cast| should emit an error.
  (void)base::unsafe_raw_ptr_static_cast<raw_ptr<int>*>(p);
  (void)base::unsafe_raw_ptr_static_cast<RawPtrWrapper*>(p);
  (void)base::unsafe_raw_ptr_static_cast<RawPtrWrapperWrapper*>(p);
  (void)base::unsafe_raw_ptr_static_cast<RawPtrWrapperWrapperWrapper*>(p);
  (void)base::unsafe_raw_ptr_static_cast<RawPtrWrapperSub*>(p);
  (void)base::unsafe_raw_ptr_static_cast<RawPtrWrapperVirtualSub*>(p);

  // CK_BitCast via |base::unsafe_raw_ptr_reinterpret_cast| should emit an
  // error.
  (void)base::unsafe_raw_ptr_reinterpret_cast<raw_ptr<int>*>(p);
  (void)base::unsafe_raw_ptr_reinterpret_cast<RawPtrWrapper*>(p);
  (void)base::unsafe_raw_ptr_reinterpret_cast<RawPtrWrapperWrapper*>(p);
  (void)base::unsafe_raw_ptr_reinterpret_cast<RawPtrWrapperWrapperWrapper*>(p);
  (void)base::unsafe_raw_ptr_reinterpret_cast<RawPtrWrapperSub*>(p);
  (void)base::unsafe_raw_ptr_reinterpret_cast<RawPtrWrapperVirtualSub*>(p);

  // CK_BitCast via |bit_cast| should emit an error.
  (void)base::unsafe_raw_ptr_bit_cast<raw_ptr<int>*>(p);
  (void)base::unsafe_raw_ptr_bit_cast<RawPtrWrapper*>(p);
  (void)base::unsafe_raw_ptr_bit_cast<RawPtrWrapperWrapper*>(p);
  (void)base::unsafe_raw_ptr_bit_cast<RawPtrWrapperWrapperWrapper*>(p);
  (void)base::unsafe_raw_ptr_bit_cast<RawPtrWrapperSub*>(p);
  (void)base::unsafe_raw_ptr_bit_cast<RawPtrWrapperVirtualSub*>(p);
}
