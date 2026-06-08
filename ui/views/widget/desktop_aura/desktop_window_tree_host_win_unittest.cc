// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

#include <windows.h>

#include <oleacc.h>

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/ax_system_caret_win.h"
#include "ui/views/test/desktop_window_tree_host_win_test_api.h"
#include "ui/views/test/widget_test.h"
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
  base::test::ScopedFeatureList scoped_feature_list(::features::kUiaProvider);

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
}

}  // namespace test
}  // namespace views
