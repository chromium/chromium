// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/win/win_window.h"

#include <windows.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_util_win.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/win/win_cursor.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/win/msg_util.h"

namespace ui {

namespace {

bool use_popup_as_root_window_for_test = false;

gfx::Rect GetWindowBoundsForClientBounds(DWORD style,
                                         DWORD ex_style,
                                         const gfx::Rect& bounds) {
  RECT wr;
  wr.left = bounds.x();
  wr.top = bounds.y();
  wr.right = bounds.x() + bounds.width();
  wr.bottom = bounds.y() + bounds.height();
  AdjustWindowRectEx(&wr, style, FALSE, ex_style);

  // Make sure to keep the window onscreen, as AdjustWindowRectEx() may have
  // moved part of it offscreen.
  gfx::Rect window_bounds(wr.left, wr.top, wr.right - wr.left,
                          wr.bottom - wr.top);
  window_bounds.set_x(std::max(0, window_bounds.x()));
  window_bounds.set_y(std::max(0, window_bounds.y()));
  return window_bounds;
}

}  // namespace

WinWindow::WinWindow(PlatformWindowDelegate* delegate, const gfx::Rect& bounds)
    : delegate_(delegate), input_method_(nullptr) {
  CHECK(delegate_);
  DWORD window_style = WS_OVERLAPPEDWINDOW;
  if (use_popup_as_root_window_for_test) {
    set_window_style(WS_POPUP);
    window_style = WS_POPUP;
  }
  gfx::Rect window_bounds =
      GetWindowBoundsForClientBounds(window_style, window_ex_style(), bounds);
  gfx::WindowImpl::Init(NULL, window_bounds);
  SetWindowText(hwnd(), L"WinWindow");
}

WinWindow::~WinWindow() {
  SetInputMethod(nullptr);
}

void WinWindow::SetInputMethod(InputMethod* input_method) {
  if (input_method_) {
    input_method_->RemoveObserver(this);
  }

  input_method_ = input_method;
  if (input_method_) {
    input_method_->AddObserver(this);
  }
}

void WinWindow::Destroy() {
  if (IsWindow(hwnd())) {
    DestroyWindow(hwnd());
  }
}

void WinWindow::OnInputMethodDestroyed(const InputMethod* input_method) {
  CHECK_EQ(input_method_, input_method);
  input_method_ = nullptr;
}

void WinWindow::OnFocus() {}

void WinWindow::OnBlur() {}

void WinWindow::OnCaretBoundsChanged(const TextInputClient* client) {}

void WinWindow::OnTextInputStateChanged(const TextInputClient* client) {}

void WinWindow::Show(bool inactive) {
  ShowWindow(hwnd(), inactive ? SW_SHOWNOACTIVATE : SW_SHOWNORMAL);
}

void WinWindow::Hide() {
  ShowWindow(hwnd(), SW_HIDE);
}

void WinWindow::Close() {
  Destroy();
}

bool WinWindow::IsVisible() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

void WinWindow::PrepareForShutdown() {}

void WinWindow::SetBoundsInPixels(const gfx::Rect& bounds) {
  gfx::Rect window_bounds = GetWindowBoundsForClientBounds(
      GetWindowLong(hwnd(), GWL_STYLE), GetWindowLong(hwnd(), GWL_EXSTYLE),
      bounds);
  unsigned int flags = SWP_NOREPOSITION;
  if (!::IsWindowVisible(hwnd())) {
    flags |= SWP_NOACTIVATE;
  }
  SetWindowPos(hwnd(), NULL, window_bounds.x(), window_bounds.y(),
               window_bounds.width(), window_bounds.height(), flags);
}

gfx::Rect WinWindow::GetBoundsInPixels() const {
  RECT cr;
  GetClientRect(hwnd(), &cr);
  return gfx::Rect(cr);
}

void WinWindow::SetBoundsInDIP(const gfx::Rect& bounds) {
  // SetBounds should not be used on Windows tests.
  NOTREACHED();
}
gfx::Rect WinWindow::GetBoundsInDIP() const {
  // GetBounds should not be used on Windows tests.
  NOTREACHED();
}

void WinWindow::SetTitle(const std::u16string& title) {
  SetWindowText(hwnd(), base::as_wcstr(title));
}

void WinWindow::SetCapture() {
  if (!HasCapture()) {
    ::SetCapture(hwnd());
  }
}

void WinWindow::ReleaseCapture() {
  if (HasCapture()) {
    ::ReleaseCapture();
  }
}

bool WinWindow::HasCapture() const {
  return ::GetCapture() == hwnd();
}

void WinWindow::SetFullscreen(bool fullscreen, int64_t target_display_id) {}

void WinWindow::Maximize() {}

void WinWindow::Minimize() {}

void WinWindow::Restore() {}

PlatformWindowState WinWindow::GetPlatformWindowState() const {
  return PlatformWindowState::kUnknown;
}

void WinWindow::Activate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WinWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WinWindow::SetUseNativeFrame(bool use_native_frame) {}

bool WinWindow::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void WinWindow::SetCursor(scoped_refptr<PlatformCursor> platform_cursor) {
  DCHECK(platform_cursor);

  auto cursor = WinCursor::FromPlatformCursor(platform_cursor);
  ::SetCursor(cursor->hcursor());

  // The new cursor needs to be stored last to avoid deleting the old cursor
  // while it's still in use.
  cursor_ = cursor;
}

void WinWindow::MoveCursorTo(const gfx::Point& location) {
  ::SetCursorPos(location.x(), location.y());
}

void WinWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {}

void WinWindow::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {}

gfx::Rect WinWindow::GetRestoredBoundsInDIP() const {
  return gfx::Rect();
}

bool WinWindow::ShouldWindowContentsBeTransparent() const {
  // The window contents need to be transparent when the titlebar area is drawn
  // by the DWM rather than Chrome, so that area can show through.  This
  // function does not describe the transparency of the whole window appearance,
  // but merely of the content Chrome draws, so even when the system titlebars
  // appear opaque, the content above them needs to be transparent, or they'll
  // be covered by a black (undrawn) region.
  return !IsFullscreen();
}

void WinWindow::SetZOrderLevel(ZOrderLevel order) {
  NOTIMPLEMENTED_LOG_ONCE();
}

ZOrderLevel WinWindow::GetZOrderLevel() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return ZOrderLevel::kNormal;
}

