// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#define DECLARE_OUT_OF_LINE_FUNC(name) void name(int* ptr)

class MacroDeclTest {
 public:
  // No expected rewrite.
  DECLARE_OUT_OF_LINE_FUNC(TestFunc);
};

// No expected rewrite.
void MacroDeclTest::TestFunc(int* ptr) {
  // Unsafe access triggers spanification analysis, but out-of-line
  // definition rewrite is cancelled because header decl is in a macro.
  ptr[0] = 42;
}

#define INT_PTR int*

class MacroTypeDeclTest {
 public:
  // No expected rewrite.
  void ProcessTypeMacro(INT_PTR ptr);
};

void MacroTypeDeclTest::ProcessTypeMacro(int* ptr) {
  // Unsafe access triggers spanification analysis, but out-of-line definition
  // rewrite is canceleed because the parameter type comes from a macro.
  ptr[0] = 42;
}
