// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <text-input-unstable-v1-server-protocol.h>
#include <wayland-server.h>

#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/wayland_input_method_context.h"
#include "ui/ozone/platform/wayland/wayland_input_method_context_factory.h"
#include "ui/ozone/platform/wayland/wayland_test.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

using ::testing::SaveArg;
using ::testing::_;

namespace ui {

class TestInputMethodContextDelegate : public LinuxInputMethodContextDelegate {
 public:
  TestInputMethodContextDelegate() {}
  ~TestInputMethodContextDelegate() override {}

  void OnCommit(const base::string16& text) override {
    was_on_commit_called_ = true;
  }
  void OnPreeditChanged(const ui::CompositionText& composition_text) override {
    was_on_preedit_changed_called_ = true;
  }
  void OnPreeditEnd() override {}
  void OnPreeditStart() override {}
  void OnDeleteSurroundingText(int32_t index, uint32_t length) override{};

  bool was_on_commit_called() { return was_on_commit_called_; }

  bool was_on_preedit_changed_called() {
    return was_on_preedit_changed_called_;
  }

 private:
  bool was_on_commit_called_ = false;
  bool was_on_preedit_changed_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodContextDelegate);
};

class WaylandInputMethodContextTest : public WaylandTest {
 public:
  WaylandInputMethodContextTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    Sync();

    input_method_context_delegate_ =
        std::make_unique<TestInputMethodContextDelegate>();

    WaylandInputMethodContextFactory factory(connection_.get());
    input_method_context_ = factory.CreateWaylandInputMethodContext(
        input_method_context_delegate_.get(), false);
    input_method_context_->Init(true);
    connection_->ScheduleFlush();

    Sync();

    zwp_text_input_ = server_.text_input_manager_v1()->text_input.get();
    window_->set_keyboard_focus(true);

    ASSERT_TRUE(connection_->text_input_manager_v1());
    ASSERT_TRUE(zwp_text_input_);
  }

 protected:
  std::unique_ptr<TestInputMethodContextDelegate>
      input_method_context_delegate_;
  std::unique_ptr<WaylandInputMethodContext> input_method_context_;
  wl::MockZwpTextInput* zwp_text_input_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandInputMethodContextTest);
};

TEST_P(WaylandInputMethodContextTest, Focus) {
  EXPECT_CALL(*zwp_text_input_, Activate(surface_->resource()));
  EXPECT_CALL(*zwp_text_input_, ShowInputPanel());
  input_method_context_->Focus();
  connection_->ScheduleFlush();
  Sync();
}

TEST_P(WaylandInputMethodContextTest, Blur) {
  EXPECT_CALL(*zwp_text_input_, Deactivate());
  EXPECT_CALL(*zwp_text_input_, HideInputPanel());
  input_method_context_->Blur();
  connection_->ScheduleFlush();
  Sync();
}

TEST_P(WaylandInputMethodContextTest, Reset) {
  EXPECT_CALL(*zwp_text_input_, Reset());
  input_method_context_->Reset();
  connection_->ScheduleFlush();
  Sync();
}

TEST_P(WaylandInputMethodContextTest, SetCursorLocation) {
  EXPECT_CALL(*zwp_text_input_, SetCursorRect(50, 0, 1, 1));
  input_method_context_->SetCursorLocation(gfx::Rect(50, 0, 1, 1));
  connection_->ScheduleFlush();
  Sync();
}

TEST_P(WaylandInputMethodContextTest, OnPreeditChanged) {
  zwp_text_input_v1_send_preedit_string(zwp_text_input_->resource(), 0,
                                        "PreeditString", "");
  Sync();
  EXPECT_TRUE(input_method_context_delegate_->was_on_preedit_changed_called());
}

TEST_P(WaylandInputMethodContextTest, OnCommit) {
  zwp_text_input_v1_send_commit_string(zwp_text_input_->resource(), 0,
                                       "CommitString");
  Sync();
  EXPECT_TRUE(input_method_context_delegate_->was_on_commit_called());
}

INSTANTIATE_TEST_CASE_P(XdgVersionV5Test,
                        WaylandInputMethodContextTest,
                        ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_CASE_P(XdgVersionV6Test,
                        WaylandInputMethodContextTest,
                        ::testing::Values(kXdgShellV6));

}  // namespace ui
