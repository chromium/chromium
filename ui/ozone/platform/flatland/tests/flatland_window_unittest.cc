// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_window.h"

#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>

#include <memory>
#include <string>

#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"
#include "ui/ozone/platform/flatland/tests/fake_flatland.h"
#include "ui/ozone/platform/flatland/tests/fake_view_ref_focused.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::_;
using ::testing::SaveArg;

namespace ui {

class FlatlandWindowTest : public ::testing::Test {
 protected:
  FlatlandWindowTest()
      : fake_flatland_publisher_(test_context_.additional_services(),
                                 fake_flatland_.GetRequestHandler()),
        fake_view_ref_focused_binding_(&fake_view_ref_focused_) {
    fake_flatland_.SetViewRefFocusedRequestHandler(
        [this](fidl::InterfaceRequest<fuchsia::ui::views::ViewRefFocused>
                   request) {
          CHECK(!fake_view_ref_focused_binding_.is_bound());
          fake_view_ref_focused_binding_.Bind(std::move(request));
        });
  }
  ~FlatlandWindowTest() override = default;

  std::unique_ptr<FlatlandWindow> CreateFlatlandWindow(
      PlatformWindowDelegate* delegate) {
    PlatformWindowInitProperties properties;
    properties.view_ref_pair = scenic::ViewRefPair::New();
    fuchsia::ui::views::ViewportCreationToken parent_token;
    fuchsia::ui::views::ViewCreationToken child_token;
    zx::channel::create(0, &parent_token.value, &child_token.value);
    properties.view_creation_token = std::move(child_token);
    return std::make_unique<FlatlandWindow>(&window_manager_, delegate,
                                            std::move(properties));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  FakeFlatland fake_flatland_;
  FakeViewRefFocused fake_view_ref_focused_;

 private:
  base::TestComponentContextForProcess test_context_;
  // Injects binding for responding to Flatland protocol connection requests.
  const base::ScopedServicePublisher<fuchsia::ui::composition::Flatland>
      fake_flatland_publisher_;
  // Injects binding for responding to ViewRefFocused protocol connection
  // requests coming to |fake_flatland_|.
  fidl::Binding<fuchsia::ui::views::ViewRefFocused>
      fake_view_ref_focused_binding_;

  FlatlandWindowManager window_manager_;
};

TEST_F(FlatlandWindowTest, Initialization) {
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget window_widget;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&window_widget));

  auto window = CreateFlatlandWindow(&delegate);
  ASSERT_NE(window_widget, gfx::kNullAcceleratedWidget);

  // Check that there are no crashes after flushing tasks.
  task_environment_.RunUntilIdle();
}

// Tests that FlatlandWindow processes and delegates focus signal.
TEST_F(FlatlandWindowTest, ProcessesFocusedSignal) {
  MockPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_));
  auto window = CreateFlatlandWindow(&delegate);

  // FlatlandWindow should start watching in ctor.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_view_ref_focused_.times_watched(), 1u);

  // Send focused=true signal.
  bool focus_delegated = false;
  EXPECT_CALL(delegate, OnActivationChanged(_))
      .WillRepeatedly(SaveArg<0>(&focus_delegated));
  fake_view_ref_focused_.ScheduleCallback(true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_view_ref_focused_.times_watched(), 2u);
  EXPECT_TRUE(focus_delegated);

  // Send focused=false signal.
  fake_view_ref_focused_.ScheduleCallback(false);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(fake_view_ref_focused_.times_watched(), 3u);
  EXPECT_FALSE(focus_delegated);
}

}  // namespace ui
