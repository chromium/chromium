// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

// clang-format off
#include "base/test/scoped_feature_list.h"
#include <windows.h>
// clang-format on

#include <oleacc.h>

#include <string_view>
#include <utility>

#include "base/functional/function_ref.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/win/windows_version.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/ax_system_caret_win.h"
#include "ui/views/test/desktop_window_tree_host_win_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_message_handler.h"

namespace views {
namespace test {

using DesktopWindowTreeHostWinTest = DesktopWidgetTest;

TEST_F(DesktopWindowTreeHostWinTest, DebuggingId) {
  Widget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  constexpr char kDebuggingName[] = "test-debugging-id";
  params.name = kDebuggingName;
  widget.Init(std::move(params));
  DesktopWindowTreeHostWin* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWin*>(
          widget.GetNativeWindow()->GetHost());
  EXPECT_EQ(std::string(kDebuggingName),
            DesktopWindowTreeHostWinTestApi(desktop_window_tree_host)
                .GetHwndMessageHandler()
                ->debugging_id());
}

TEST_F(DesktopWindowTreeHostWinTest, RedundantSetCapture) {
  Widget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget.Init(std::move(params));
  widget.Show();

  HWNDMessageHandler* handler =
      DesktopWindowTreeHostWinTestApi(static_cast<DesktopWindowTreeHostWin*>(
                                          widget.GetNativeWindow()->GetHost()))
          .GetHwndMessageHandler();

  handler->SetCapture();
  EXPECT_TRUE(handler->HasCapture());

  // Second set capture should no-op. Should not crash.
  handler->SetCapture();
  EXPECT_TRUE(handler->HasCapture());

  handler->ReleaseCapture();
  EXPECT_FALSE(handler->HasCapture());
}

TEST_F(DesktopWindowTreeHostWinTest, SetAllowScreenshots) {
  Widget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget.Init(std::move(params));

  DesktopWindowTreeHostWin* host = static_cast<DesktopWindowTreeHostWin*>(
      widget.GetNativeWindow()->GetHost());
  DesktopWindowTreeHostWinTestApi host_api(host);

  // Set not allow screenshots.
  widget.SetAllowScreenshots(false);

  // The logical state updates immediately.
  EXPECT_FALSE(widget.AreScreenshotsAllowed());

  // But the OS affinity will not be set because the widget is not shown.
  DWORD affinity;
  HWND hwnd = host_api.GetHWND();
  EXPECT_TRUE(::GetWindowDisplayAffinity(hwnd, &affinity));
  EXPECT_EQ(static_cast<DWORD>(WDA_NONE), affinity);

  // Show the widget and the OS display affinity should be updated.
  widget.Show();
  EXPECT_TRUE(::GetWindowDisplayAffinity(hwnd, &affinity));
  EXPECT_EQ(static_cast<DWORD>(WDA_MONITOR), affinity);

  // Toggling while showing should take effect immediately at the OS level.
  widget.SetAllowScreenshots(true);
  EXPECT_TRUE(widget.AreScreenshotsAllowed());
  EXPECT_TRUE(::GetWindowDisplayAffinity(hwnd, &affinity));
  EXPECT_EQ(static_cast<DWORD>(WDA_NONE), affinity);
}

TEST_F(DesktopWindowTreeHostWinTest, SetExcludeFromScreenCapture) {
  Widget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget.Init(std::move(params));

  DesktopWindowTreeHostWin* host = static_cast<DesktopWindowTreeHostWin*>(
      widget.GetNativeWindow()->GetHost());
  DesktopWindowTreeHostWinTestApi host_api(host);

  // Force simulation of local session so the test behaves consistently
  // regardless of whether the build/test host is a remote session VM.
  host_api.SetRemoteSessionForTesting(false);

  // Set exclude from screen capture.
  widget.SetExcludeFromScreenCapture(true);

  // It will not be set because the widget is not shown.
  DWORD affinity;
  HWND hwnd = host_api.GetHWND();
  EXPECT_TRUE(::GetWindowDisplayAffinity(hwnd, &affinity));
  EXPECT_EQ(static_cast<DWORD>(WDA_NONE), affinity);

  // Show the widget and should update the display affinity.
  widget.Show();
  EXPECT_TRUE(::GetWindowDisplayAffinity(hwnd, &affinity));
  if (base::win::GetVersion() >= base::win::Version::WIN10_20H1) {
    EXPECT_EQ(static_cast<DWORD>(WDA_EXCLUDEFROMCAPTURE), affinity);
  } else {
    EXPECT_EQ(static_cast<DWORD>(WDA_MONITOR), affinity);
  }

  // Toggling while showing should take effect immediately.
  widget.SetExcludeFromScreenCapture(false);
  EXPECT_TRUE(::GetWindowDisplayAffinity(hwnd, &affinity));
  EXPECT_EQ(static_cast<DWORD>(WDA_NONE), affinity);
}

struct RemoteSessionExclusionTestParams {
  bool is_remote;
  bool feature_enabled;
  bool expect_exclusion;
};

class DesktopWindowTreeHostWinRemoteSessionTest
    : public DesktopWindowTreeHostWinTest,
      public ::testing::WithParamInterface<RemoteSessionExclusionTestParams> {
 public:
  DesktopWindowTreeHostWinRemoteSessionTest() = default;
};

TEST_P(DesktopWindowTreeHostWinRemoteSessionTest, UpdateDisplayAffinity) {
  const RemoteSessionExclusionTestParams& test_params = GetParam();

  ::base::test::ScopedFeatureList scoped_feature_list;
  if (test_params.feature_enabled) {
    scoped_feature_list.InitAndEnableFeature(
        views::features::kAllowWindowCaptureExclusionInRemoteSessions);
  } else {
    scoped_feature_list.InitAndDisableFeature(
        views::features::kAllowWindowCaptureExclusionInRemoteSessions);
  }

  Widget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget.Init(std::move(params));

  DesktopWindowTreeHostWin* host = static_cast<DesktopWindowTreeHostWin*>(
      widget.GetNativeWindow()->GetHost());
  DesktopWindowTreeHostWinTestApi host_api(host);

  // Configure simulated remote session state before setting capture exclusion
  // and showing.
  host_api.SetRemoteSessionForTesting(test_params.is_remote);
  widget.SetExcludeFromScreenCapture(true);
  widget.Show();

  DWORD affinity;
  HWND hwnd = host_api.GetHWND();
  EXPECT_TRUE(::GetWindowDisplayAffinity(hwnd, &affinity));

  if (test_params.expect_exclusion) {
    if (base::win::GetVersion() >= base::win::Version::WIN10_20H1) {
      EXPECT_EQ(static_cast<DWORD>(WDA_EXCLUDEFROMCAPTURE), affinity);
    } else {
      EXPECT_EQ(static_cast<DWORD>(WDA_MONITOR), affinity);
    }
  } else {
    EXPECT_EQ(static_cast<DWORD>(WDA_NONE), affinity);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DesktopWindowTreeHostWinRemoteSessionTest,
    ::testing::Values(
        // Remote active, feature disabled (default) -> Bypassed (affinity
        // WDA_NONE)
        RemoteSessionExclusionTestParams{/*is_remote=*/true,
                                         /*feature_enabled=*/false,
                                         /*expect_exclusion=*/false},
        // Remote active, feature enabled -> Exclusion allowed
        RemoteSessionExclusionTestParams{/*is_remote=*/true,
                                         /*feature_enabled=*/true,
                                         /*expect_exclusion=*/true},
        // Remote inactive, feature disabled -> Exclusion allowed
        RemoteSessionExclusionTestParams{/*is_remote=*/false,
                                         /*feature_enabled=*/false,
                                         /*expect_exclusion=*/true}));

class DesktopWindowTreeHostWinAccessibilityObjectTest
    : public DesktopWidgetTest {
 public:
  DesktopWindowTreeHostWinAccessibilityObjectTest() = default;

  DesktopWindowTreeHostWinAccessibilityObjectTest(
      const DesktopWindowTreeHostWinAccessibilityObjectTest&) = delete;
  DesktopWindowTreeHostWinAccessibilityObjectTest& operator=(
      const DesktopWindowTreeHostWinAccessibilityObjectTest&) = delete;

  ~DesktopWindowTreeHostWinAccessibilityObjectTest() override = default;

 protected:
  void CacheRootNode(const Widget& widget) {
    DesktopWindowTreeHostWinTestApi host(static_cast<DesktopWindowTreeHostWin*>(
        widget.GetNativeWindow()->GetHost()));
    host.GetNativeViewAccessible()->QueryInterface(IID_PPV_ARGS(&test_node_));
  }

  void CacheCaretNode(const Widget& widget) {
    DesktopWindowTreeHostWinTestApi host(static_cast<DesktopWindowTreeHostWin*>(
        widget.GetNativeWindow()->GetHost()));

    host.EnsureAXSystemCaretCreated();

    host.GetAXSystemCaret()->GetCaret()->QueryInterface(
        IID_PPV_ARGS(&test_node_));
  }

  Microsoft::WRL::ComPtr<ui::AXPlatformNodeWin> test_node_;
};

// This test validates that we do not leak the root accessibility object when
// handing it out.
TEST_F(DesktopWindowTreeHostWinAccessibilityObjectTest, RootDoesNotLeak) {
  {
    Widget widget;
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    widget.Init(std::move(params));
    widget.Show();

    // Cache a pointer to the object we return to Windows.
    CacheRootNode(widget);

    // Repeatedly call the public API to obtain an accessibility object. If our
    // code is leaking references, this will drive up the reference count.
    HWND hwnd = widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget();
    for (int i = 0; i < 10; i++) {
      Microsoft::WRL::ComPtr<IAccessible> root_accessible;
      EXPECT_HRESULT_SUCCEEDED(::AccessibleObjectFromWindow(
          hwnd, OBJID_CLIENT, IID_PPV_ARGS(&root_accessible)));
      EXPECT_NE(root_accessible.Get(), nullptr);
    }

    // Close the widget and destroy it by letting it go out of scope.
    widget.CloseNow();
  }

  // At this point our test reference should be the only one remaining.
  EXPECT_EQ(test_node_->ref_count_for_testing(), 1u);
}

// This test validates that we do not leak the caret accessibility object when
// handing it out.
TEST_F(DesktopWindowTreeHostWinAccessibilityObjectTest, CaretDoesNotLeak) {
  {
    Widget widget;
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    widget.Init(std::move(params));
    widget.Show();

    // Cache a pointer to the object we return to Windows.
    CacheCaretNode(widget);

    // Repeatedly call the public API to obtain an accessibility object. If our
    // code is leaking references, this will drive up the reference count.
    HWND hwnd = widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget();
    for (int i = 0; i < 10; i++) {
      Microsoft::WRL::ComPtr<IAccessible> caret_accessible;
      EXPECT_HRESULT_SUCCEEDED(::AccessibleObjectFromWindow(
          hwnd, OBJID_CARET, IID_PPV_ARGS(&caret_accessible)));
      EXPECT_NE(caret_accessible.Get(), nullptr);
    }

    // Close the widget and destroy it by letting it go out of scope.
    widget.CloseNow();
  }

  // At this point our test reference should be the only one remaining.
  EXPECT_EQ(test_node_->ref_count_for_testing(), 1u);
}

// This test validates that we do not leak the root accessibility object when
// handing it out (UIA mode).
TEST_F(DesktopWindowTreeHostWinAccessibilityObjectTest, UiaRootDoesNotLeak) {
  {
    Widget widget;
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    widget.Init(std::move(params));
    widget.Show();

    // Cache a pointer to the object we return to Windows.
    CacheRootNode(widget);

    // Repeatedly call the public API to obtain an accessibility object. If our
    // code is leaking references, this will drive up the reference count.
    Microsoft::WRL::ComPtr<IUIAutomation> uia;
    ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                              CLSCTX_INPROC_SERVER,
                                              IID_IUIAutomation, &uia));
    HWND hwnd = widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget();
    for (int i = 0; i < 10; i++) {
      Microsoft::WRL::ComPtr<IUIAutomationElement> root_element;
      EXPECT_HRESULT_SUCCEEDED(uia->ElementFromHandle(hwnd, &root_element));
      EXPECT_NE(root_element.Get(), nullptr);

      // Raise an event on the root node. This will cause UIA to cache a pointer
      // to it.
      ::UiaRaiseStructureChangedEvent(test_node_.Get(),
                                      StructureChangeType_ChildrenInvalidated,
                                      nullptr, 0);
    }

    // Close the widget and destroy it by letting it go out of scope.
    widget.CloseNow();
  }

