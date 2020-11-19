// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_TEST_DIALOG_MODEL_HOST_H_
#define UI_BASE_TEST_TEST_DIALOG_MODEL_HOST_H_

#include "ui/base/models/dialog_model_host.h"

namespace ui {

class TestDialogModelHost : public DialogModelHost {
 public:
  using DialogModelHost::GetPassKey;
};

}  // namespace ui

#endif  // UI_BASE_TEST_TEST_DIALOG_MODEL_HOST_H_