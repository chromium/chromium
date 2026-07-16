// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern int UnsafeIndex();

// Case 1: Function redeclaration in a macro.
// Validates `redecl->getBeginLoc().isMacroID()` in Spanifier.cpp.
#define DECLARE_FUNC(name) void name(int* ptr, int size);

// Use the macro to declare foo:
DECLARE_FUNC(foo)

// Out-of-line definition of foo:
// No expected rewrite: foo is declared via macro expansion.
void foo(int* ptr, int size) {
  // Unsafe buffer access using a dynamic index:
  ptr[UnsafeIndex()] = 0;
}

// Case 2: Overridden method in a macro.
// Validates `overridden_method_decl->getBeginLoc().isMacroID()` in
// Spanifier.cpp.
class BaseClassWithMacro {
 public:
#define DECLARE_OVERRIDDEN_METHOD(type) virtual void MacroMethod(type* ptr);
  DECLARE_OVERRIDDEN_METHOD(int)
};

class DerivedClassWithMacro : public BaseClassWithMacro {
 public:
  // No expected rewrite: MacroMethod overrides a base method declared via
  // macro expansion.
  void MacroMethod(int* ptr) override { ptr[UnsafeIndex()] = 0; }
};

void test_macro_blocked_functions() {
  int buf1[5];
  // No expected rewrite: foo's parameter is not rewritten due to macro
  // declaration.
  foo(buf1, 5);

  int buf2[5];
  DerivedClassWithMacro d;
  // No expected rewrite: MacroMethod's parameter is not rewritten due to
  // macro declaration in base class.
  d.MacroMethod(buf2);
}
