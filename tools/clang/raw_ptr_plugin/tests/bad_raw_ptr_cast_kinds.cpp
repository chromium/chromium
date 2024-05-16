// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/memory/raw_ptr.h"

struct RawPtrWrapper {
  raw_ptr<int> ptr;
};
struct RawPtrWrapperSub : RawPtrWrapper {};

void VariousCasting() {
  raw_ptr<int> ptr;
  RawPtrWrapper wrapped;
  RawPtrWrapper arr[10];
  void* void_ptr = nullptr;

  // CK_BitCast should emit an error.
  (void)reinterpret_cast<RawPtrWrapper*>(void_ptr);

  // CK_LValueBitCast should emit an error.
  RawPtrWrapper& ref = wrapped;
  ref = reinterpret_cast<RawPtrWrapper&>(void_ptr);

  // CK_LValueToRValueBitCast should emit an error.
  (void)__builtin_bit_cast(int*, ptr);
  (void)__builtin_bit_cast(raw_ptr<int>, wrapped);
  (void)__builtin_bit_cast(int*, wrapped);

  // CK_PointerToIntegral should emit an error.
  uintptr_t i = reinterpret_cast<uintptr_t>(&wrapped);

  // CK_IntegralToPointer should emit an error.
  wrapped = *reinterpret_cast<RawPtrWrapper*>(i);

  // CK_ArrayToPointerDecay should be safe.
  (void)static_cast<RawPtrWrapper*>(arr);

  // This line has two casts, CK_ArrayToPointerDecay and CK_BitCast.
  // q = (void*) (RawPtrWrapper*) arr;
  // The latter should emit an error.
  void_ptr = arr;

  // CK_BaseToDerived should be safe.
  RawPtrWrapperSub* sub = static_cast<RawPtrWrapperSub*>(&wrapped);

  // CK_DerivedToBase should be safe.
  wrapped = *static_cast<RawPtrWrapper*>(sub);

  // CK_ToVoid should be safe.
  (void)&wrapped;

  // Illegal casts: should be disallowed by the compiler.
  (void)static_cast<int*>(ptr);
  (void)static_cast<raw_ptr<int>>(wrapped);
  (void)static_cast<int*>(wrapped);

  (void)reinterpret_cast<int*>(ptr);
  (void)reinterpret_cast<raw_ptr<int>>(wrapped);
  (void)reinterpret_cast<int*>(wrapped);
}
