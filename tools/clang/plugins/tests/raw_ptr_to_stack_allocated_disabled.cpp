// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/raw_ptr.h"

struct StackAllocatedType {
  using IsStackAllocatedTypeMarker [[maybe_unused]] = int;
};
struct StackAllocatedSubType : public StackAllocatedType {};
struct NonStackAllocatedType {};

// typedefs should be checked
typedef raw_ptr<StackAllocatedType> ErrTypeA;
typedef raw_ptr<StackAllocatedSubType> ErrTypeB;
typedef raw_ptr<NonStackAllocatedType> OkTypeC;
typedef raw_ptr<std::vector<StackAllocatedType>> ErrTypeD;
typedef raw_ptr<std::vector<StackAllocatedSubType>> ErrTypeE;
typedef raw_ptr<std::vector<NonStackAllocatedType>> OkTypeF;
typedef std::vector<raw_ptr<StackAllocatedType>> ErrTypeG;
typedef std::vector<raw_ptr<StackAllocatedSubType>> ErrTypeH;
typedef std::vector<raw_ptr<NonStackAllocatedType>> OkTypeI;

// fields should be checked
struct MyStruct {
  raw_ptr<StackAllocatedType> err_a;
  raw_ptr<StackAllocatedSubType> err_b;
  raw_ptr<NonStackAllocatedType> ok_c;
  raw_ptr<std::vector<StackAllocatedType>> err_d;
  raw_ptr<std::vector<StackAllocatedSubType>> err_e;
  raw_ptr<std::vector<NonStackAllocatedType>> ok_f;
  std::vector<raw_ptr<StackAllocatedType>> err_g;
  std::vector<raw_ptr<StackAllocatedSubType>> err_h;
  std::vector<raw_ptr<NonStackAllocatedType>> ok_i;
};

// variables should be checked
void MyFunc() {
  raw_ptr<StackAllocatedType> err_a;
  raw_ptr<StackAllocatedSubType> err_b;
  raw_ptr<NonStackAllocatedType> ok_c;
  raw_ptr<std::vector<StackAllocatedType>> err_d;
  raw_ptr<std::vector<StackAllocatedSubType>> err_e;
  raw_ptr<std::vector<NonStackAllocatedType>> ok_f;
  std::vector<raw_ptr<StackAllocatedType>> err_g;
  std::vector<raw_ptr<StackAllocatedSubType>> err_h;
  std::vector<raw_ptr<NonStackAllocatedType>> ok_i;

  raw_ref<StackAllocatedType> err_raw_ref;
  raw_ref<NonStackAllocatedType> ok_raw_ref;
}
