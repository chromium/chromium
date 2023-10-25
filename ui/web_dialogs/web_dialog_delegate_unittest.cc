// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_delegate.h"

#include "content/public/browser/web_ui_message_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

using WebDialogDelegateTest = testing::Test;

class TestMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit TestMessageHandler(bool* destroyed) : destroyed_(destroyed) {}
  ~TestMessageHandler() override { *destroyed_ = true; }

 protected:
  void RegisterMessages() override {}
  raw_ptr<bool> destroyed_;
};

// This test validates that:
// 1. AddWebUIMessageHandler() takes ownership of the passed-in handlers, and
// 2. GetWebUIMessageHandlers() passes ownership of the handlers back
TEST(WebDialogDelegateTest, MessageHandlerOwnershipIsPassed) {
  ui::WebDialogDelegate delegate;

  bool destroyed_1 = false;
  bool destroyed_2 = false;

  delegate.AddWebUIMessageHandler(
      std::make_unique<TestMessageHandler>(&destroyed_1));
  delegate.AddWebUIMessageHandler(
      std::make_unique<TestMessageHandler>(&destroyed_2));

  EXPECT_FALSE(destroyed_1);
  EXPECT_FALSE(destroyed_2);

  // Extract the WebUIMessageHandlers as owning raw pointers, and ensure they
  // don't get destroyed in the process.
  std::vector<content::WebUIMessageHandler*> handlers;
  delegate.GetWebUIMessageHandlers(&handlers);

  EXPECT_FALSE(destroyed_1);
  EXPECT_FALSE(destroyed_2);

  // Now, put both handlers back into unique_ptrs and let them go out of scope,
  // destroying them, and validate that destroyed_1 and destroyed_2 are set.
  {
    auto handler_1 = base::WrapUnique(handlers[0]);
    auto handler_2 = base::WrapUnique(handlers[1]);
  }

  EXPECT_TRUE(destroyed_1);
  EXPECT_TRUE(destroyed_2);
}