  // At this point our test reference should be the only one remaining.
  EXPECT_EQ(test_node_->ref_count_for_testing(), 1u);
}

TEST_F(DesktopWindowTreeHostWinTest, IsInNativeMoveResizeLoop) {
  Widget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget.Init(std::move(params));
  widget.Show();

  DesktopWindowTreeHostWin* host = static_cast<DesktopWindowTreeHostWin*>(
      widget.GetNativeWindow()->GetHost());
  EXPECT_FALSE(host->IsInNativeMoveResizeLoop());

  HWND hwnd = widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  ::SendMessage(hwnd, WM_ENTERMENULOOP, FALSE, 0);
  EXPECT_TRUE(host->IsInNativeMoveResizeLoop());

  ::SendMessage(hwnd, WM_EXITMENULOOP, FALSE, 0);
  EXPECT_FALSE(host->IsInNativeMoveResizeLoop());
}

TEST_F(DesktopWindowTreeHostWinTest, IsInNativeMoveResizeLoopAcrossWindows) {
  Widget widget_a;
  widget_a.Init(CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                             Widget::InitParams::TYPE_WINDOW));
  widget_a.Show();

  Widget widget_b;
  widget_b.Init(CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                             Widget::InitParams::TYPE_WINDOW));
  widget_b.Show();

  DesktopWindowTreeHostWin* host_a = static_cast<DesktopWindowTreeHostWin*>(
      widget_a.GetNativeWindow()->GetHost());
  DesktopWindowTreeHostWin* host_b = static_cast<DesktopWindowTreeHostWin*>(
      widget_b.GetNativeWindow()->GetHost());
  EXPECT_FALSE(host_a->IsInNativeMoveResizeLoop());
  EXPECT_FALSE(host_b->IsInNativeMoveResizeLoop());

  // While one window is running a native menu loop, all hosts on the thread
  // should report that a modal loop is active.
  HWND hwnd_a = widget_a.GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  ::SendMessage(hwnd_a, WM_ENTERMENULOOP, FALSE, 0);
  EXPECT_TRUE(host_a->IsInNativeMoveResizeLoop());
  EXPECT_TRUE(host_b->IsInNativeMoveResizeLoop());

  ::SendMessage(hwnd_a, WM_EXITMENULOOP, FALSE, 0);
  EXPECT_FALSE(host_a->IsInNativeMoveResizeLoop());
  EXPECT_FALSE(host_b->IsInNativeMoveResizeLoop());
}

