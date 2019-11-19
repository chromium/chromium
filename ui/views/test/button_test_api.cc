// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/button_test_api.h"

#include "ui/views/controls/button/button.h"

namespace views {
namespace test {

void ButtonTestApi::NotifyClick(const ui::Event& event) {
  button_->NotifyClick(event);
}

}  // namespace test
}  // namespace views
