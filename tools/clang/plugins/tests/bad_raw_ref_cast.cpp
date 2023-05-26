// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

struct RawRefWrapper {
  raw_ref<int> ref;
};

struct RawRefWrapperWrapper {
  RawRefWrapper* ref;
};

class RawRefWrapperWrapperWrapper {
 public:
  explicit RawRefWrapperWrapperWrapper(RawRefWrapperWrapper& ref) : ref_(ref) {}

  RawRefWrapperWrapper& ref_;
};

struct RawRefWrapperSub : RawRefWrapper {};
struct RawRefWrapperVirtualSub : virtual RawRefWrapper {};

void MyFunc(void* p) {}

void CastFromCastingUnsafe() {
  // "Casting unsafe" type variables;
  raw_ref<int> p0;
  RawRefWrapper p1;
  RawRefWrapperWrapper p2;
  RawRefWrapperWrapperWrapper p3(p2);
  RawRefWrapper p4[10];
  RawRefWrapperSub p5;
  RawRefWrapperVirtualSub p6;

  // CK_BitCast via |static_cast| should emit an error.
  (void)static_cast<void*>(&p0);
  (void)static_cast<void*>(&p1);
  (void)static_cast<void*>(&p2);
  (void)static_cast<void*>(&p3);
  (void)static_cast<void*>(&p4);
  (void)static_cast<void*>(&p5);
  (void)static_cast<void*>(&p6);

  // CK_BitCast via C-style casting should emit an error.
  (void)(void*)&p0;
  (void)(void*)&p1;
  (void)(void*)&p2;
  (void)(void*)&p3;
  (void)(void*)&p4;
  (void)(void*)&p5;
  (void)(void*)&p6;

  // CK_BitCast via |reinterpret_cast| should emit an error.
  (void)reinterpret_cast<void*>(&p0);
  (void)reinterpret_cast<void*>(&p1);
  (void)reinterpret_cast<void*>(&p2);
  (void)reinterpret_cast<void*>(&p3);
  (void)reinterpret_cast<void*>(&p4);
  (void)reinterpret_cast<void*>(&p5);
  (void)reinterpret_cast<void*>(&p6);

  // CK_LValueToRValueBitCast via |bit_cast| should emit an error.
  (void)__builtin_bit_cast(void*, &p0);
  (void)__builtin_bit_cast(void*, &p1);
  (void)__builtin_bit_cast(void*, &p2);
  (void)__builtin_bit_cast(void*, &p3);
  (void)__builtin_bit_cast(void*, &p4);
  (void)__builtin_bit_cast(void*, &p5);
  (void)__builtin_bit_cast(void*, &p6);

  // CK_BitCast via implicit casting should emit an error.
  MyFunc(&p0);
  MyFunc(&p1);
  MyFunc(&p2);
  MyFunc(&p3);
  MyFunc(&p4);
  MyFunc(&p5);
  MyFunc(&p6);
}

void CastToCastingUnsafe() {
  void* p = nullptr;

  // CK_BitCast via |static_cast| should emit an error.
  (void)static_cast<raw_ref<int>*>(p);
  (void)static_cast<RawRefWrapper*>(p);
  (void)static_cast<RawRefWrapperWrapper*>(p);
  (void)static_cast<RawRefWrapperWrapperWrapper*>(p);
  (void)static_cast<RawRefWrapperSub*>(p);
  (void)static_cast<RawRefWrapperVirtualSub*>(p);

  // CK_BitCast via C-style casting should emit an error.
  (void)(raw_ptr<int>*)p;
  (void)(RawRefWrapper*)p;
  (void)(RawRefWrapperWrapper*)p;
  (void)(RawRefWrapperWrapperWrapper*)p;
  (void)(RawRefWrapperSub*)p;
  (void)(RawRefWrapperVirtualSub*)p;

  // CK_BitCast via |reinterpret_cast| should emit an error.
  (void)reinterpret_cast<raw_ref<int>*>(p);
  (void)reinterpret_cast<RawRefWrapper*>(p);
  (void)reinterpret_cast<RawRefWrapperWrapper*>(p);
  (void)reinterpret_cast<RawRefWrapperWrapperWrapper*>(p);
  (void)reinterpret_cast<RawRefWrapperSub*>(p);
  (void)reinterpret_cast<RawRefWrapperVirtualSub*>(p);

  // CK_LValueToRValueBitCast via |bit_cast| should emit an error.
  (void)__builtin_bit_cast(raw_ref<int>*, p);
  (void)__builtin_bit_cast(RawRefWrapper*, p);
  (void)__builtin_bit_cast(RawRefWrapperWrapper*, p);
  (void)__builtin_bit_cast(RawRefWrapperWrapperWrapper*, p);
  (void)__builtin_bit_cast(RawRefWrapperSub*, p);
  (void)__builtin_bit_cast(RawRefWrapperVirtualSub*, p);
}
