// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_delegate.h"

#include "base/test/bind.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

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

TEST(WebDialogDelegateTest, CallbackOnClose) {
  bool called = false;
  auto delegate = std::make_unique<WebDialogDelegate>();

  delegate->set_delete_on_close(false);
  delegate->RegisterOnDialogClosedCallback(
      base::BindLambdaForTesting([&](const std::string&) { called = true; }));

  EXPECT_FALSE(called);
  delegate->OnDialogClosed("{}");
  EXPECT_TRUE(called);

  called = false;
  delegate.reset();
  EXPECT_FALSE(called);
}

class DeletionTestWebDialogDelegate : public WebDialogDelegate {
 public:
  explicit DeletionTestWebDialogDelegate(bool* deleted) : deleted_(deleted) {}
  ~DeletionTestWebDialogDelegate() override { *deleted_ = true; }

 private:
  raw_ptr<bool> deleted_;
};

TEST(WebDialogDelegateTest, DeleteOnClose) {
  bool deleted = false;
  auto delegate = std::make_unique<DeletionTestWebDialogDelegate>(&deleted);

  delegate->set_delete_on_close(true);
  delegate.release()->OnDialogClosed("{}");

  EXPECT_TRUE(deleted);
}

TEST(WebDialogDelegateTest, NoDeleteOnClose) {
  bool deleted = false;
  auto delegate = std::make_unique<DeletionTestWebDialogDelegate>(&deleted);

  delegate->set_delete_on_close(false);
  delegate->OnDialogClosed("{}");

  EXPECT_FALSE(deleted);

  delegate.reset();

  EXPECT_TRUE(deleted);
}

TEST(WebDialogDelegateTest, AcceleratorsAreHandled) {
  bool called = false;
  auto delegate = std::make_unique<WebDialogDelegate>();

  const ui::Accelerator accelerator{ui::VKEY_Z, ui::EF_CONTROL_DOWN};
  const ui::Accelerator other_accelerator{ui::VKEY_Z, ui::EF_SHIFT_DOWN};

  delegate->RegisterAccelerator(
      accelerator, base::BindLambdaForTesting(
                       [&](WebDialogDelegate& provided_delegate,
                           const ui::Accelerator& provided_accelerator) {
                         EXPECT_EQ(&provided_delegate, delegate.get());
                         EXPECT_EQ(provided_accelerator, accelerator);
                         called = true;
                         return true;
                       }));

  EXPECT_FALSE(delegate->AcceleratorPressed(other_accelerator));
  EXPECT_FALSE(called);
  EXPECT_TRUE(delegate->AcceleratorPressed(accelerator));
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace ui
