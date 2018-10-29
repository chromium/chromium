// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"

#include <utility>

#include "base/environment.h"
#include "base/run_loop.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/test/platform_test_helper.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util_internal.h"
#endif

namespace views {

namespace {

bool InitializeVisuals() {
#if defined(USE_X11)
  bool has_compositing_manager = false;
  int depth = 0;
  bool using_argb_visual;

  if (depth > 0)
    return has_compositing_manager;

  // testing/xvfb.py runs xvfb and xcompmgr.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  has_compositing_manager = env->HasVar("_CHROMIUM_INSIDE_XVFB");
  ui::XVisualManager::GetInstance()->ChooseVisualForWindow(
      has_compositing_manager, nullptr, &depth, nullptr, &using_argb_visual);

  if (using_argb_visual)
    EXPECT_EQ(32, depth);

  return using_argb_visual;
#else
  return false;
#endif
}

}  // namespace

ViewsTestBase::ViewsTestBase()
    : scoped_task_environment_(
          base::test::ScopedTaskEnvironment::MainThreadType::UI),
      setup_called_(false),
      teardown_called_(false),
      has_compositing_manager_(InitializeVisuals()) {}

ViewsTestBase::~ViewsTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";
}

// static
bool ViewsTestBase::IsMus() {
  return PlatformTestHelper::IsMus();
}

void ViewsTestBase::SetUp() {
  testing::Test::SetUp();
  ui::MaterialDesignController::Initialize();
  setup_called_ = true;
  if (!views_delegate_for_setup_)
    views_delegate_for_setup_.reset(new TestViewsDelegate());

  test_helper_.reset(
      new ScopedViewsTestHelper(std::move(views_delegate_for_setup_)));
}

void ViewsTestBase::TearDown() {
  ui::Clipboard::DestroyClipboardForCurrentThread();

  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunPendingMessages();
  teardown_called_ = true;
  testing::Test::TearDown();
  test_helper_.reset();
}

void ViewsTestBase::RunPendingMessages() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

Widget::InitParams ViewsTestBase::CreateParams(
    Widget::InitParams::Type type) {
  Widget::InitParams params(type);
  params.context = GetContext();
  return params;
}

bool ViewsTestBase::HasCompositingManager() const {
  return has_compositing_manager_;
}

void ViewsTestBase::SimulateNativeDestroy(Widget* widget) {
  test_helper_->platform_test_helper()->SimulateNativeDestroy(widget);
}

gfx::NativeWindow ViewsTestBase::GetContext() {
  return test_helper_->GetContext();
}

}  // namespace views
