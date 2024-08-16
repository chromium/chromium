// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"

#include <utility>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/test_platform_native_widget.h"
#include "ui/views/view_test_api.h"

#if defined(USE_AURA)
#include "ui/views/widget/native_widget_aura.h"
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif
#elif BUILDFLAG(IS_MAC)
#include "ui/views/widget/native_widget_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace views {

void ViewsTestBase::WidgetCloser::operator()(Widget* widget) const {
  widget->CloseNow();
}

ViewsTestBase::ViewsTestBase(
    std::unique_ptr<base::test::TaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // ViewsTestBase is also used by a number of Lacros interactive_ui_tests.
  // Lacros interactive_ui_tests are expected to process the
  // --lacros-mojo-socket-for-testing command line switch in order to set up
  // crosapi. However, ViewsTestBase doesn't implement that as it's also used by
  // other tests and doesn't need crosapi. Hence disable crosapi explicitly.
  chromeos::BrowserParamsProxy::DisableCrosapiForTesting();
#endif
}

ViewsTestBase::~ViewsTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";
}

void ViewsTestBase::SetUp() {
  testing::Test::SetUp();
  setup_called_ = true;

  std::optional<ViewsDelegate::NativeWidgetFactory> factory;
  if (native_widget_type_ == NativeWidgetType::kDesktop) {
    factory = base::BindRepeating(&ViewsTestBase::CreateNativeWidgetForTest,
                                  base::Unretained(this));
  }
  test_helper_ = std::make_unique<ScopedViewsTestHelper>(
      std::move(views_delegate_for_setup_), std::move(factory));
}

void ViewsTestBase::TearDown() {
  if (interactive_setup_called_) {
    ui::ResourceBundle::CleanupSharedInstance();
  }
  ui::Clipboard::DestroyClipboardForCurrentThread();

  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunPendingMessages();
  teardown_called_ = true;
  testing::Test::TearDown();
  test_helper_.reset();
  ax_platform_.reset();
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
  ax_platform_.emplace();
}

void ViewsTestBase::RunPendingMessages() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

Widget::InitParams ViewsTestBase::CreateParams(
    Widget::InitParams::Ownership ownership,
    Widget::InitParams::Type type) {
  Widget::InitParams params(ownership, type);
  params.context = GetContext();
  return params;
}

Widget::InitParams ViewsTestBase::CreateParams(Widget::InitParams::Type type) {
  return CreateParams(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET, type);
}

std::unique_ptr<Widget> ViewsTestBase::CreateTestWidget(
    Widget::InitParams::Ownership ownership,
    Widget::InitParams::Type type) {
  return CreateTestWidget(CreateParamsForTestWidget(ownership, type));
}

std::unique_ptr<Widget> ViewsTestBase::CreateTestWidget(
    Widget::InitParams params) {
  std::unique_ptr<Widget> widget = AllocateTestWidget();
  widget->Init(std::move(params));
  return widget;
}

void ViewsTestBase::SimulateNativeDestroy(Widget* widget) {
  test_helper_->SimulateNativeDestroy(widget);
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
void ViewsTestBase::SimulateDesktopNativeDestroy(Widget* widget) {
  test_helper_->SimulateDesktopNativeDestroy(widget);
}
#endif

#if !BUILDFLAG(IS_MAC)
int ViewsTestBase::GetSystemReservedHeightAtTopOfScreen() {
  return 0;
}
#endif

gfx::NativeWindow ViewsTestBase::GetContext() {
  return test_helper_->GetContext();
}

NativeWidget* ViewsTestBase::CreateNativeWidgetForTest(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
#if BUILDFLAG(IS_MAC)
  return new test::TestPlatformNativeWidget<NativeWidgetMac>(delegate, false,
                                                             nullptr);
#elif defined(USE_AURA)
  // For widgets that have a modal parent, don't force a native widget type.
  // This logic matches DesktopTestViewsDelegate as well as ChromeViewsDelegate.
  if (init_params.parent && init_params.type != Widget::InitParams::TYPE_MENU &&
      init_params.type != Widget::InitParams::TYPE_TOOLTIP) {
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
#endif
}

std::unique_ptr<Widget> ViewsTestBase::AllocateTestWidget() {
  return std::make_unique<Widget>();
}

Widget::InitParams ViewsTestBase::CreateParamsForTestWidget(
    Widget::InitParams::Ownership ownership,
    Widget::InitParams::Type type) {
  Widget::InitParams params = CreateParams(ownership, type);
  params.bounds = gfx::Rect(0, 0, 400, 400);
  return params;
}

Widget::InitParams ViewsTestBase::CreateParamsForTestWidget(
    Widget::InitParams::Type type) {
  return CreateParamsForTestWidget(
      Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, type);
}

void ViewsTestWithDesktopNativeWidget::SetUp() {
  set_native_widget_type(NativeWidgetType::kDesktop);
  ViewsTestBase::SetUp();
}

}  // namespace views
