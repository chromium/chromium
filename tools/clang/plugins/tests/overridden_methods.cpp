// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "overridden_methods.h"

// Fill in the implementations
void DerivedClass::SomeMethod() {}
void DerivedClass::SomeOtherMethod() {}
void DerivedClass::WebKitModifiedSomething() {}

DerivedClassWithDefaultedDtor::DerivedClassWithDefaultedDtor() {}

class ImplementationInterimClass : public BaseClass {
 public:
  // Should warn about pure virtual methods.
  virtual void SomeMethod() = 0;
};

class ImplementationDerivedClass : public ImplementationInterimClass,
                                   public webkit_glue::WebKitObserverImpl {
 public:
  // Should warn about destructors.
  virtual ~ImplementationDerivedClass() {}
  // Should warn.
  virtual void SomeMethod();
  // Should not warn if marked as override.
  void SomeOtherMethod() override;
  // Should warn for inline implementations in implementation files.
  virtual void SomeInlineMethod() {}
  // Should warn if overriding a method whose origin is blink.
  virtual void WebKitModifiedSomething();
  // Should warn with the insertion point after the const.
  virtual void SomeConstMethod() const {}
  // Should warn with the insertion point after the throw spec.
  virtual void SomeMethodWithExceptionSpec() throw() {}
  // Should warn with the insertion point after both the const and the throw
  // specifiers.
  virtual void SomeConstMethodWithExceptionSpec() const noexcept(false) {}
  // Should warn even if overridden method isn't pure.
  virtual void SomeNonPureBaseMethod() {}
  // Should warn and place correctly even when there is a comment.
  virtual void SomeMethodWithComment();  // This is a comment.
  // Should warn and place correctly even if there is a comment and body.
  virtual void SomeMethodWithCommentAndBody() {}  // This is a comment.
};

int main() {
  DerivedClass something;
  ImplementationDerivedClass something_else;
  DerivedClassWithDefaultedDtor defaulted_dtor;
}