void WinWindow::StackAbove(gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WinWindow::StackAtTop() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WinWindow::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WinWindow::SetVisibilityChangedAnimationsEnabled(bool enabled) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WinWindow::SetShape(std::unique_ptr<ShapeRects> native_shape,
                         const gfx::Transform& transform) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WinWindow::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WinWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                               const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WinWindow::SizeConstraintsChanged() {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool WinWindow::IsAnimatingClosed() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool WinWindow::IsFullscreen() const {
  return GetPlatformWindowState() == PlatformWindowState::kFullScreen;
}

LRESULT WinWindow::OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param) {
  const CHROME_MSG msg = {hwnd(),
                          message,
                          w_param,
                          l_param,
                          static_cast<DWORD>(GetMessageTime()),
                          {CR_GET_X_LPARAM(l_param), CR_GET_Y_LPARAM(l_param)}};
  std::unique_ptr<Event> event = EventFromNative(msg);
  if (IsMouseEventFromTouch(message)) {
    event->SetFlags(event->flags() | EF_FROM_TOUCH);
  }
  if (!(event->flags() & ui::EF_IS_NON_CLIENT)) {
    delegate_->DispatchEvent(event.get());
  }
  SetMsgHandled(event->handled());
  return 0;
}

LRESULT WinWindow::OnCaptureChanged(UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param) {
  delegate_->OnLostCapture();
  return 0;
}

LRESULT WinWindow::OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param) {
  const CHROME_MSG msg = {hwnd(), message, w_param, l_param};
  KeyEvent event(msg);
  delegate_->DispatchEvent(&event);
  SetMsgHandled(event.handled());
  return 0;
}

