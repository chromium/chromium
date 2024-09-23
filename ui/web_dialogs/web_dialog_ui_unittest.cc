// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_ui.h"

#include "content/public/common/bindings_policy.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(WebDialogUITest, NoBindingsSetForWebDialogUI) {
  content::TestWebUI test_web_ui;
  EXPECT_EQ(content::BindingsPolicySet(), test_web_ui.GetBindings());

  WebDialogUI web_dialog_ui(&test_web_ui);
  EXPECT_EQ(content::BindingsPolicySet(), test_web_ui.GetBindings());
}

TEST(MojoWebDialogUITest, ChromeSendAndMojoBindingsForMojoWebDialogUI) {
  content::TestWebUI test_web_ui;
  EXPECT_EQ(content::BindingsPolicySet(), test_web_ui.GetBindings());

  MojoWebDialogUI web_dialog_ui(&test_web_ui);

  // MojoWebDialogUIs rely on both Mojo and chrome.send().
  EXPECT_EQ(content::kWebUIBindingsPolicySet, test_web_ui.GetBindings());
}

}  // namespace ui