// The native size/move modal loop is already on the stack once WM_ENTERSIZEMOVE
// has been delivered, but no WM_SIZING/WM_MOVING arrives until the user moves
// the mouse. That loop pumps application tasks (including Mojo IPCs from
// renderers), so IsInNativeMoveResizeLoop() must already report true at
// WM_ENTERSIZEMOVE. Otherwise a task running in the gap sees false and can
// start an OS-level drag while the user is holding a resize border.
TEST_F(DesktopWindowTreeHostWinTest, IsInNativeMoveResizeLoopOnEnterSizeMove) {
  Widget widget;
  widget.Init(CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                           Widget::InitParams::TYPE_WINDOW));
  widget.Show();

  DesktopWindowTreeHostWin* host = static_cast<DesktopWindowTreeHostWin*>(
      widget.GetNativeWindow()->GetHost());
  EXPECT_FALSE(host->IsInNativeMoveResizeLoop());

  HWND hwnd = widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget();

  // The user has pressed the mouse on a resize border. Windows has entered the
  // modal loop, but the mouse has not moved yet.
  ::SendMessage(hwnd, WM_ENTERSIZEMOVE, 0, 0);
  EXPECT_TRUE(host->IsInNativeMoveResizeLoop());

  // The user now drags, producing the first WM_SIZING.
  RECT rect = {};
  ::GetWindowRect(hwnd, &rect);
  ::SendMessage(hwnd, WM_SIZING, WMSZ_RIGHT, reinterpret_cast<LPARAM>(&rect));
  EXPECT_TRUE(host->IsInNativeMoveResizeLoop());

  ::SendMessage(hwnd, WM_EXITSIZEMOVE, 0, 0);
  EXPECT_FALSE(host->IsInNativeMoveResizeLoop());
}

