// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/hwnd_util.h"

#include <shobjidl.h>
#include <windows.h>

#include <dwmapi.h>
#include <winuser.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/win/window_impl.h"

namespace gfx {

// Test wrapper around native window HWND.
class TestNativeWindow : public WindowImpl {
 public:
  TestNativeWindow() {}

  TestNativeWindow(const TestNativeWindow&) = delete;
  TestNativeWindow& operator=(const TestNativeWindow&) = delete;

  ~TestNativeWindow() override;

 private:
  // Overridden from WindowImpl:
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override {
    return FALSE;  // Results in DefWindowProc().
  }
};

TestNativeWindow::~TestNativeWindow() {
  if (hwnd()) {
    DestroyWindow(hwnd());
  }
}

// Test wrapper around native window HWND.
class TestWin32Window {
 public:
  TestWin32Window() {}

  TestWin32Window(const TestWin32Window&) = delete;
  TestWin32Window& operator=(const TestWin32Window&) = delete;

  ~TestWin32Window();

  HWND Create(DWORD style);

 private:
  HWND hwnd_ = NULL;
};

TestWin32Window::~TestWin32Window() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
  }
}

HWND TestWin32Window::Create(DWORD style) {
  const wchar_t class_name[] = L"TestWin32Window";
  WNDCLASSEX wcex = {sizeof(wcex)};
  wcex.lpfnWndProc = DefWindowProc;
  wcex.hInstance = ::GetModuleHandle(nullptr);
  wcex.lpszClassName = class_name;
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  RegisterClassEx(&wcex);
  hwnd_ = CreateWindowEx(0, class_name, class_name, style, 0, 0, 100, 100,
                         nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  ShowWindow(hwnd_, SW_SHOWNORMAL);
  EXPECT_TRUE(UpdateWindow(hwnd_));
  return hwnd_;
}

// Registers a window class with the given name, and creates and shows a window
// of that class. Destroys the window and unregisters the class when destroyed.
class TestWindowOfClass {
 public:
  explicit TestWindowOfClass(std::wstring class_name)
      : class_name_(std::move(class_name)) {}

  TestWindowOfClass(const TestWindowOfClass&) = delete;
  TestWindowOfClass& operator=(const TestWindowOfClass&) = delete;

  ~TestWindowOfClass();

  // Returns null on failure.
  HWND Create(DWORD style, DWORD ex_style);

 private:
  const std::wstring class_name_;
  const HINSTANCE instance_ = ::GetModuleHandle(nullptr);
  bool registered_ = false;
  HWND hwnd_ = nullptr;
};

TestWindowOfClass::~TestWindowOfClass() {
  // The window must be gone before its class can be unregistered.
  if (hwnd_) {
    EXPECT_TRUE(DestroyWindow(hwnd_));
  }
  if (registered_) {
    EXPECT_TRUE(UnregisterClass(class_name_.c_str(), instance_));
  }
}

HWND TestWindowOfClass::Create(DWORD style, DWORD ex_style) {
  WNDCLASSEX wcex = {sizeof(wcex)};
  wcex.lpfnWndProc = DefWindowProc;
  wcex.hInstance = instance_;
  wcex.lpszClassName = class_name_.c_str();
  registered_ = RegisterClassEx(&wcex) != 0;
  if (!registered_) {
    return nullptr;
  }
  hwnd_ =
      CreateWindowEx(ex_style, class_name_.c_str(), class_name_.c_str(), style,
                     0, 0, 100, 100, nullptr, nullptr, instance_, nullptr);
  if (hwnd_) {
    ShowWindow(hwnd_, SW_SHOWNORMAL);
  }
  return hwnd_;
}

// This class currently tests the behavior of
// IsWindowVisibleAndFullyOpaque with hwnds
// with various attributes (e.g., minimized, transparent, etc).
class WindowVisibleAndFullyOpaqueTest : public testing::Test {
 public:
  WindowVisibleAndFullyOpaqueTest() {}

  WindowVisibleAndFullyOpaqueTest(const WindowVisibleAndFullyOpaqueTest&) =
      delete;
  WindowVisibleAndFullyOpaqueTest& operator=(
      const WindowVisibleAndFullyOpaqueTest&) = delete;

  TestNativeWindow* native_win() { return native_win_.get(); }

  HWND CreateNativeWindow(DWORD style, DWORD ex_style) {
    native_win_ = std::make_unique<TestNativeWindow>();
    native_win_->set_window_style(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN |
                                  style);
    native_win_->set_window_ex_style(ex_style);
    Rect bounds(0, 0, 100, 100);
    native_win_->Init(nullptr, bounds);
    HWND hwnd = native_win_->hwnd();
    base::win::ScopedGDIObject<HRGN> region(CreateRectRgn(0, 0, 0, 0));
    if (GetWindowRgn(hwnd, region.get()) == COMPLEXREGION) {
      // If the newly created window has a complex region by default, e.g.,
      // if it has the WS_EX_LAYERED style, it will be ignored during the
      // occlusion calculation. So, force it to have a simple region so that
      // we get test coverage for the window.
      RECT bounding_rect;
      EXPECT_TRUE(GetWindowRect(hwnd, &bounding_rect));
      base::win::ScopedGDIObject<HRGN> rectangular_region(
          CreateRectRgnIndirect(&bounding_rect));
      SetWindowRgn(hwnd, rectangular_region.get(), /*redraw=*/TRUE);
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    EXPECT_TRUE(UpdateWindow(hwnd));
    return hwnd;
  }

  bool CheckWindowVisibleAndFullyOpaque(HWND hwnd, Rect* win_rect) {
    bool ret = IsWindowVisibleAndFullyOpaque(hwnd, win_rect);
    // In general, if IsWindowVisibleAndFullyOpaque returns false, the
    // returned rect should not be altered.
    if (!ret) {
      EXPECT_EQ(*win_rect, Rect(0, 0, 0, 0));
    }
    return ret;
  }

 private:
  std::unique_ptr<TestNativeWindow> native_win_;
};

TEST_F(WindowVisibleAndFullyOpaqueTest, VisibleOpaqueWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, /*ex_style=*/0);
  Rect returned_rect;
  // Normal windows should be visible.
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &returned_rect));

  // Check that the returned rect == the actual window rect of the hwnd.
  RECT win_rect;
  ASSERT_TRUE(GetWindowRect(hwnd, &win_rect));
  EXPECT_EQ(returned_rect, Rect(win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, MinimizedWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, /*ex_style=*/0);
  Rect win_rect;
  ShowWindow(hwnd, SW_MINIMIZE);
  // Minimized windows are not considered visible.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, TransparentWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, WS_EX_TRANSPARENT);
  Rect win_rect;
  // Transparent windows are not considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, ToolWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, WS_EX_TOOLWINDOW);
  Rect win_rect;
  // Tool windows are not considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, LayeredAlphaWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, WS_EX_LAYERED);
  Rect win_rect;
  BYTE alpha = 1;
  DWORD flags = LWA_ALPHA;
  COLORREF color_ref = RGB(1, 1, 1);
  SetLayeredWindowAttributes(hwnd, color_ref, alpha, flags);
  // Layered windows with alpha < 255 are not considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, UpdatedLayeredAlphaWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, WS_EX_LAYERED);
  Rect win_rect;
  base::win::ScopedCreateDC hdc(::CreateCompatibleDC(nullptr));
  BLENDFUNCTION blend = {AC_SRC_OVER, 0x00, 0xFF, AC_SRC_ALPHA};

  ::UpdateLayeredWindow(hwnd, hdc.Get(), nullptr, nullptr, nullptr, nullptr,
                        RGB(0xFF, 0xFF, 0xFF), &blend, ULW_OPAQUE);
  // Layered windows set up with UpdateLayeredWindow instead of
  // SetLayeredWindowAttributes should not be considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, LayeredNonAlphaWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, WS_EX_LAYERED);
  Rect win_rect;
  BYTE alpha = 1;
  DWORD flags = 0;
  COLORREF color_ref = RGB(1, 1, 1);
  SetLayeredWindowAttributes(hwnd, color_ref, alpha, flags);
  // Layered non alpha windows are considered visible and opaque.
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, ComplexRegionWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, /*ex_style=*/0);
  Rect win_rect;
  // Create a region with rounded corners, which should be a complex region.
  base::win::ScopedGDIObject<HRGN> region(
      CreateRoundRectRgn(1, 1, 100, 100, 5, 5));
  SetWindowRgn(hwnd, region.get(), /*redraw=*/TRUE);
  // Windows with complex regions are not considered visible and fully opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, PopupChromeWindow) {
  HWND hwnd = CreateNativeWindow(WS_POPUP, /*ex_style=*/0);
  Rect win_rect;
  // Chrome Popup Windows of class Chrome_WidgetWin_ are considered visible.
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, PopupWindow) {
  TestWin32Window test_window;
  HWND hwnd = test_window.Create(WS_POPUPWINDOW);
  Rect win_rect;
  // Normal Popup Windows are not considered visible.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, CloakedWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, /*ex_style=*/0);
  Rect win_rect;
  BOOL cloak = TRUE;
  DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
  // Cloaked Windows are not considered visible.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, SimpleRegionWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, /*ex_style=*/0);
  Rect win_rect;
  base::win::ScopedGDIObject<HRGN> region(CreateRectRgn(0, 0, 50, 50));
  ASSERT_TRUE(region.is_valid());
  ASSERT_TRUE(SetWindowRgn(hwnd, region.get(), /*redraw=*/TRUE));
  // The system owns the region once SetWindowRgn() succeeds.
  std::ignore = region.release();
  RECT region_box;
  ASSERT_EQ(GetWindowRgnBox(hwnd, &region_box), SIMPLEREGION);
  // A rectangular region is a simple region, so the window still counts.
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(WindowVisibleAndFullyOpaqueTest, MaximizedWindow) {
  HWND hwnd = CreateNativeWindow(/*style=*/0, /*ex_style=*/0);
  ShowWindow(hwnd, SW_MAXIMIZE);
  ASSERT_TRUE(IsZoomed(hwnd));
  Rect returned_rect;
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &returned_rect));

  // The frame of a maximized window extends past the monitor's work area, so
  // the returned rect is the window rect fitted to the work area.
  RECT win_rect;
  ASSERT_TRUE(GetWindowRect(hwnd, &win_rect));
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  ASSERT_TRUE(GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                             &monitor_info));
  Rect expected_rect(win_rect);
  expected_rect.AdjustToFit(Rect(monitor_info.rcWork));
  EXPECT_EQ(returned_rect, expected_rect);
}