void WinWindow::OnInputLangChange(DWORD character_set, HKL input_language_id) {
  if (input_method_) {
    input_method_->OnInputLocaleChanged();
  }
}

LRESULT WinWindow::OnImeMessages(UINT message, WPARAM w_param, LPARAM l_param) {
  if (input_method_) {
    LRESULT result = 0;
    base::WeakPtr<WinWindow> ref(msg_handler_weak_factory_.GetWeakPtr());
    const CHROME_MSG msg = {hwnd(), message, w_param, l_param};
    const bool msg_handled =
        input_method_->OnUntranslatedIMEMessage(msg, &result);
    if (ref.get()) {
      SetMsgHandled(msg_handled);
    }
    return result;
  }

  return ::DefWindowProc(hwnd(), message, w_param, l_param);
}

LRESULT WinWindow::OnNCActivate(UINT message, WPARAM w_param, LPARAM l_param) {
  delegate_->OnActivationChanged(!!w_param);
  return ::DefWindowProc(hwnd(), message, w_param, l_param);
}

void WinWindow::OnClose() {
  delegate_->OnCloseRequest();
}

LRESULT WinWindow::OnCreate(CREATESTRUCT* create_struct) {
  delegate_->OnAcceleratedWidgetAvailable(hwnd());
  return 0;
}

void WinWindow::OnDestroy() {
  delegate_->OnClosed();
}

void WinWindow::OnPaint(HDC) {
  gfx::Rect damage_rect;
  RECT update_rect = {0};
  if (GetUpdateRect(hwnd(), &update_rect, FALSE)) {
    damage_rect = gfx::Rect(update_rect);
  }
  delegate_->OnDamageRect(damage_rect);
  ValidateRect(hwnd(), NULL);
}

void WinWindow::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (!(window_pos->flags & SWP_NOSIZE) || !(window_pos->flags & SWP_NOMOVE)) {
    RECT cr;
    GetClientRect(hwnd(), &cr);
    delegate_->OnBoundsChanged({true});
  }
}

LRESULT WinWindow::OnSetCursor(UINT message, WPARAM w_param, LPARAM l_param) {
  // `cursor_` must be a `ui::WinCursor`, so that custom image cursors are
  // properly ref-counted. `cursor` below is only used for system cursors and
  // doesn't replace the current cursor so an HCURSOR can be used directly.
  wchar_t* cursor = IDC_ARROW;
  // Reimplement the necessary default behavior here. Calling DefWindowProc can
  // trigger weird non-client painting for non-glass windows with custom frames.
  // Using a ScopedRedrawLock to prevent caption rendering artifacts may allow
  // content behind this window to incorrectly paint in front of this window.
  // Invalidating the window to paint over either set of artifacts is not ideal.
  switch (LOWORD(l_param)) {
    case HTSIZE:
      cursor = IDC_SIZENWSE;
      break;
    case HTLEFT:
    case HTRIGHT:
      cursor = IDC_SIZEWE;
      break;
    case HTTOP:
    case HTBOTTOM:
      cursor = IDC_SIZENS;
      break;
    case HTTOPLEFT:
    case HTBOTTOMRIGHT:
      cursor = IDC_SIZENWSE;
      break;
    case HTTOPRIGHT:
    case HTBOTTOMLEFT:
      cursor = IDC_SIZENESW;
      break;
    case HTCLIENT:
      if (cursor_) {
        SetCursor(cursor_);
        return 1;
      }
      break;
    case LOWORD(HTERROR):  // Use HTERROR's LOWORD value for valid comparison.
      SetMsgHandled(false);
      break;
    default:
      // Use the default value, IDC_ARROW.
      break;
  }
  ::SetCursor(::LoadCursor(nullptr, cursor));
  return 1;
}

namespace test {

// static
void SetUsePopupAsRootWindowForTest(bool use) {
  use_popup_as_root_window_for_test = use;
}

}  // namespace test
}  // namespace ui
