// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_BASE_H_
#define UI_VIEWS_TEST_VIEWS_TEST_BASE_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace views {

// A base class for views unit test. It creates a message loop necessary
// to drive UI events and takes care of OLE initialization for windows.
class ViewsTestBase : public PlatformTest {
 public:
  enum class NativeWidgetType {
    // On Aura, corresponds to NativeWidgetAura.
    kDefault,
    // On chromeos, corresponds to NativeWidgetAura (DesktopNativeWidgetAura
    // is not used on ChromeOS).
    kDesktop,
  };

  // This class can be used as a deleter for std::unique_ptr<Widget>
  // to call function Widget::CloseNow automatically.
  struct WidgetCloser {
    void operator()(Widget* widget) const;
  };
  using WidgetAutoclosePtr = std::unique_ptr<Widget, WidgetCloser>;

  // Constructs a ViewsTestBase with |traits| being forwarded to its
  // TaskEnvironment. MainThreadType always defaults to UI and must not be
  // specified.
  template <typename... TaskEnvironmentTraits>
  NOINLINE explicit ViewsTestBase(TaskEnvironmentTraits&&... traits)
      : ViewsTestBase(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI,
            std::forward<TaskEnvironmentTraits>(traits)...)) {}

  // Alternatively a subclass may pass a TaskEnvironment directly.
  explicit ViewsTestBase(
      std::unique_ptr<base::test::TaskEnvironment> task_environment);

  ViewsTestBase(const ViewsTestBase&) = delete;
  ViewsTestBase& operator=(const ViewsTestBase&) = delete;

  ~ViewsTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // This copies some of the setup done in ViewsTestSuite, so it's only
  // necessary for a ViewsTestBase that runs out of that test suite, such as in
  // interactive ui tests.
  void SetUpForInteractiveTests();

  void RunPendingMessages();

  // Returns CreateParams for a widget of type |type|.  This is used by
  // CreateParamsForTestWidget() and thus by CreateTestWidget(), and may also be
  // used directly.  The default implementation sets the context to
  // GetContext().
  virtual Widget::InitParams CreateParams(
      Widget::InitParams::Ownership ownership,
      Widget::InitParams::Type type);

  // TODO(crbug.com/339619005): Remove once all uses are explicitly specifying
  // Widget ownership.
  Widget::InitParams CreateParams(Widget::InitParams::Type type);

  virtual std::unique_ptr<Widget> CreateTestWidget(
      Widget::InitParams::Ownership ownership,
      Widget::InitParams::Type type =
          Widget::InitParams::TYPE_WINDOW_FRAMELESS);

  virtual std::unique_ptr<Widget> CreateTestWidget(Widget::InitParams params);

  // Simulate an OS-level destruction of the native window held by non-desktop
  // |widget|.
  void SimulateNativeDestroy(Widget* widget);

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  // Simulate an OS-level destruction of the native window held by desktop
  // |widget|.
  void SimulateDesktopNativeDestroy(Widget* widget);
#endif

  // Get the system reserved height at the top of the screen. On Mac, this
  // includes the menu bar and title bar.
  static int GetSystemReservedHeightAtTopOfScreen();

 protected:
  base::test::TaskEnvironment* task_environment() {
    return task_environment_.get();
  }
  TestViewsDelegate* test_views_delegate() const {
    return test_helper_->test_views_delegate();
  }

  void set_native_widget_type(NativeWidgetType native_widget_type) {
    DCHECK(!setup_called_);
    native_widget_type_ = native_widget_type;
  }

  template <typename T>
  T* set_views_delegate(std::unique_ptr<T> views_delegate) {
    DCHECK(!setup_called_);
    T* const ret = views_delegate.get();
    views_delegate_for_setup_ = std::move(views_delegate);
    return ret;
  }

#if defined(USE_AURA)
  aura::Window* root_window() {
    return aura::test::AuraTestHelper::GetInstance()->GetContext();
  }

  ui::EventSink* GetEventSink() { return host()->GetEventSink(); }

  aura::WindowTreeHost* host() {
    return aura::test::AuraTestHelper::GetInstance()->GetHost();
  }
#endif

  // Returns a context view. In aura builds, this will be the RootWindow.
  gfx::NativeWindow GetContext();

  // Factory for creating the native widget when |native_widget_type_| is set to
  // kDesktop.
  NativeWidget* CreateNativeWidgetForTest(
      const Widget::InitParams& init_params,
      internal::NativeWidgetDelegate* delegate);

  // Instantiates a Widget for CreateTestWidget(), but does no other
  // initialization.  Overriding this allows subclasses to customize the Widget
  // subclass returned from CreateTestWidget().
  virtual std::unique_ptr<Widget> AllocateTestWidget();

  // Constructs the params for CreateTestWidget().
  Widget::InitParams CreateParamsForTestWidget(
      views::Widget::InitParams::Ownership ownership,
      views::Widget::InitParams::Type type =
          views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);

  // TODO(crbug.com/339619005): Remove once all uses are explicitly specifying
  // Widget ownership.
  Widget::InitParams CreateParamsForTestWidget(
      views::Widget::InitParams::Type type =
          views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::optional<ui::AXPlatformForTest> ax_platform_;

  // Controls what type of widget will be created by default for a test (i.e.
  // when creating a Widget and leaving InitParams::native_widget unspecified).
  // kDefault will allow the system default to be used (typically
  // NativeWidgetAura on Aura). kDesktop forces DesktopNativeWidgetAura on Aura.
  // There are exceptions, such as for modal dialog widgets, for which this
  // value is ignored.
  NativeWidgetType native_widget_type_ = NativeWidgetType::kDefault;

  std::unique_ptr<TestViewsDelegate> views_delegate_for_setup_;
  std::unique_ptr<ScopedViewsTestHelper> test_helper_;
  bool interactive_setup_called_ = false;
  bool setup_called_ = false;
  bool teardown_called_ = false;

#if BUILDFLAG(IS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif
};

// A helper that makes it easier to declare basic views tests that want to test
// desktop native widgets. See |ViewsTestBase::native_widget_type_| and
// |ViewsTestBase::CreateNativeWidgetForTest|. In short, for Aura, this will
// result in most Widgets automatically being backed by a
// DesktopNativeWidgetAura. For Mac, it has no impact as a NativeWidgetMac is
// used either way.
class ViewsTestWithDesktopNativeWidget : public ViewsTestBase {
 public:
  using ViewsTestBase::ViewsTestBase;

  ViewsTestWithDesktopNativeWidget(const ViewsTestWithDesktopNativeWidget&) =
      delete;
  ViewsTestWithDesktopNativeWidget& operator=(
      const ViewsTestWithDesktopNativeWidget&) = delete;

  ~ViewsTestWithDesktopNativeWidget() override = default;

  // ViewsTestBase:
  void SetUp() override;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_BASE_H_
