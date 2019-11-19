// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"

#include <utility>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/test/test_platform_native_widget.h"

#if defined(USE_AURA)
#include "ui/views/widget/native_widget_aura.h"
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif
#elif defined(OS_MACOSX)
#include "ui/views/widget/native_widget_mac.h"
#endif

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

ViewsTestBase::ViewsTestBase(
    ViewsTestBase::SubclassManagesTaskEnvironment /* tag */)
    : task_environment_(base::nullopt) {
  // MaterialDesignController is initialized here instead of in SetUp because
  // a subclass might construct a MaterialDesignControllerTestAPI as a member to
  // override the value, and this must happen first.
  ui::MaterialDesignController::Initialize();
}

ViewsTestBase::~ViewsTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";
}

void ViewsTestBase::SetUp() {
  has_compositing_manager_ = InitializeVisuals();

  testing::Test::SetUp();
  setup_called_ = true;
  if (!views_delegate_for_setup_)
    views_delegate_for_setup_ = std::make_unique<TestViewsDelegate>();

  if (native_widget_type_ == NativeWidgetType::kDesktop) {
    ViewsDelegate::GetInstance()->set_native_widget_factory(base::BindRepeating(
        &ViewsTestBase::CreateNativeWidgetForTest, base::Unretained(this)));
  }

  test_helper_ = std::make_unique<ScopedViewsTestHelper>(
      std::move(views_delegate_for_setup_));
}

void ViewsTestBase::TearDown() {
  if (interactive_setup_called_)
    ui::ResourceBundle::CleanupSharedInstance();
  ui::Clipboard::DestroyClipboardForCurrentThread();

  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunPendingMessages();
  teardown_called_ = true;
  testing::Test::TearDown();
  test_helper_.reset();
}

void ViewsTestBase::SetUpForInteractiveTests() {
  DCHECK(!setup_called_);
  interactive_setup_called_ = true;

  // Mojo is initialized here similar to how each browser test case initializes
  // Mojo when starting. This only works because each interactive_ui_test runs
  // in a new process.
  mojo::core::Init();

  gl::GLSurfaceTestSupport::InitializeOneOff();
  ui::RegisterPathProvider();
  base::FilePath ui_test_pak_path;
  ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
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

NativeWidget* ViewsTestBase::CreateNativeWidgetForTest(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
#if defined(OS_MACOSX)
  return new test::TestPlatformNativeWidget<NativeWidgetMac>(delegate, false,
                                                             nullptr);
#elif defined(USE_AURA)
  // For widgets that have a modal parent, don't force a native widget type.
  // This logic matches DesktopTestViewsDelegate as well as ChromeViewsDelegate.
  if (init_params.parent &&
      init_params.type != views::Widget::InitParams::TYPE_MENU &&
      init_params.type != views::Widget::InitParams::TYPE_TOOLTIP) {
    // Returning null results in using the platform default, which is
    // NativeWidgetAura.
    return nullptr;
  }

  if (native_widget_type_ == NativeWidgetType::kDesktop) {
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
    return new test::TestPlatformNativeWidget<DesktopNativeWidgetAura>(
        delegate, false, nullptr);
#else
    return new test::TestPlatformNativeWidget<NativeWidgetAura>(delegate, false,
                                                                nullptr);
#endif
  }

  return new test::TestPlatformNativeWidget<NativeWidgetAura>(delegate, true,
                                                              nullptr);
#else
  NOTREACHED();
  return nullptr;
#endif
}

void ViewsTestBaseWithNativeWidgetType::SetUp() {
  set_native_widget_type(GetParam());
  ViewsTestBase::SetUp();
}

void ViewsTestWithDesktopNativeWidget::SetUp() {
  set_native_widget_type(NativeWidgetType::kDesktop);
  ViewsTestBase::SetUp();
}

}  // namespace views