// The Windows taskbar is a popup tool window. Its class name exempts it from
// both the tool window and the popup window filters.
TEST_F(WindowVisibleAndFullyOpaqueTest, TaskbarWindow) {
  TestWindowOfClass test_window(L"Shell_TrayWnd");
  HWND hwnd = test_window.Create(WS_POPUP, WS_EX_TOOLWINDOW);
  ASSERT_TRUE(hwnd);
  Rect win_rect;
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

// A popup window whose class name starts with "Chrome_WidgetWin_" is exempt
// from the popup window filter, even when the class name has the maximum
// length of 255 characters. A name that long must not be mistaken for a
// truncated one.
TEST_F(WindowVisibleAndFullyOpaqueTest, PopupWindowWithLongestClassName) {
  std::wstring class_name = L"Chrome_WidgetWin_";
  class_name.resize(255, L'x');
  TestWindowOfClass test_window(class_name);
  HWND hwnd = test_window.Create(WS_POPUP, /*ex_style=*/0);
  ASSERT_TRUE(hwnd);
  Rect win_rect;
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

// Verifies that WindowImpl::WndProc forwards messages to DefWindowProc when
// GWLP_USERDATA is null (for example after ClearUserData() runs before
// DestroyWindow()).
TEST(WindowImplTest, WndProcCallsDefWindowProcWhenUserDataIsNull) {
  TestNativeWindow window;
  window.Init(nullptr, Rect(0, 0, 100, 100));
  ASSERT_TRUE(window.hwnd());

  // Null out GWLP_USERDATA to simulate ClearUserData().
  SetWindowLongPtr(window.hwnd(), GWLP_USERDATA, 0);

  // DefWindowProc always returns TRUE for WM_QUERYOPEN; if WndProc swallows the
  // message, this would be 0.
  LRESULT result = SendMessage(window.hwnd(), WM_QUERYOPEN, 0, 0);
  EXPECT_NE(result, 0)
      << "WndProc should forward to DefWindowProc when GWLP_USERDATA is "
         "null.";
}

namespace {

constexpr GUID kCurrentDesktopId = {
    0x11111111,
    0x2222,
    0x3333,
    {0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb}};
constexpr GUID kOtherDesktopId = {
    0x22222222,
    0x3333,
    0x4444,
    {0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc}};

// Models Explorer's per-window virtual desktop state: a newly registered window
// starts with GUID_NULL (which is not on the current desktop) until Explorer
// asynchronously assigns a concrete desktop GUID.
class FakeVirtualDesktopManager
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IVirtualDesktopManager> {
 public:
  FakeVirtualDesktopManager() = default;
  FakeVirtualDesktopManager(const FakeVirtualDesktopManager&) = delete;
  FakeVirtualDesktopManager& operator=(const FakeVirtualDesktopManager&) =
      delete;
  ~FakeVirtualDesktopManager() override = default;

  IFACEMETHODIMP IsWindowOnCurrentVirtualDesktop(
      HWND top_level_window,
      BOOL* on_current_desktop) override {
    if (!on_current_desktop) {
      return E_POINTER;
    }
    *on_current_desktop = FALSE;
    if (FAILED(is_on_current_desktop_error_)) {
      if (is_on_current_desktop_calls_before_failure_ == 0) {
        return is_on_current_desktop_error_;
      }
      --is_on_current_desktop_calls_before_failure_;
    }
    auto it = window_desktops_.find(top_level_window);
    if (it == window_desktops_.end()) {
      return TYPE_E_ELEMENTNOTFOUND;
    }
    *on_current_desktop =
        (it->second != GUID_NULL && it->second == current_desktop_id_) ? TRUE
                                                                       : FALSE;
    ApplyPendingAssignment();
    return S_OK;
  }

  IFACEMETHODIMP GetWindowDesktopId(HWND top_level_window,
                                    GUID* desktop_id) override {
    if (!desktop_id) {
      return E_POINTER;
    }
    *desktop_id = GUID_NULL;
    ++get_desktop_id_call_count_;
    if (FAILED(get_desktop_id_error_)) {
      return get_desktop_id_error_;
    }
    auto it = window_desktops_.find(top_level_window);
    if (it == window_desktops_.end()) {
      return TYPE_E_ELEMENTNOTFOUND;
    }
    *desktop_id = it->second;
    return S_OK;
  }

  IFACEMETHODIMP MoveWindowToDesktop(HWND top_level_window,
                                     REFGUID desktop_id) override {
    return E_NOTIMPL;
  }

  int get_desktop_id_call_count() const { return get_desktop_id_call_count_; }

  void SetWindowDesktopId(HWND window, const GUID& desktop_id) {
    window_desktops_[window] = desktop_id;
  }

  // Registers `window` with GUID_NULL initially, then assigns `desktop_id`
  // after the first IsWindowOnCurrentVirtualDesktop() call completes.
  void QueueAsyncDesktopAssignment(HWND window, const GUID& desktop_id) {
    window_desktops_[window] = GUID_NULL;
    pending_assignment_ = std::make_pair(window, desktop_id);
  }

  void FailIsWindowOnCurrentVirtualDesktop(HRESULT hr,
                                           int calls_before_failure = 0) {
    is_on_current_desktop_error_ = hr;
    is_on_current_desktop_calls_before_failure_ = calls_before_failure;
  }

  void FailGetWindowDesktopId(HRESULT hr) { get_desktop_id_error_ = hr; }

 private:
  void ApplyPendingAssignment() {
    if (pending_assignment_) {
      window_desktops_[pending_assignment_->first] =
          pending_assignment_->second;
      pending_assignment_.reset();
    }
  }

  GUID current_desktop_id_ = kCurrentDesktopId;
  std::map<HWND, GUID> window_desktops_;
  std::optional<std::pair<HWND, GUID>> pending_assignment_;
  int get_desktop_id_call_count_ = 0;
  HRESULT is_on_current_desktop_error_ = S_OK;
  int is_on_current_desktop_calls_before_failure_ = 0;
  HRESULT get_desktop_id_error_ = S_OK;
};

class IsWindowOnCurrentVirtualDesktopTest : public testing::Test {
 public:
  IsWindowOnCurrentVirtualDesktopTest() = default;
  IsWindowOnCurrentVirtualDesktopTest(
      const IsWindowOnCurrentVirtualDesktopTest&) = delete;
  IsWindowOnCurrentVirtualDesktopTest& operator=(
      const IsWindowOnCurrentVirtualDesktopTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(com_initializer_.Succeeded());
    window_.Init(nullptr, Rect(0, 0, 100, 100));
    ASSERT_TRUE(::IsWindow(window_.hwnd()));
    vdm_ = Microsoft::WRL::Make<FakeVirtualDesktopManager>();
  }

  HWND hwnd() const { return window_.hwnd(); }

  base::win::ScopedCOMInitializer com_initializer_;
  TestNativeWindow window_;
  Microsoft::WRL::ComPtr<FakeVirtualDesktopManager> vdm_;
};

}  // namespace

TEST_F(IsWindowOnCurrentVirtualDesktopTest, OnCurrentDesktop) {
  vdm_->SetWindowDesktopId(hwnd(), kCurrentDesktopId);

  EXPECT_EQ(IsWindowOnCurrentVirtualDesktop(hwnd(), vdm_), true);
  // Fast path should not make a second COM call to query the desktop GUID.
  EXPECT_EQ(vdm_->get_desktop_id_call_count(), 0);
}

TEST_F(IsWindowOnCurrentVirtualDesktopTest, OffCurrentDesktop) {
  vdm_->SetWindowDesktopId(hwnd(), kOtherDesktopId);

  EXPECT_EQ(IsWindowOnCurrentVirtualDesktop(hwnd(), vdm_), false);
}

TEST_F(IsWindowOnCurrentVirtualDesktopTest, GuidNullTreatedAsOnCurrentDesktop) {
  vdm_->SetWindowDesktopId(hwnd(), GUID_NULL);

  EXPECT_EQ(IsWindowOnCurrentVirtualDesktop(hwnd(), vdm_), true);
}

TEST_F(IsWindowOnCurrentVirtualDesktopTest,
       AssignedToCurrentDesktopDuringCheck) {
  vdm_->QueueAsyncDesktopAssignment(hwnd(), kCurrentDesktopId);

  EXPECT_EQ(IsWindowOnCurrentVirtualDesktop(hwnd(), vdm_), true);
}

TEST_F(IsWindowOnCurrentVirtualDesktopTest, AssignedToOtherDesktopDuringCheck) {
  vdm_->QueueAsyncDesktopAssignment(hwnd(), kOtherDesktopId);

  EXPECT_EQ(IsWindowOnCurrentVirtualDesktop(hwnd(), vdm_), false);
}

TEST_F(IsWindowOnCurrentVirtualDesktopTest, InitialCheckFailureReturnsNullopt) {
  // Untracked window fails IsWindowOnCurrentVirtualDesktop() with
  // TYPE_E_ELEMENTNOTFOUND.
  EXPECT_EQ(IsWindowOnCurrentVirtualDesktop(hwnd(), vdm_), std::nullopt);
}

TEST_F(IsWindowOnCurrentVirtualDesktopTest,
       GetWindowDesktopIdFailureReturnsNullopt) {
  vdm_->SetWindowDesktopId(hwnd(), GUID_NULL);
  vdm_->FailGetWindowDesktopId(TYPE_E_ELEMENTNOTFOUND);

  EXPECT_EQ(IsWindowOnCurrentVirtualDesktop(hwnd(), vdm_), std::nullopt);
}

TEST_F(IsWindowOnCurrentVirtualDesktopTest, RecheckFailureReturnsNullopt) {
  vdm_->QueueAsyncDesktopAssignment(hwnd(), kCurrentDesktopId);
  vdm_->FailIsWindowOnCurrentVirtualDesktop(RPC_E_DISCONNECTED,
                                            /*calls_before_failure=*/1);

  EXPECT_EQ(IsWindowOnCurrentVirtualDesktop(hwnd(), vdm_), std::nullopt);
}

}  // namespace gfx
