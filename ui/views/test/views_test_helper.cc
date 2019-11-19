// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper.h"

namespace views {

ViewsTestHelper::ViewsTestHelper() = default;

ViewsTestHelper::~ViewsTestHelper() = default;

void ViewsTestHelper::SetUp() {
}

void ViewsTestHelper::TearDown() {
}

gfx::NativeWindow ViewsTestHelper::GetContext() {
  return nullptr;
}

}  // namespace views
