// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

namespace {

// Naive renaming will break the build, by leaving return type the same name as
// the function name - to avoid this "Get" prefix needs to be prepended as
// suggested in https://crbug.com/582312#c17.
class Foo582312 {};
using Bar = Foo582312;
static Bar* GetBar() {
  return nullptr;
}

}  // namespace

// Tests that the prototype for a function is updated.
int TestFunctionThatTakesTwoInts(int x, int y);
// Overload to test using declarations that introduce multiple shadow
// declarations.
int TestFunctionThatTakesTwoInts(int x, int y, int z);

// Test that the actual function definition is also updated.
int TestFunctionThatTakesTwoInts(int x, int y) {
  if (x == 0)
    return y;
  // Calls to the function also need to be updated.
  return TestFunctionThatTakesTwoInts(x - 1, y + 1);
}

// This is named like the begin() method which isn't renamed, but
// here it's not a method so it should be.
void Begin() {}
// Same for trace() and friends.
void Trace() {}
void Lock() {}

class SwapType {};

// swap() functions are not renamed.
void swap(SwapType& a, SwapType& b) {}

// Note: F is already Google style and should not change.
void F() {
  // Test referencing a function without calling it.
  int (*function_pointer)(int, int) = &TestFunctionThatTakesTwoInts;
}

void Bug640688(int);  // Declaration within blink namespace.

// Tests for --method-blocklist cmdline parameter.
namespace IdlFunctions {
void foo();
}  // namespace IdlFunctions

}  // namespace blink

// Definition outside of blink namespace.
void blink::Bug640688(int my_param) {
  char my_variable = 'c';
}

using blink::TestFunctionThatTakesTwoInts;

void G() {
  TestFunctionThatTakesTwoInts(1, 2);

  blink::SwapType a, b;
  swap(a, b);
}
