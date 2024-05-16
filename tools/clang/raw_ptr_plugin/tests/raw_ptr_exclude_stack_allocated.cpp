// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

struct StackAllocatedType {
  using IsStackAllocatedTypeMarker [[maybe_unused]] = int;
};
struct StackAllocatedSubType : public StackAllocatedType {};
struct NonStackAllocatedType {};

struct MyStruct {
  StackAllocatedType* excluded_a;
  StackAllocatedSubType* excluded_b;
  std::vector<StackAllocatedType>* excluded_c;
  std::vector<StackAllocatedSubType>* excluded_d;

  StackAllocatedType& excluded_ref_a;
  StackAllocatedSubType& excluded_ref_b;
  std::vector<StackAllocatedType>& excluded_ref_c;
  std::vector<StackAllocatedSubType>& excluded_ref_d;

  NonStackAllocatedType* err_a;
  std::vector<NonStackAllocatedType>* err_b;
  NonStackAllocatedType& err_ref_a;
  std::vector<NonStackAllocatedType>& err_ref_b;
};
