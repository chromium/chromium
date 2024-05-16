// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

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

void CastFromCastingUnsafe() {
  // "Casting unsafe" type variables;
  raw_ptr<int> p0;
  RawPtrWrapper p1;
  RawPtrWrapperWrapper p2;
  RawPtrWrapperWrapperWrapper p3(p2);
  RawPtrWrapper p4[10];
  RawPtrWrapperSub p5;
  RawPtrWrapperVirtualSub p6;

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
  (void)static_cast<raw_ptr<int>*>(p);
  (void)static_cast<RawPtrWrapper*>(p);
  (void)static_cast<RawPtrWrapperWrapper*>(p);
  (void)static_cast<RawPtrWrapperWrapperWrapper*>(p);
  (void)static_cast<RawPtrWrapperSub*>(p);
  (void)static_cast<RawPtrWrapperVirtualSub*>(p);

  // CK_BitCast via C-style casting should emit an error.
  (void)(raw_ptr<int>*)p;
  (void)(RawPtrWrapper*)p;
  (void)(RawPtrWrapperWrapper*)p;
  (void)(RawPtrWrapperWrapperWrapper*)p;
  (void)(RawPtrWrapperSub*)p;
  (void)(RawPtrWrapperVirtualSub*)p;

  // CK_BitCast via |reinterpret_cast| should emit an error.
  (void)reinterpret_cast<raw_ptr<int>*>(p);
  (void)reinterpret_cast<RawPtrWrapper*>(p);
  (void)reinterpret_cast<RawPtrWrapperWrapper*>(p);
  (void)reinterpret_cast<RawPtrWrapperWrapperWrapper*>(p);
  (void)reinterpret_cast<RawPtrWrapperSub*>(p);
  (void)reinterpret_cast<RawPtrWrapperVirtualSub*>(p);

  // CK_LValueToRValueBitCast via |bit_cast| should emit an error.
  (void)__builtin_bit_cast(raw_ptr<int>*, p);
  (void)__builtin_bit_cast(RawPtrWrapper*, p);
  (void)__builtin_bit_cast(RawPtrWrapperWrapper*, p);
  (void)__builtin_bit_cast(RawPtrWrapperWrapperWrapper*, p);
  (void)__builtin_bit_cast(RawPtrWrapperSub*, p);
  (void)__builtin_bit_cast(RawPtrWrapperVirtualSub*, p);
}
