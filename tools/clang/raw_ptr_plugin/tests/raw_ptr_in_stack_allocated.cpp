// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// fields should be checked
struct FooStruct {
  using IsStackAllocatedTypeMarker [[maybe_unused]] = int;

  int* ptr1;  // OK: inside STACK_ALLOCATED();
};

struct BarStruct : FooStruct {
  int* ptr2;  // OK: inside inherited STACK_ALLOCATED();
};
