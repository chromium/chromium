// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "ui/views/view.h"

#include <memory>

namespace views {

// `DeprecatedLayoutImmediately()` should be the only way to trigger layout
// synchronously.
struct SyncLayout : public View {
  SyncLayout() : child_(AddChildView(std::make_unique<View>())) {
    child_->Layout();  // expected-error {{too few arguments to function call}}
  }

  void DoSomething() {
    Layout({});           // expected-error {{calling a private constructor}}
    LayoutImmediately();  // expected-error {{'LayoutImmediately' is a private member}}
  }

 private:
  View* child_;
};

// `LayoutSuperclass<SuperT>(this)` should be the only way to trigger superclass
// layout.
struct SuperclassLayout : public View {
  void Layout(PassKey key) override {
    View::Layout(key);                         // expected-error {{call to deleted constructor}}
    LayoutSuperclass<SuperclassLayout>(this);  // expected-error {{no matching member function}}
    LayoutSuperclass<SyncLayout>(this);        // expected-error {{no matching member function}}
  }
};

}  // namespace views