namespace {

DWORD GetExpectedExclusionAffinity() {
  return (base::win::GetVersion() >= base::win::Version::WIN10_20H1)
             ? static_cast<DWORD>(WDA_EXCLUDEFROMCAPTURE)
             : static_cast<DWORD>(WDA_MONITOR);
}

// Helper to initialize and show a widget with simulated local session for
// consistent capture exclusion testing.
HWND InitTestWidget(Widget& widget,
                    Widget::InitParams params,
                    bool exclude_capture = false) {
  widget.Init(std::move(params));
  auto* host = static_cast<DesktopWindowTreeHostWin*>(
      widget.GetNativeWindow()->GetHost());
  DesktopWindowTreeHostWinTestApi host_api(host);
  host_api.SetRemoteSessionForTesting(false);
  if (exclude_capture) {
    widget.SetExcludeFromScreenCapture(true);
  }
  widget.Show();
  return host_api.GetHWND();
}

// Helper to find the active Win32 #32768 popup menu window on the thread.
HWND FindActivePopupMenuHWND() {
  HWND found_menu_hwnd = nullptr;
  ::EnumThreadWindows(
      ::GetCurrentThreadId(),
      [](HWND hwnd, LPARAM lParam) -> BOOL {
        constexpr wchar_t kSystemMenuClassName[] = L"#32768";
        wchar_t class_name[32];
        const int len = ::GetClassName(hwnd, class_name, std::size(class_name));
        if (len > 0 &&
            std::wstring_view(class_name, static_cast<size_t>(len)) ==
                kSystemMenuClassName) {
          *reinterpret_cast<HWND*>(lParam) = hwnd;
          return FALSE;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&found_menu_hwnd));
  return found_menu_hwnd;
}

// Helper to verify that a child window inherits capture exclusion from its
// parent and dynamically tracks updates when parent exclusion is toggled.
void VerifyExclusionPropagation(Widget* parent_widget, HWND child_hwnd) {
  SCOPED_TRACE("VerifyExclusionPropagation");
  DWORD affinity = WDA_NONE;
  EXPECT_TRUE(::GetWindowDisplayAffinity(child_hwnd, &affinity));
  EXPECT_EQ(GetExpectedExclusionAffinity(), affinity);

  // Verify dynamic downward propagation: when parent's capture exclusion is
  // disabled, the child should also be updated immediately.
  parent_widget->SetExcludeFromScreenCapture(false);
  EXPECT_TRUE(::GetWindowDisplayAffinity(child_hwnd, &affinity));
  EXPECT_EQ(static_cast<DWORD>(WDA_NONE), affinity);

  // Toggling parent's capture exclusion back on should also propagate down.
  parent_widget->SetExcludeFromScreenCapture(true);
  EXPECT_TRUE(::GetWindowDisplayAffinity(child_hwnd, &affinity));
  EXPECT_EQ(GetExpectedExclusionAffinity(), affinity);
}

// Helper to display a native popup menu modally, locate its Win32 #32768
// window, execute a test callback, and dismiss the menu.
void ShowTestPopupMenu(HWND hwnd, base::FunctionRef<void(HWND)> callback) {
  struct Context {
    base::FunctionRef<void(HWND)> callback;
    bool callback_executed = false;
  } context{callback};

  // Use a thread-local pointer to bridge the C++ lambda into the Win32
  // TIMERPROC callback for the duration of the synchronous modal loop.
  static thread_local Context* g_context = nullptr;
  g_context = &context;

  ::BringWindowToTop(hwnd);
  ::SetActiveWindow(hwnd);
  ::SetForegroundWindow(hwnd);

  // Use a thread timer (null HWND) to ensure timer messages are delivered
  // directly during the modal TrackPopupMenu message loop.
  UINT_PTR timer_id =
      ::SetTimer(nullptr, 0, 10, [](HWND, UINT, UINT_PTR id, DWORD) {
        if (g_context && !g_context->callback_executed) {
          if (HWND menu_hwnd = FindActivePopupMenuHWND()) {
            g_context->callback_executed = true;
            ::KillTimer(nullptr, id);
            // Execute the callback outside of EnumThreadWindows to
            // avoid nested EnumThreadWindows calls on the same
            // thread when modifying widget capture exclusion inside
            // the callback.
            g_context->callback(menu_hwnd);
            ::EndMenu();
          }
        }
      });

  HMENU menu = ::CreatePopupMenu();
  ::AppendMenu(menu, MF_STRING, 1, L"Test Item");
  ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_NOANIMATION, 0, 0,
                   0, hwnd, nullptr);
  ::DestroyMenu(menu);
  ::KillTimer(nullptr, timer_id);

  EXPECT_TRUE(context.callback_executed);
  g_context = nullptr;
}

}  // namespace

