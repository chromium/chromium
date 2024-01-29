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

}  // namespace views
