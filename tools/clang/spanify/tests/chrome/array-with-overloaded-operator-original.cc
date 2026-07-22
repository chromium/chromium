// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

struct MyClass {};
enum MyEnum { VALUE };

// Overload operator+ for MyClass(&)[10] and MyEnum
MyClass* operator+(MyClass (&arr)[10], MyEnum val);

int UnsafeIndex();

void test_regression() {
  MyClass arr[10];
  // Trigger spanification of arr
  arr[UnsafeIndex()] = MyClass();

  MyEnum val = VALUE;
  arr + val;
}
