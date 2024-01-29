// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "ui/views/view.h"

namespace views {

struct SyncLayout : public View {
  void DoSomething() {
    LayoutImmediately();  // expected-error {{'LayoutImmediately' is a private member}}
  }
};

// `LayoutSuperclass<SuperT>(this)` should be the only way to trigger superclass
// layout.
struct SuperclassLayout : public View {
  void Layout() override {
    LayoutSuperclass<SuperclassLayout>(this);  // expected-error {{no matching member function}}
    LayoutSuperclass<SyncLayout>(this);        // expected-error {{no matching member function}}
  }
};

}  // namespace views
