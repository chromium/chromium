// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

template <typename T>
struct MyArbitraryContainer {};

template <typename T>
struct ExcludedContainer {};

struct MyStruct {
  // Error expected: included_types is empty, so all containers are checked.
  MyArbitraryContainer<int*> arbitrary_member;

  // No error expected: ExcludedContainer is in excluded_types.
  ExcludedContainer<int*> excluded_member;
};