TEST_F(DesktopWindowTreeHostWinTest, ExcludeActiveSystemMenuFromCapture) {
  Widget widget;
  HWND hwnd =
      InitTestWidget(widget,
                     CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_WINDOW),
                     /*exclude_capture=*/true);

  ShowTestPopupMenu(hwnd, [&](HWND menu_hwnd) {
    DWORD affinity = WDA_NONE;
    EXPECT_TRUE(::GetWindowDisplayAffinity(menu_hwnd, &affinity));
    EXPECT_EQ(GetExpectedExclusionAffinity(), affinity);
  });
  widget.CloseNow();
}

TEST_F(DesktopWindowTreeHostWinTest, ExcludeActiveSystemMenuFromCaptureUpdate) {
  Widget widget;
  HWND hwnd =
      InitTestWidget(widget,
                     CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_WINDOW),
                     /*exclude_capture=*/false);

  ShowTestPopupMenu(hwnd, [&](HWND menu_hwnd) {
    DWORD affinity = WDA_NONE;
    EXPECT_TRUE(::GetWindowDisplayAffinity(menu_hwnd, &affinity));
    EXPECT_EQ(static_cast<DWORD>(WDA_NONE), affinity);

    // Enable capture exclusion while menu is open.
    widget.SetExcludeFromScreenCapture(true);
    EXPECT_TRUE(::GetWindowDisplayAffinity(menu_hwnd, &affinity));
    EXPECT_EQ(GetExpectedExclusionAffinity(), affinity);

    // Disable capture exclusion while menu is open.
    widget.SetExcludeFromScreenCapture(false);
    EXPECT_TRUE(::GetWindowDisplayAffinity(menu_hwnd, &affinity));
    EXPECT_EQ(static_cast<DWORD>(WDA_NONE), affinity);
  });
  widget.CloseNow();
}

