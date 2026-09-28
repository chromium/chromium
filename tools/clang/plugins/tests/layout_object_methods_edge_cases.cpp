// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Which methods of LayoutObject subclasses the check looks at: methods that
// are defined in the class body, also in class templates and their
// instantiations and in local classes, but not member function templates,
// methods that are defined out of line, or lambdas.

namespace blink {

class Visitor;
void foo() {}

class LayoutObject {
 public:
  void CheckIsNotDestroyed() const {}

  // Not checked: defined out of line.
  int OutOfLine();

  // Not checked: member function templates.
  template <typename T>
  int MemberTemplate(T t) {
    foo();
    return 0;
  }

  // Checked.
  int InClass() {
    foo();
    // Not checked: a lambda.
    auto lambda = [this]() { foo(); };
    lambda();
    return 0;
  }
};

int LayoutObject::OutOfLine() {
  foo();
  return MemberTemplate(1) + MemberTemplate('a');
}

template <typename T>
class LayoutTemplate : public LayoutObject {
 public:
  // Checked, in the template and in its instantiations.
  int InClassTemplate() {
    foo();
    return 0;
  }

  // Defined out of line: the template isn't checked, its instantiations are.
  int OutOfLineInTemplate();
};

template <typename T>
int LayoutTemplate<T>::OutOfLineInTemplate() {
  foo();
  return 0;
}

int Instantiate() {
  LayoutTemplate<int> layout_int;
  LayoutTemplate<char> layout_char;
  return layout_int.InClassTemplate() + layout_int.OutOfLineInTemplate() +
         layout_char.InClassTemplate();
}

void FunctionWithLocalClass() {
  class LocalLayoutObject : public LayoutObject {
   public:
    // Checked.
    int InLocalClass() {
      foo();
      return 0;
    }
  };
  LocalLayoutObject().InLocalClass();
}

}  // namespace blink
