// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_TEST_DIALOG_MODEL_HOST_H_
#define UI_BASE_TEST_TEST_DIALOG_MODEL_HOST_H_

#include "ui/base/models/dialog_model_host.h"

namespace ui {

class TestDialogModelHost : public DialogModelHost {
 public:
  // TODO(pbos): Flesh out more of DialogModelHost public test APIs to enable
  // hosting a DialogModel, then try to make this private again. Right now
  // unittests inline OnDialogCancelled(ui::TestDialogModelHost::GetPassKey())
  // instead of having TestDialogModelHost manage user flows by methods such as
  // TestDialogModelHost::Cancel() and the destructor of TestDialogModelHost
  // call OnDialogDestroying().
  using DialogModelHost::GetPassKey;
};

}  // namespace ui

#endif  // UI_BASE_TEST_TEST_DIALOG_MODEL_HOST_H_