TEST_F(DesktopWindowTreeHostWinTest, ExcludeOwnedWindowsFromCapture) {
  Widget parent_widget;
  HWND parent_hwnd =
      InitTestWidget(parent_widget,
                     CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_WINDOW),
                     /*exclude_capture=*/true);

  // Create an owned/child top-level widget (like a bubble or menu).
  Widget child_widget;
  Widget::InitParams child_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_MENU);
  child_params.parent = parent_widget.GetNativeWindow();
  HWND child_hwnd = InitTestWidget(child_widget, std::move(child_params));

  // Verify that the child window's Win32 owner is indeed the parent window.
  EXPECT_EQ(parent_hwnd, ::GetWindow(child_hwnd, GW_OWNER));

  VerifyExclusionPropagation(&parent_widget, child_hwnd);

  child_widget.CloseNow();
  parent_widget.CloseNow();
}

TEST_F(DesktopWindowTreeHostWinTest, ExcludeContextWindowsFromCapture) {
  Widget parent_widget;
  HWND parent_hwnd =
      InitTestWidget(parent_widget,
                     CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_WINDOW),
                     /*exclude_capture=*/true);

  // Create a descendant aura::Window inside the parent widget (simulating a
  // view or child window such as the search bar / FindBarHost).
  auto* descendant_window =
      new aura::Window(nullptr, aura::client::WINDOW_TYPE_CONTROL);
  descendant_window->Init(ui::LAYER_NOT_DRAWN);
  parent_widget.GetNativeWindow()->AddChild(descendant_window);

  // Create a tooltip widget using params.context = descendant_window, exactly
  // as corewm::TooltipAura does.
  Widget tooltip_widget;
  Widget::InitParams tooltip_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_TOOLTIP);
  tooltip_params.context = descendant_window;
  tooltip_params.force_software_compositing = true;
  HWND tooltip_hwnd = InitTestWidget(tooltip_widget, std::move(tooltip_params));

  // Verify that the tooltip window's Win32 owner is resolved to parent window.
  EXPECT_EQ(parent_hwnd, ::GetWindow(tooltip_hwnd, GW_OWNER));

  VerifyExclusionPropagation(&parent_widget, tooltip_hwnd);

  tooltip_widget.CloseNow();
  parent_widget.CloseNow();
}

}  // namespace test
}  // namespace views
