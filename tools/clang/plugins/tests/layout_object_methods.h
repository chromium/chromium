// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_LAYOUT_OBJECT_METHODS_H_
#define TOOLS_CLANG_PLUGINS_TESTS_LAYOUT_OBJECT_METHODS_H_

namespace blink {

class Visitor;

void foo() {}

class LayoutObject {
 public:
  // These methods should be ignored.
  static void StaticMethod() {}
  void CheckIsNotDestroyed() {}
  void Trace(Visitor*) const {}

  int ShouldPass1() {
    CheckIsNotDestroyed();
    foo();
    return 0;
  }
  int ShouldFail1() {
    // CheckIsNotDestroyed();
    foo();
    return 0;
  }
};

class LayoutBoxModelObject : public LayoutObject {
 public:
  int ShouldPass2() {
    CheckIsNotDestroyed();
    return 0;
  }
  int ShouldFail2() {
    ShouldPass2();
    CheckIsNotDestroyed();  // This should be the first statement.
    return 0;
  }
};

class LayoutBox : public LayoutBoxModelObject {
 public:
  int ShouldPass3() {
    CheckIsNotDestroyed();
    return 0;
  }
  int ShouldFail3() {
    foo();
    CheckIsNotDestroyed();  // This should be the first statement.
    return 0;
  }
};

}  // namespace blink

#endif  // TOOLS_CLANG_PLUGINS_TESTS_LAYOUT_OBJECT_METHODS_H_
