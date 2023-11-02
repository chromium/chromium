// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test_api.h"

#include "ui/views/widget/widget.h"

namespace views {

void DisableActivationChangeHandlingForTests() {
  Widget::SetDisableActivationChangeHandling(
      Widget::DisableActivationChangeHandlingType::kIgnore);
}

}  // namespace views
