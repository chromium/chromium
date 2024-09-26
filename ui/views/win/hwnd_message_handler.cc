// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_message_handler.h"

#include <tchar.h>

#include <dwmapi.h>
#include <oleacc.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <utility>

#include "base/auto_reset.h"
#include "base/containers/heap_array.h"
#include "base/debug/gdi_debug_util_win.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util_win.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/dark_mode_support.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_window_handle_event_info.pbzero.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/ax_system_caret_win.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/hwnd_metrics.h"
#include "ui/base/win/internal_constants.h"
#include "ui/base/win/lock_state.h"
#include "ui/base/win/mouse_wheel_util.h"
#include "ui/base/win/session_change_observer.h"
#include "ui/base/win/touch_input.h"
#include "ui/base/win/win_cursor.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/types/event_type.h"
#include "ui/events/win/system_event_state_lookup.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/path_win.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/gfx/win/rendering_window_manager.h"
#include "ui/latency/latency_info.h"
#include "ui/native_theme/native_theme_win.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget_hwnd_utils.h"
#include "ui/views/win/fullscreen_handler.h"
#include "ui/views/win/hwnd_message_handler_delegate.h"
#include "ui/views/win/hwnd_message_handler_headless.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/views/win/pen_event_handler_util.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"

namespace views {
namespace {

// MoveLoopMouseWatcher is used to determine if the user canceled or completed a
// move. win32 doesn't appear to offer a way to determine the result of a move,
// so we install hooks to determine if we got a mouse up and assume the move
// completed.
class MoveLoopMouseWatcher {
 public:
  MoveLoopMouseWatcher(base::WeakPtr<HWNDMessageHandler> host,
                       bool hide_on_escape);

  MoveLoopMouseWatcher(const MoveLoopMouseWatcher&) = delete;
  MoveLoopMouseWatcher& operator=(const MoveLoopMouseWatcher&) = delete;

  ~MoveLoopMouseWatcher();

  // Unhooks the MoveLoopMouseWatcher instance associated with `host`, if any.
  static void UnhookForHost(HWNDMessageHandler* host);

  // Returns true if the mouse is up, or if we couldn't install the hook.
  bool got_mouse_up() const { return got_mouse_up_; }

 private:
  // Instance that owns the hook. We only allow one instance to hook the mouse
  // at a time.
  static MoveLoopMouseWatcher* instance_;

  // Key and mouse callbacks from the hook.
  static LRESULT CALLBACK MouseHook(int n_code, WPARAM w_param, LPARAM l_param);
  static LRESULT CALLBACK KeyHook(int n_code, WPARAM w_param, LPARAM l_param);

  void Unhook();

  // HWNDMessageHandler that created us.
  base::WeakPtr<HWNDMessageHandler> host_;

  // Should the window be hidden when escape is pressed?
  const bool hide_on_escape_;

  // Did we get a mouse up?
  bool got_mouse_up_ = false;

  // Hook identifiers.
  HHOOK mouse_hook_ = nullptr;
  HHOOK key_hook_ = nullptr;
};

// static
MoveLoopMouseWatcher* MoveLoopMouseWatcher::instance_ = nullptr;

MoveLoopMouseWatcher::MoveLoopMouseWatcher(
    base::WeakPtr<HWNDMessageHandler> host,
    bool hide_on_escape)
    : host_(std::move(host)), hide_on_escape_(hide_on_escape) {
  // Only one instance can be active at a time.
  if (instance_)
    instance_->Unhook();

  mouse_hook_ =
      SetWindowsHookEx(WH_MOUSE, &MouseHook, nullptr, GetCurrentThreadId());
  if (mouse_hook_) {
    instance_ = this;
    // We don't care if setting the key hook succeeded.
    key_hook_ =
        SetWindowsHookEx(WH_KEYBOARD, &KeyHook, nullptr, GetCurrentThreadId());
  }
  if (instance_ != this) {
    // Failed installation. Assume we got a mouse up in this case, otherwise
    // we'll think all drags were canceled.
    got_mouse_up_ = true;
  }
}

MoveLoopMouseWatcher::~MoveLoopMouseWatcher() {
  Unhook();
}

// static
void MoveLoopMouseWatcher::UnhookForHost(HWNDMessageHandler* host) {
  if (instance_ && instance_->host_.get() == host) {
    instance_->Unhook();
  }
}

void MoveLoopMouseWatcher::Unhook() {
  if (instance_ != this)
    return;

  DCHECK(mouse_hook_);
  UnhookWindowsHookEx(mouse_hook_);
  if (key_hook_)
    UnhookWindowsHookEx(key_hook_);
  key_hook_ = nullptr;
  mouse_hook_ = nullptr;
  instance_ = nullptr;
}

// static
LRESULT CALLBACK MoveLoopMouseWatcher::MouseHook(int n_code,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  DCHECK(instance_);
  if (n_code == HC_ACTION && w_param == WM_LBUTTONUP)
    instance_->got_mouse_up_ = true;
  return CallNextHookEx(instance_->mouse_hook_, n_code, w_param, l_param);
}

// static
LRESULT CALLBACK MoveLoopMouseWatcher::KeyHook(int n_code,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  if (n_code == HC_ACTION && w_param == VK_ESCAPE) {
    int value = TRUE;
    DwmSetWindowAttribute(instance_->host_->hwnd(),
                          DWMWA_TRANSITIONS_FORCEDISABLED, &value,
                          sizeof(value));
    if (instance_->hide_on_escape_)
      instance_->host_->Hide();
  }
  return CallNextHookEx(instance_->key_hook_, n_code, w_param, l_param);
}

// Called from OnNCActivate.
BOOL CALLBACK EnumChildWindowsForRedraw(HWND hwnd, LPARAM lparam) {
  DWORD process_id;
  GetWindowThreadProcessId(hwnd, &process_id);
  UINT flags = RDW_INVALIDATE | RDW_NOCHILDREN | RDW_FRAME;
  if (process_id == GetCurrentProcessId())
    flags |= RDW_UPDATENOW;
  RedrawWindow(hwnd, nullptr, nullptr, flags);
  return TRUE;
}

// Enables or disables the menu item for the specified command and menu.
void EnableMenuItemByCommand(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

// The thickness of an auto-hide taskbar in pixels.
constexpr int kAutoHideTaskbarThicknessPx = 2;

ui::EventType GetTouchEventType(POINTER_FLAGS pointer_flags) {
  if (pointer_flags & POINTER_FLAG_DOWN)
    return ui::EventType::kTouchPressed;
  if (pointer_flags & POINTER_FLAG_UPDATE)
    return ui::EventType::kTouchMoved;
  if (pointer_flags & POINTER_FLAG_UP)
    return ui::EventType::kTouchReleased;
  return ui::EventType::kTouchMoved;
}

bool IsHitTestOnResizeHandle(LRESULT hittest) {
  return hittest == HTRIGHT || hittest == HTLEFT || hittest == HTTOP ||
         hittest == HTBOTTOM || hittest == HTTOPLEFT || hittest == HTTOPRIGHT ||
         hittest == HTBOTTOMLEFT || hittest == HTBOTTOMRIGHT;
}

// Convert |param| to the gfx::ResizeEdge used in gfx::SizeRectToAspectRatio().
gfx::ResizeEdge GetWindowResizeEdge(UINT param) {
  switch (param) {
    case WMSZ_BOTTOM:
      return gfx::ResizeEdge::kBottom;
    case WMSZ_TOP:
      return gfx::ResizeEdge::kTop;
    case WMSZ_LEFT:
      return gfx::ResizeEdge::kLeft;
    case WMSZ_RIGHT:
      return gfx::ResizeEdge::kRight;
    case WMSZ_TOPLEFT:
      return gfx::ResizeEdge::kTopLeft;
    case WMSZ_TOPRIGHT:
      return gfx::ResizeEdge::kTopRight;
    case WMSZ_BOTTOMLEFT:
      return gfx::ResizeEdge::kBottomLeft;
    case WMSZ_BOTTOMRIGHT:
      return gfx::ResizeEdge::kBottomRight;
    default:
      NOTREACHED();
  }
}

int GetFlagsFromRawInputMessage(RAWINPUT* input) {
  int flags = ui::EF_NONE;
  if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
    flags |= ui::EF_BACK_MOUSE_BUTTON;
  if (input->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
    flags |= ui::EF_FORWARD_MOUSE_BUTTON;

  return ui::GetModifiersFromKeyState() | flags;
}

constexpr auto kTouchDownContextResetTimeout = base::Milliseconds(500);

// Windows does not flag synthesized mouse messages from touch or pen in all
// cases. This causes us grief as we don't want to process touch and mouse
// messages concurrently. Hack as per msdn is to check if the time difference
// between the touch/pen message and the mouse move is within 500 ms and at the
// same location as the cursor.
constexpr int kSynthesizedMouseMessagesTimeDifference = 500;

// Returns true if the window is arranged via Snap. For example, the browser
// window is snapped via buttons shown when the mouse is hovered over window
// maximize button.
bool IsWindowArranged(HWND window) {
  // IsWindowArranged() is not a part of any header file.
  // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindowarranged
  using IsWindowArrangedFuncType = BOOL(WINAPI*)(HWND);
  static const auto is_window_arranged_func =
      reinterpret_cast<IsWindowArrangedFuncType>(
          base::win::GetUser32FunctionPointer("IsWindowArranged"));
  return is_window_arranged_func ? is_window_arranged_func(window) : false;
}

}  // namespace

// A scoping class that prevents a window from being able to redraw in response
// to invalidations that may occur within it for the lifetime of the object.
//
// Why would we want such a thing? Well, it turns out Windows has some
// "unorthodox" behavior when it comes to painting its non-client areas.
// Occasionally, Windows will paint portions of the default non-client area
// right over the top of the custom frame. This is not simply fixed by handling
// WM_NCPAINT/WM_PAINT, with some investigation it turns out that this
// rendering is being done *inside* the default implementation of some message
// handlers and functions:
//  . WM_SETTEXT
//  . WM_SETICON
//  . WM_NCLBUTTONDOWN
//  . EnableMenuItem, called from our WM_INITMENU handler
// The solution is to handle these messages and call DefWindowProc ourselves,
// but prevent the window from being able to update itself for the duration of
// the call. We do this with this class, which automatically calls its
// associated Window's lock and unlock functions as it is created and destroyed.
// See documentation in those methods for the technique used.
//
// The lock only has an effect if the window was visible upon lock creation, as
// it doesn't guard against direct visiblility changes, and multiple locks may
// exist simultaneously to handle certain nested Windows messages.
//
// This lock is disabled when DirectComposition is used and there's a child
// rendering window, as the WS_CLIPCHILDREN on the parent window should
// prevent the glitched rendering and making the window contents non-visible
// can cause a them to disappear for a frame.
//
// We normally skip locked updates when Aero is on for two reasons:
// 1. Because it isn't necessary. However, for windows without WS_CAPTION a
//    close button may still be drawn, so the resize lock remains enabled for
//    them. See http://crrev.com/130323
// 2. Because toggling the WS_VISIBLE flag may occur while the GPU process is
//    attempting to present a child window's backbuffer onscreen. When these
//    two actions race with one another, the child window will either flicker
//    or will simply stop updating entirely.
//
// IMPORTANT: Do not use this scoping object for large scopes or periods of
//            time! IT WILL PREVENT THE WINDOW FROM BEING REDRAWN! (duh).
//
// I would love to hear Raymond Chen's explanation for all this. And maybe a
// list of other messages that this applies to ;-)
class HWNDMessageHandler::ScopedRedrawLock {
 public:
  explicit ScopedRedrawLock(HWNDMessageHandler* owner)
      : owner_(owner),
        hwnd_(owner_->hwnd()),
        should_lock_(owner_->IsVisible() && !owner->HasChildRenderingWindow() &&
                     ::IsWindow(hwnd_) && !owner_->IsHeadless() &&
                     (!(GetWindowLong(hwnd_, GWL_STYLE) & WS_CAPTION))) {
    if (should_lock_)
      owner_->LockUpdates();
  }

  ScopedRedrawLock(const ScopedRedrawLock&) = delete;
  ScopedRedrawLock& operator=(const ScopedRedrawLock&) = delete;

  ~ScopedRedrawLock() {
    if (!cancel_unlock_ && should_lock_ && ::IsWindow(hwnd_))
      owner_->UnlockUpdates();
  }

  // Cancel the unlock operation, call this if the Widget is being destroyed.
  void CancelUnlockOperation() { cancel_unlock_ = true; }

 private:
  // The owner having its style changed.
  raw_ptr<HWNDMessageHandler> owner_;
  // The owner's HWND, cached to avoid action after window destruction.
  HWND hwnd_;
  // A flag indicating that the unlock operation was canceled.
  bool cancel_unlock_ = false;
  // If false, don't use redraw lock.
  const bool should_lock_;
};

// static HWNDMessageHandler member initialization.
base::LazyInstance<HWNDMessageHandler::FullscreenWindowMonitorMap>::
    DestructorAtExit HWNDMessageHandler::fullscreen_monitor_map_ =
        LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, public:

LONG HWNDMessageHandler::last_touch_or_pen_message_time_ = 0;
bool HWNDMessageHandler::is_pen_active_in_client_area_ = false;
bool HWNDMessageHandler::handle_pen_events_in_client_area_ = true;

// static
std::unique_ptr<HWNDMessageHandler> HWNDMessageHandler::Create(
    HWNDMessageHandlerDelegate* delegate,
    const std::string& debugging_id,
    bool headless_mode) {
  HWNDMessageHandler* message_handler =
      headless_mode ? new HWNDMessageHandlerHeadless(delegate, debugging_id)
                    : new HWNDMessageHandler(delegate, debugging_id);
  return base::WrapUnique(message_handler);
}

HWNDMessageHandler::HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate,
                                       const std::string& debugging_id)
    : WindowImpl(debugging_id),
      delegate_(delegate),
      fullscreen_handler_(new FullscreenHandler),
      waiting_for_close_now_(false),
      restored_enabled_(false),
      current_cursor_(base::MakeRefCounted<ui::WinCursor>()),
      dpi_(0),
      called_enable_non_client_dpi_scaling_(false),
      active_mouse_tracking_flags_(0),
      is_right_mouse_pressed_on_caption_(false),
      lock_updates_count_(0),
      ignore_window_pos_changes_(false),
      last_monitor_(nullptr),
      is_first_nccalc_(true),
      menu_depth_(0),
      id_generator_(0),
      pen_processor_(&id_generator_, true),
      touch_down_contexts_(0),
      last_mouse_hwheel_time_(0),
      dwm_transition_desired_(false),
      sent_window_size_changing_(false),
      did_return_uia_object_(false),
      left_button_down_on_caption_(false),
      background_fullscreen_hack_(false),
      pointer_events_for_touch_(::features::IsUsingWMPointerForTouch()) {}

HWNDMessageHandler::~HWNDMessageHandler() {
  // Unhook MoveLoopMouseWatcher, to prevent call backs to this after deletion.
  MoveLoopMouseWatcher::UnhookForHost(this);

  // Clear pointer to this in `hwnd()`'s user data, to prevent installed hooks
  // from calling back into this after deletion.
  ClearUserData();
}

void HWNDMessageHandler::Init(HWND parent, const gfx::Rect& bounds) {
  TRACE_EVENT0("views", "HWNDMessageHandler::Init");
  GetMonitorAndRects(bounds.ToRECT(), &last_monitor_, &last_monitor_rect_,
                     &last_work_area_);

  initial_bounds_valid_ = !bounds.IsEmpty();

  // Create the platform window.
  WindowImpl::Init(parent, bounds);

  InitExtras();
}

void HWNDMessageHandler::InitModalType(ui::mojom::ModalType modal_type) {
  if (modal_type == ui::mojom::ModalType::kNone) {
    return;
  }
  // We implement modality by crawling up the hierarchy of windows starting
  // at the owner, disabling all of them so that they don't receive input
  // messages.
  HWND start = ::GetWindow(hwnd(), GW_OWNER);
  while (start) {
    ::EnableWindow(start, FALSE);
    start = ::GetParent(start);
  }
}

void HWNDMessageHandler::Close() {
  if (!IsWindow(hwnd()))
    return;  // No need to do anything.

  // Let's hide ourselves right away.
  Hide();

  // Modal dialog windows disable their owner windows; re-enable them now so
  // they can activate as foreground windows upon this window's destruction.
  RestoreEnabledIfNecessary();

  // Re-enable flicks which removes the window property.
  base::win::EnableFlicks(hwnd());

  if (!waiting_for_close_now_) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    waiting_for_close_now_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&HWNDMessageHandler::CloseNow,
                                  msg_handler_weak_factory_.GetWeakPtr()));
  }
}

void HWNDMessageHandler::CloseNow() {
  // We may already have been destroyed if the selection resulted in a tab
  // switch which will have reactivated the browser window and closed us, so
  // we need to check to see if we're still a window before trying to destroy
  // ourself.
  waiting_for_close_now_ = false;
  if (IsWindow(hwnd()))
    DestroyWindow(hwnd());
}

gfx::Rect HWNDMessageHandler::GetWindowBoundsInScreen() const {
  RECT r;
  GetWindowRect(hwnd(), &r);
  return gfx::Rect(r);
}

gfx::Rect HWNDMessageHandler::GetClientAreaBoundsInScreen() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  POINT point = {r.left, r.top};
  ClientToScreen(hwnd(), &point);
  return gfx::Rect(point.x, point.y, r.right - r.left, r.bottom - r.top);
}

gfx::Rect HWNDMessageHandler::GetRestoredBounds() const {
  // If we're in fullscreen mode, we've changed the normal bounds to the monitor
  // rect, so return the saved bounds instead.
  if (IsFullscreen())
    return fullscreen_handler_->GetRestoreBounds();

  gfx::Rect bounds;
  GetWindowPlacement(&bounds, nullptr);
  return bounds;
}

gfx::Rect HWNDMessageHandler::GetClientAreaBounds() const {
  if (IsMinimized())
    return gfx::Rect();
  if (delegate_->WidgetSizeIsClientSize())
    return GetClientAreaBoundsInScreen();
  return GetWindowBoundsInScreen();
}

void HWNDMessageHandler::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  bool succeeded = !!::GetWindowPlacement(hwnd(), &wp);
  DCHECK(succeeded);

  if (bounds != nullptr) {
    if (wp.showCmd == SW_SHOWNORMAL) {
      // GetWindowPlacement can return misleading position if a normalized
      // window was resized using Aero Snap feature (see comment 9 in bug
      // 36421). As a workaround, using GetWindowRect for normalized windows.
      succeeded = GetWindowRect(hwnd(), &wp.rcNormalPosition) != 0;
      DCHECK(succeeded);

      *bounds = gfx::Rect(wp.rcNormalPosition);
    } else {
      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      succeeded =
          GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST),
                         &mi) != 0;
      DCHECK(succeeded);

      *bounds = gfx::Rect(wp.rcNormalPosition);
      // Convert normal position from workarea coordinates to screen
      // coordinates.
      bounds->Offset(mi.rcWork.left - mi.rcMonitor.left,
                     mi.rcWork.top - mi.rcMonitor.top);
    }
  }

  if (show_state) {
    if (wp.showCmd == SW_SHOWMAXIMIZED)
      *show_state = ui::mojom::WindowShowState::kMaximized;
    else if (wp.showCmd == SW_SHOWMINIMIZED)
      *show_state = ui::mojom::WindowShowState::kMinimized;
    else
      *show_state = ui::mojom::WindowShowState::kNormal;
  }
}

void HWNDMessageHandler::SetBounds(const gfx::Rect& bounds_in_pixels,
                                   bool force_size_changed) {
  background_fullscreen_hack_ = false;
  SetBoundsInternal(bounds_in_pixels, force_size_changed);
}

void HWNDMessageHandler::SetParentOrOwner(HWND new_parent) {
  HWND parent = GetParent(hwnd());
  HWND owner = GetWindow(hwnd(), GW_OWNER);
  // A hwnd cannot be a child window and an owned window at the same time.
  DCHECK(!(parent && owner));

  if (parent) {
    // This is a child window.
    // TODO(crbug.com/40284685): allows setting NULL parent since WinAPI permits
    // it. It will require updating window styles. See
    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent#remarks.
    DCHECK(new_parent);
    SetParent(hwnd(), new_parent);
  } else {
    // This is either an owned window or an un-owned window.
    SetWindowLongPtr(hwnd(), GWLP_HWNDPARENT,
                     reinterpret_cast<LONG_PTR>(new_parent));
  }
}

void HWNDMessageHandler::SetDwmFrameExtension(DwmFrameState state) {
  if (!delegate_->HasFrame() && !is_translucent_) {
    MARGINS m = {0, 0, 0, 0};
    if (state == DwmFrameState::kOn && !IsMaximized())
      m = {0, 0, 1, 0};
    DwmExtendFrameIntoClientArea(hwnd(), &m);
  }
}

void HWNDMessageHandler::SetSize(const gfx::Size& size) {
  SetWindowPos(hwnd(), nullptr, 0, 0, size.width(), size.height(),
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
}

void HWNDMessageHandler::CenterWindow(const gfx::Size& size) {
  HWND parent = GetParent(hwnd());
  if (!IsWindow(parent)) {
    parent = ::GetWindow(hwnd(), GW_OWNER);
  }
  gfx::CenterAndSizeWindow(parent, hwnd(), size);
}

void HWNDMessageHandler::SetRegion(HRGN region) {
  custom_window_region_.reset(region);
  ResetWindowRegion(true, true);
}

void HWNDMessageHandler::StackAbove(HWND other_hwnd) {
  // Windows API allows to stack behind another windows only.
  DCHECK(other_hwnd);
  HWND next_window = GetNextWindow(other_hwnd, GW_HWNDPREV);
  SetWindowPos(hwnd(), next_window ? next_window : HWND_TOP, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void HWNDMessageHandler::StackAtTop() {
  SetWindowPos(hwnd(), HWND_TOP, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void HWNDMessageHandler::Show(ui::mojom::WindowShowState show_state,
                              const gfx::Rect& pixel_restore_bounds) {
  TRACE_EVENT0("views", "HWNDMessageHandler::Show");

  int native_show_state;
  if (show_state == ui::mojom::WindowShowState::kMaximized &&
      !pixel_restore_bounds.IsEmpty()) {
    WINDOWPLACEMENT placement = {0};
    placement.length = sizeof(WINDOWPLACEMENT);
    placement.showCmd = SW_SHOWMAXIMIZED;
    placement.rcNormalPosition = pixel_restore_bounds.ToRECT();
    SetWindowPlacement(hwnd(), &placement);
    native_show_state = SW_SHOWMAXIMIZED;
  } else {
    const bool is_maximized = IsMaximized();

    // Use SW_SHOW/SW_SHOWNA instead of SW_SHOWNORMAL/SW_SHOWNOACTIVATE so that
    // the window is not restored to its original position if it is maximized.
    // This could be used unconditionally for
    // ui::mojom::WindowShowState::kInactive, but cross-platform behavior when
    // showing a minimized window is inconsistent, some platforms restore the
    // position, some do not. See crbug.com/1296710
    switch (show_state) {
      case ui::mojom::WindowShowState::kInactive:
        native_show_state = is_maximized ? SW_SHOWNA : SW_SHOWNOACTIVATE;
        break;
      case ui::mojom::WindowShowState::kMaximized:
        native_show_state = SW_SHOWMAXIMIZED;
        break;
      case ui::mojom::WindowShowState::kMinimized:
        native_show_state = SW_SHOWMINIMIZED;
        break;
      case ui::mojom::WindowShowState::kNormal:
        if ((GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_TRANSPARENT) ||
            (GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
          native_show_state = is_maximized ? SW_SHOWNA : SW_SHOWNOACTIVATE;
        } else {
          native_show_state = is_maximized ? SW_SHOW : SW_SHOWNORMAL;
        }
        break;
      case ui::mojom::WindowShowState::kFullscreen:
        native_show_state = SW_SHOWNORMAL;
        SetFullscreen(true, display::kInvalidDisplayId);
        break;
      default:
        native_show_state = delegate_->GetInitialShowState();
        break;
    }

    ShowWindow(hwnd(), native_show_state);
    // When launched from certain programs like bash and Windows Live
    // Messenger, show_state is set to SW_HIDE, so we need to correct that
    // condition. We don't just change show_state to SW_SHOWNORMAL because
    // MSDN says we must always first call ShowWindow with the specified
    // value from STARTUPINFO, otherwise all future ShowWindow calls will be
    // ignored (!!#@@#!). Instead, we call ShowWindow again in this case.
    if (native_show_state == SW_HIDE) {
      native_show_state = SW_SHOWNORMAL;
      ShowWindow(hwnd(), native_show_state);
    }
  }

  // We need to explicitly activate the window if we've been shown with a state
  // that should activate, because if we're opened from a desktop shortcut while
  // an existing window is already running it doesn't seem to be enough to use
  // one of these flags to activate the window.
  if (native_show_state == SW_SHOWNORMAL ||
      native_show_state == SW_SHOWMAXIMIZED)
    Activate();

  if (!delegate_->HandleInitialFocus(show_state))
    SetInitialFocus();
}

void HWNDMessageHandler::Hide() {
  if (IsWindow(hwnd())) {
    // NOTE: Be careful not to activate any windows here (for example, calling
    // ShowWindow(SW_HIDE) will automatically activate another window).  This
    // code can be called while a window is being deactivated, and activating
    // another window will screw up the activation that is already in progress.
    SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                     SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
  }
}

void HWNDMessageHandler::Maximize() {
  ExecuteSystemMenuCommand(SC_MAXIMIZE);
}

void HWNDMessageHandler::Minimize() {
  ExecuteSystemMenuCommand(SC_MINIMIZE);
  delegate_->HandleNativeBlur(nullptr);
}

void HWNDMessageHandler::Restore() {
  ExecuteSystemMenuCommand(SC_RESTORE);
}

void HWNDMessageHandler::Activate() {
  if (IsMinimized()) {
    base::AutoReset<bool> restoring_activate(&notify_restore_on_activate_,
                                             true);
    ::ShowWindow(hwnd(), SW_RESTORE);
  }

  ::SetWindowPos(hwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  SetForegroundWindow(hwnd());
}

void HWNDMessageHandler::Deactivate() {
  HWND next_hwnd = ::GetNextWindow(hwnd(), GW_HWNDNEXT);
  while (next_hwnd) {
    if (::IsWindowVisible(next_hwnd)) {
      ::SetForegroundWindow(next_hwnd);
      return;
    }
    next_hwnd = ::GetNextWindow(next_hwnd, GW_HWNDNEXT);
  }
}

void HWNDMessageHandler::SetAlwaysOnTop(bool on_top) {
  ::SetWindowPos(hwnd(), on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool HWNDMessageHandler::IsVisible() const {
  return !!::IsWindowVisible(hwnd());
}

bool HWNDMessageHandler::IsActive() const {
  return ::GetActiveWindow() == hwnd();
}

bool HWNDMessageHandler::IsMinimized() const {
  return !!::IsIconic(hwnd());
}

bool HWNDMessageHandler::IsMaximized() const {
  return !!::IsZoomed(hwnd()) && !IsFullscreen();
}

bool HWNDMessageHandler::IsFullscreen() const {
  return fullscreen_handler_->fullscreen();
}

bool HWNDMessageHandler::IsAlwaysOnTop() const {
  return (GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

bool HWNDMessageHandler::IsHeadless() const {
  return false;
}

bool HWNDMessageHandler::RunMoveLoop(const gfx::Vector2d& drag_offset,
                                     bool hide_on_escape) {
  ReleaseCapture();
  MoveLoopMouseWatcher watcher(msg_handler_weak_factory_.GetWeakPtr(),
                               hide_on_escape);
  // In Aura, we handle touch events asynchronously. So we need to allow nested
  // tasks while in windows move loop.
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;

  SendMessage(hwnd(), WM_SYSCOMMAND, SC_MOVE | 0x0002,
              static_cast<LPARAM>(GetMessagePos()));
  // Windows doesn't appear to offer a way to determine whether the user
  // canceled the move or not. We assume if the user released the mouse it was
  // successful.
  return watcher.got_mouse_up();
}

void HWNDMessageHandler::EndMoveLoop() {
  SendMessage(hwnd(), WM_CANCELMODE, 0, 0);
}

void HWNDMessageHandler::SendFrameChanged() {
  SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE |
                   SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOSENDCHANGING |
                   SWP_NOSIZE | SWP_NOZORDER);
}

void HWNDMessageHandler::FlashFrame(bool flash) {
  FLASHWINFO fwi;
  fwi.cbSize = sizeof(fwi);
  fwi.hwnd = hwnd();
  if (flash) {
    fwi.dwFlags = custom_window_region_.is_valid() ? FLASHW_TRAY : FLASHW_ALL;
    fwi.uCount = 4;
    fwi.dwTimeout = 0;
  } else {
    fwi.dwFlags = FLASHW_STOP;
  }
  FlashWindowEx(&fwi);
}

void HWNDMessageHandler::ClearNativeFocus() {
    ::SetFocus(hwnd());
}

void HWNDMessageHandler::SetCapture() {
  // We may need to change this to !HasCapture() || release_capture_errno_ to
  // avoid checking when the call to `::ReleaseCapture` below fails.
  // Logging release_capture_errno_ will tell us if the DCHECK below is caused
  // by ::ReleaseCapture failing.
  DCHECK(!HasCapture()) << " release capture error = "
                        << logging::SystemErrorCodeToString(
                               release_capture_errno_);
  ::SetCapture(hwnd());
}

void HWNDMessageHandler::ReleaseCapture() {
  if (HasCapture() && !::ReleaseCapture())
    release_capture_errno_ = ::GetLastError();
  else
    release_capture_errno_ = 0;
}

bool HWNDMessageHandler::HasCapture() const {
  return ::GetCapture() == hwnd();
}

FullscreenHandler* HWNDMessageHandler::fullscreen_handler() {
  return fullscreen_handler_.get();
}

void HWNDMessageHandler::SetVisibilityChangedAnimationsEnabled(bool enabled) {
  int dwm_value = enabled ? FALSE : TRUE;
  DwmSetWindowAttribute(hwnd(), DWMWA_TRANSITIONS_FORCEDISABLED, &dwm_value,
                        sizeof(dwm_value));
}

bool HWNDMessageHandler::SetTitle(const std::u16string& title) {
  std::wstring current_title;
  auto len_with_null = static_cast<size_t>(GetWindowTextLength(hwnd())) + 1;
  if (len_with_null == 1 && title.length() == 0)
    return false;
  if (len_with_null - 1 == title.length() &&
      GetWindowText(hwnd(), base::WriteInto(&current_title, len_with_null),
                    len_with_null) &&
      current_title == base::AsWStringView(title)) {
    return false;
  }
  SetWindowText(hwnd(), base::as_wcstr(title));
  return true;
}

void HWNDMessageHandler::SetCursor(scoped_refptr<ui::WinCursor> cursor) {
  DCHECK(cursor);

  TRACE_EVENT1("ui,input", "HWNDMessageHandler::SetCursor", "cursor",
               static_cast<const void*>(cursor->hcursor()));
  ::SetCursor(cursor->hcursor());
  current_cursor_ = cursor;
}

void HWNDMessageHandler::FrameTypeChanged() {
  needs_dwm_frame_clear_ = true;
  if (!custom_window_region_.is_valid() && IsFrameSystemDrawn())
    dwm_transition_desired_ = true;
  if (!dwm_transition_desired_ || !IsFullscreen())
    PerformDwmTransition();
}

void HWNDMessageHandler::PaintAsActiveChanged() {
  if (!delegate_->HasNonClientView() || !delegate_->CanActivate() ||
      !delegate_->HasFrame() ||
      (delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN)) {
    return;
  }

  DefWindowProcWithRedrawLock(WM_NCACTIVATE, delegate_->ShouldPaintAsActive(),
                              0);
}

void HWNDMessageHandler::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                        const gfx::ImageSkia& app_icon) {
  if (!window_icon.isNull()) {
    base::win::ScopedHICON previous_icon = std::move(window_icon_);
    window_icon_ = IconUtil::CreateHICONFromSkBitmap(*window_icon.bitmap());
    SendMessage(hwnd(), WM_SETICON, ICON_SMALL,
                reinterpret_cast<LPARAM>(window_icon_.get()));
  }
  if (!app_icon.isNull()) {
    base::win::ScopedHICON previous_icon = std::move(app_icon_);
    app_icon_ = IconUtil::CreateHICONFromSkBitmap(*app_icon.bitmap());
    SendMessage(hwnd(), WM_SETICON, ICON_BIG,
                reinterpret_cast<LPARAM>(app_icon_.get()));
  }
}

void HWNDMessageHandler::SetFullscreen(bool fullscreen,
                                       int64_t target_display_id) {
  // Erase any prior reference to this window in the fullscreen window map.
  HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY);
  FullscreenWindowMonitorMap::iterator iter =
      fullscreen_monitor_map_.Get().find(monitor);
  if (iter != fullscreen_monitor_map_.Get().end())
    fullscreen_monitor_map_.Get().erase(iter);

  background_fullscreen_hack_ = false;
  auto ref = msg_handler_weak_factory_.GetWeakPtr();
  fullscreen_handler()->SetFullscreen(fullscreen, target_display_id);
  if (!ref)
    return;

  // Add an entry in the fullscreen window map if the window is now fullscreen.
  if (fullscreen) {
    HMONITOR new_monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY);
    (fullscreen_monitor_map_.Get())[new_monitor] = this;
  }
  // If we are out of fullscreen and there was a pending DWM transition for the
  // window, then go ahead and do it now.
  if (!fullscreen && dwm_transition_desired_)
    PerformDwmTransition();
}

void HWNDMessageHandler::SetAspectRatio(float aspect_ratio,
                                        const gfx::Size& excluded_margin) {
  // If the aspect ratio is not in the valid range, do nothing.
  DCHECK_GT(aspect_ratio, 0.0f);

  aspect_ratio_ = aspect_ratio;

  // Convert to pixels.
  excluded_margin_ =
      display::win::ScreenWin::DIPToScreenSize(hwnd(), excluded_margin);

  // When the aspect ratio is set, size the window to adhere to it. This keeps
  // the same origin point as the original window.
  RECT window_rect;
  if (GetWindowRect(hwnd(), &window_rect)) {
    gfx::Rect rect(window_rect);

    SizeWindowToAspectRatio(WMSZ_BOTTOMRIGHT, &rect);
    SetBoundsInternal(rect, false);
  }
}

void HWNDMessageHandler::SizeConstraintsChanged() {
  LONG style = GetWindowLong(hwnd(), GWL_STYLE);
  // Ignore if this is not a standard window.
  if (style & static_cast<LONG>(WS_POPUP | WS_CHILD))
    return;

  // Windows cannot have WS_THICKFRAME set if translucent.
  // See CalculateWindowStylesFromInitParams().
  if (delegate_->CanResize() && !is_translucent_) {
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    if (!delegate_->CanMaximize())
      style &= ~WS_MAXIMIZEBOX;
  } else {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
  if (delegate_->CanMinimize()) {
    style |= WS_MINIMIZEBOX;
  } else {
    style &= ~WS_MINIMIZEBOX;
  }
  SetWindowLong(hwnd(), GWL_STYLE, style);
}

// static
void HWNDMessageHandler::UseDefaultHandlerForPenEventsUntilPenUp() {
  handle_pen_events_in_client_area_ = false;
}

bool HWNDMessageHandler::HasChildRenderingWindow() {
  // This can change dynamically if the system switches between GPU and
  // software rendering.
  return gfx::RenderingWindowManager::GetInstance()->HasValidChildWindow(
      hwnd());
}

void HWNDMessageHandler::set_is_translucent(bool is_translucent) {
  is_translucent_ = is_translucent;
}

bool HWNDMessageHandler::is_translucent() const {
  return is_translucent_;
}

std::unique_ptr<aura::ScopedEnableUnadjustedMouseEvents>
HWNDMessageHandler::RegisterUnadjustedMouseEvent() {
  std::unique_ptr<ScopedEnableUnadjustedMouseEventsWin> scoped_enable =
      ScopedEnableUnadjustedMouseEventsWin::StartMonitor(this);

  DCHECK(using_wm_input_);
  return scoped_enable;
}

void HWNDMessageHandler::set_using_wm_input(bool using_wm_input) {
  using_wm_input_ = using_wm_input;
}

bool HWNDMessageHandler::using_wm_input() const {
  return using_wm_input_;
}

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, gfx::WindowImpl overrides:

HICON HWNDMessageHandler::GetDefaultWindowIcon() const {
  return ViewsDelegate::GetInstance()->GetDefaultWindowIcon();
}

HICON HWNDMessageHandler::GetSmallWindowIcon() const {
  return ViewsDelegate::GetInstance()->GetSmallWindowIcon();
}

LRESULT HWNDMessageHandler::OnWndProc(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  TRACE_EVENT("ui,toplevel", "HWNDMessageHandler::OnWndProc",
              [&](perfetto::EventContext ctx) {
                perfetto::protos::pbzero::ChromeWindowHandleEventInfo* args =
                    ctx.event()->set_chrome_window_handle_event_info();
                args->set_message_id(message);
                args->set_hwnd_ptr(
                    static_cast<uint64_t>(reinterpret_cast<uintptr_t>(hwnd())));
              });

  HWND window = hwnd();
  LRESULT result = 0;
  if (delegate_ && delegate_->PreHandleMSG(message, w_param, l_param, &result))
    return result;

  // Otherwise we handle everything else.
  // NOTE: We inline ProcessWindowMessage() as 'this' may be destroyed during
  // dispatch and ProcessWindowMessage() doesn't deal with that well.
  const BOOL old_msg_handled = msg_handled_;
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  const BOOL processed =
      _ProcessWindowMessage(window, message, w_param, l_param, result, 0);
  if (!ref)
    return 0;
  msg_handled_ = old_msg_handled;

  if (!processed) {
    result = DefWindowProc(window, message, w_param, l_param);
    // DefWindowProc() may have destroyed the window and/or us in a nested
    // message loop.
    if (!ref || !::IsWindow(window))
      return result;
  }

  if (delegate_) {
    delegate_->PostHandleMSG(message, w_param, l_param);
    if (message == WM_NCDESTROY) {
      RestoreEnabledIfNecessary();
      delegate_->HandleDestroyed();
    }
  }

  if (message == WM_ACTIVATE && IsTopLevelWindow(window)) {
    PostProcessActivateMessage(LOWORD(w_param), !!HIWORD(w_param),
                               reinterpret_cast<HWND>(l_param));
  }
  return result;
}

void HWNDMessageHandler::OnFocus() {}

void HWNDMessageHandler::OnBlur() {}

void HWNDMessageHandler::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  if (!ax_system_caret_)
    ax_system_caret_ = std::make_unique<ui::AXSystemCaretWin>(hwnd());

  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE) {
    ax_system_caret_->Hide();
    return;
  }

  const gfx::Rect dip_caret_bounds(client->GetCaretBounds());
  gfx::Rect caret_bounds =
      display::win::ScreenWin::DIPToScreenRect(hwnd(), dip_caret_bounds);
  // Collapse any selection.
  caret_bounds.set_width(1);
  ax_system_caret_->MoveCaretTo(caret_bounds);
}

void HWNDMessageHandler::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!client || client->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    OnCaretBoundsChanged(client);
}

void HWNDMessageHandler::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  DestroyAXSystemCaret();
}

LRESULT HWNDMessageHandler::HandleMouseMessage(unsigned int message,
                                               WPARAM w_param,
                                               LPARAM l_param,
                                               bool* handled) {
  // Don't track forwarded mouse messages. We expect the caller to track the
  // mouse.
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  LRESULT ret = HandleMouseEventInternal(message, w_param, l_param, false);
  *handled = !ref.get() || msg_handled_;
  return ret;
}

LRESULT HWNDMessageHandler::HandleKeyboardMessage(unsigned int message,
                                                  WPARAM w_param,
                                                  LPARAM l_param,
                                                  bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  LRESULT ret = 0;
  if ((message == WM_CHAR) || (message == WM_SYSCHAR))
    ret = OnImeMessages(message, w_param, l_param);
  else
    ret = OnKeyEvent(message, w_param, l_param);
  *handled = !ref.get() || msg_handled_;
  return ret;
}

LRESULT HWNDMessageHandler::HandleTouchMessage(unsigned int message,
                                               WPARAM w_param,
                                               LPARAM l_param,
                                               bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  LRESULT ret = OnTouchEvent(message, w_param, l_param);
  *handled = !ref.get() || msg_handled_;
  return ret;
}

LRESULT HWNDMessageHandler::HandlePointerMessage(unsigned int message,
                                                 WPARAM w_param,
                                                 LPARAM l_param,
                                                 bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  LRESULT ret = OnPointerEvent(message, w_param, l_param);
  *handled = !ref.get() || msg_handled_;
  return ret;
}

LRESULT HWNDMessageHandler::HandleInputMessage(unsigned int message,
                                               WPARAM w_param,
                                               LPARAM l_param,
                                               bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  LRESULT ret = OnInputEvent(message, w_param, l_param);
  *handled = !ref.get() || msg_handled_;
  return ret;
}

LRESULT HWNDMessageHandler::HandleScrollMessage(unsigned int message,
                                                WPARAM w_param,
                                                LPARAM l_param,
                                                bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  LRESULT ret = OnScrollMessage(message, w_param, l_param);
  *handled = !ref.get() || msg_handled_;
  return ret;
}

LRESULT HWNDMessageHandler::HandleNcHitTestMessage(unsigned int message,
                                                   WPARAM w_param,
                                                   LPARAM l_param,
                                                   bool* handled) {
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  LRESULT ret = OnNCHitTest(
      gfx::Point(CR_GET_X_LPARAM(l_param), CR_GET_Y_LPARAM(l_param)));
  *handled = !ref.get() || msg_handled_;
  return ret;
}

void HWNDMessageHandler::HandleParentChanged() {
  // If the forwarder window's parent is changed then we need to reset our
  // context as we will not receive touch releases if the touch was initiated
  // in the forwarder window.
  touch_ids_.clear();
}

void HWNDMessageHandler::ApplyPinchZoomScale(float scale) {
  POINT cursor_pos = GetCursorPos();
  ScreenToClient(hwnd(), &cursor_pos);

  ui::GestureEventDetails event_details(ui::EventType::kGesturePinchUpdate);
  event_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  event_details.set_scale(scale);

  ui::GestureEvent event(cursor_pos.x, cursor_pos.y, ui::EF_NONE,
                         base::TimeTicks::Now(), event_details);
  delegate_->HandleGestureEvent(&event);
}

void HWNDMessageHandler::ApplyPinchZoomBegin() {
  POINT cursor_pos = GetCursorPos();
  ScreenToClient(hwnd(), &cursor_pos);

  ui::GestureEventDetails event_details(ui::EventType::kGesturePinchBegin);
  event_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);

  ui::GestureEvent event(cursor_pos.x, cursor_pos.y, ui::EF_NONE,
                         base::TimeTicks::Now(), event_details);
  delegate_->HandleGestureEvent(&event);
}

void HWNDMessageHandler::ApplyPinchZoomEnd() {
  POINT cursor_pos = GetCursorPos();
  ScreenToClient(hwnd(), &cursor_pos);

  ui::GestureEventDetails event_details(ui::EventType::kGesturePinchEnd);
  event_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);

  ui::GestureEvent event(cursor_pos.x, cursor_pos.y, ui::EF_NONE,
                         base::TimeTicks::Now(), event_details);
  delegate_->HandleGestureEvent(&event);
}

void HWNDMessageHandler::ApplyPanGestureEvent(
    int scroll_x,
    int scroll_y,
    ui::EventMomentumPhase momentum_phase,
    ui::ScrollEventPhase phase) {
  gfx::Vector2d offset{scroll_x, scroll_y};

  POINT root_location = GetCursorPos();

  POINT location = {root_location.x, root_location.y};
  ScreenToClient(hwnd(), &location);

  gfx::Point cursor_location(location);
  gfx::Point cursor_root_location(root_location);

  int modifiers = ui::GetModifiersFromKeyState();

  ui::ScrollEvent event(ui::EventType::kScroll, cursor_location,
                        ui::EventTimeForNow(), modifiers, scroll_x, scroll_y,
                        scroll_x, scroll_y, 2, momentum_phase, phase);
  delegate_->HandleScrollEvent(&event);
}

void HWNDMessageHandler::ApplyPanGestureScroll(int scroll_x, int scroll_y) {
  ApplyPanGestureEvent(scroll_x, scroll_y, ui::EventMomentumPhase::NONE,
                       ui::ScrollEventPhase::kUpdate);
}

void HWNDMessageHandler::ApplyPanGestureFling(int scroll_x, int scroll_y) {
  ApplyPanGestureEvent(scroll_x, scroll_y,
                       ui::EventMomentumPhase::INERTIAL_UPDATE,
                       ui::ScrollEventPhase::kNone);
}

void HWNDMessageHandler::ApplyPanGestureScrollBegin(int scroll_x,
                                                    int scroll_y) {
  // Phase information will be ingored in ApplyPanGestureEvent().
  ApplyPanGestureEvent(scroll_x, scroll_y, ui::EventMomentumPhase::NONE,
                       ui::ScrollEventPhase::kBegan);
}

void HWNDMessageHandler::ApplyPanGestureScrollEnd(bool transitioning_to_pinch) {
  ApplyPanGestureEvent(0, 0,
                       transitioning_to_pinch ? ui::EventMomentumPhase::BLOCKED
                                              : ui::EventMomentumPhase::NONE,
                       ui::ScrollEventPhase::kEnd);
}

void HWNDMessageHandler::ApplyPanGestureFlingBegin() {
  ApplyPanGestureEvent(0, 0, ui::EventMomentumPhase::BEGAN,
                       ui::ScrollEventPhase::kNone);
}

void HWNDMessageHandler::ApplyPanGestureFlingEnd() {
  ApplyPanGestureEvent(0, 0, ui::EventMomentumPhase::END,
                       ui::ScrollEventPhase::kNone);
}

gfx::NativeViewAccessible HWNDMessageHandler::GetChildOfAXFragmentRoot() {
  return delegate_->GetNativeViewAccessible();
}

gfx::NativeViewAccessible HWNDMessageHandler::GetParentOfAXFragmentRoot() {
  return nullptr;
}

bool HWNDMessageHandler::IsAXFragmentRootAControlElement() {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, private:

void HWNDMessageHandler::InitExtras() {
  if (!called_enable_non_client_dpi_scaling_ && delegate_->HasFrame()) {
    // Derived signature; not available in headers.
    // This call gets Windows to scale the non-client area when
    // WM_DPICHANGED is fired.
    using EnableChildWindowDpiMessagePtr = LRESULT(WINAPI*)(HWND, BOOL);
    static const auto enable_child_window_dpi_message_func =
        reinterpret_cast<EnableChildWindowDpiMessagePtr>(
            base::win::GetUser32FunctionPointer("EnableChildWindowDpiMessage"));
    if (enable_child_window_dpi_message_func) {
      enable_child_window_dpi_message_func(hwnd(), TRUE);
    }
  }

  prop_window_target_ = std::make_unique<ui::ViewProp>(
      hwnd(), ui::WindowEventTarget::kWin32InputEventTarget,
      static_cast<ui::WindowEventTarget*>(this));
  DCHECK(delegate_->GetHWNDMessageDelegateInputMethod());
  observation_.Observe(delegate_->GetHWNDMessageDelegateInputMethod());

  // The usual way for UI Automation to obtain a fragment root is through
  // WM_GETOBJECT. However, if there's a relation such as "Controller For"
  // between element A in one window and element B in another window, UIA might
  // call element A to discover the relation, receive a pointer to element B,
  // then ask element B for its fragment root, without having sent WM_GETOBJECT
  // to element B's window.
  // So we create the fragment root now to ensure it's ready if asked for.
  if (::ui::AXPlatform::GetInstance().IsUiaProviderEnabled()) {
    ax_fragment_root_ = std::make_unique<ui::AXFragmentRootWin>(hwnd(), this);
  }

  // Disable pen flicks (http://crbug.com/506977)
  base::win::DisableFlicks(hwnd());
}

int HWNDMessageHandler::GetAppbarAutohideEdges(HMONITOR monitor) {
  autohide_factory_.InvalidateWeakPtrs();
  return ViewsDelegate::GetInstance()->GetAppbarAutohideEdges(
      monitor, base::BindOnce(&HWNDMessageHandler::OnAppbarAutohideEdgesChanged,
                              autohide_factory_.GetWeakPtr()));
}

void HWNDMessageHandler::OnAppbarAutohideEdgesChanged() {
  // This triggers querying WM_NCCALCSIZE again.
  RECT client;
  GetWindowRect(hwnd(), &client);

  // Add SWP_NOZORDER and SWP_NOACTIVATE flags to SetWindowPos to preserve the
  // correct Z-order after restarting maximized browsers. Without these flags,
  // SetWindowPos would always bring the current window to the top.
  SetWindowPos(hwnd(), nullptr, client.left, client.top,
               client.right - client.left, client.bottom - client.top,
               SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
}

void HWNDMessageHandler::SetInitialFocus() {
  if (!(GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_TRANSPARENT) &&
      !(GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
    // The window does not get keyboard messages unless we focus it.
    // Headless windows don't get native focus, so just pretend we grabbed one.
    if (IsHeadless()) {
      delegate_->HandleNativeFocus(/*last_focused_window=*/0);
    } else {
      ::SetFocus(hwnd());
    }
  }
}

void HWNDMessageHandler::PostProcessActivateMessage(
    int activation_state,
    bool minimized,
    HWND window_gaining_or_losing_activation) {
  DCHECK(IsTopLevelWindow(hwnd()));

  // Check if this is a legacy window created for screen readers.
  if (::IsChild(hwnd(), window_gaining_or_losing_activation) &&
      gfx::GetClassName(window_gaining_or_losing_activation) ==
          std::wstring(ui::kLegacyRenderWidgetHostHwnd)) {
    // In Aura, there is only one main HWND which comprises the whole browser
    // window. Some screen readers however, expect every unique web content
    // container (WebView) to be in its own HWND. In these cases, a dummy HWND
    // with class name |Chrome_RenderWidgetHostHWND| is created for each web
    // content container.
    // Note that this dummy window should not interfere with focus, and instead
    // delegates its accessibility implementation to the root of the
    // |BrowserAccessibilityManager| tree.
    return;
  }

  const bool active = activation_state != WA_INACTIVE && !minimized;
  if (notify_restore_on_activate_) {
    notify_restore_on_activate_ = false;
    delegate_->HandleWindowMinimizedOrRestored(/*restored=*/true);
    // Prevent OnSize() from also calling HandleWindowMinimizedOrRestored.
    last_size_param_ = SIZE_RESTORED;
  }
  if (delegate_->CanActivate())
    delegate_->HandleActivationChanged(active);

  if (!::IsWindow(window_gaining_or_losing_activation))
    window_gaining_or_losing_activation = ::GetForegroundWindow();

  // If the window losing activation is a fullscreen window, we reduce the size
  // of the window by 1px. i.e. Not fullscreen. This is to work around an
  // apparent bug in the Windows taskbar where in it tracks fullscreen state on
  // a per thread basis. This causes it not be a topmost window when any
  // maximized window on a thread which has a fullscreen window is active. This
  // affects the way these windows interact with the taskbar, they obscure it
  // when maximized, autohide does not work correctly, etc.
  // By reducing the size of the fullscreen window by 1px, we ensure that the
  // taskbar no longer treats the window and in turn the thread as a fullscreen
  // thread. This in turn ensures that maximized windows on the same thread
  // don't obscure the taskbar, etc.
  // Please note that this taskbar behavior only occurs if the window becoming
  // active is on the same monitor as the fullscreen window.
  if (!active) {
    if (IsFullscreen() && ::IsWindow(window_gaining_or_losing_activation)) {
      HMONITOR active_window_monitor = MonitorFromWindow(
          window_gaining_or_losing_activation, MONITOR_DEFAULTTOPRIMARY);
      HMONITOR fullscreen_window_monitor =
          MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY);

      if (active_window_monitor == fullscreen_window_monitor)
        OnBackgroundFullscreen();
    }
  } else if (background_fullscreen_hack_) {
    // Restore the bounds of the window to fullscreen.
    DCHECK(IsFullscreen());
    MONITORINFO monitor_info = {sizeof(monitor_info)};
    GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY),
                   &monitor_info);
    SetBoundsInternal(gfx::Rect(monitor_info.rcMonitor), false);
    // Inform the taskbar that this window is now a fullscreen window so it go
    // behind the window in the Z-Order. The taskbar heuristics to detect
    // fullscreen windows are not reliable. Marking it explicitly seems to work
    // around these problems.
    fullscreen_handler()->MarkFullscreen(true);
    background_fullscreen_hack_ = false;
  } else {
    // If the window becoming active has a fullscreen window on the same
    // monitor then we need to reduce the size of the fullscreen window by
    // 1 px. Please refer to the comments above for the reasoning behind
    // this.
    CheckAndHandleBackgroundFullscreenOnMonitor(
        window_gaining_or_losing_activation);
  }
}

void HWNDMessageHandler::RestoreEnabledIfNecessary() {
  if (delegate_->IsModal() && !restored_enabled_) {
    restored_enabled_ = true;
    // If we were run modally, we need to undo the disabled-ness we inflicted on
    // the owner's parent hierarchy.
    HWND start = ::GetWindow(hwnd(), GW_OWNER);
    while (start) {
      ::EnableWindow(start, TRUE);
      start = ::GetParent(start);
    }
  }
}

void HWNDMessageHandler::ExecuteSystemMenuCommand(int command) {
  if (command)
    SendMessage(hwnd(), WM_SYSCOMMAND, static_cast<WPARAM>(command), 0);
}

void HWNDMessageHandler::TrackMouseEvents(DWORD mouse_tracking_flags) {
  // Begin tracking mouse events for this HWND so that we get WM_MOUSELEAVE
  // when the user moves the mouse outside this HWND's bounds.
  if (active_mouse_tracking_flags_ == 0 || mouse_tracking_flags & TME_CANCEL) {
    if (mouse_tracking_flags & TME_CANCEL) {
      // We're about to cancel active mouse tracking, so empty out the stored
      // state.
      active_mouse_tracking_flags_ = 0;
    } else {
      active_mouse_tracking_flags_ = mouse_tracking_flags;
    }

    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = mouse_tracking_flags;
    tme.hwndTrack = hwnd();
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
  } else if (mouse_tracking_flags != active_mouse_tracking_flags_) {
    TrackMouseEvents(active_mouse_tracking_flags_ | TME_CANCEL);
    TrackMouseEvents(mouse_tracking_flags);
  }
}

void HWNDMessageHandler::ClientAreaSizeChanged() {
  // Ignore size changes due to fullscreen windows losing activation and
  // in headless mode since it maintains expected rather than actual platform
  // window bounds.
  if ((background_fullscreen_hack_ && !sent_window_size_changing_) ||
      IsHeadless()) {
    return;
  }
  auto ref = msg_handler_weak_factory_.GetWeakPtr();
  delegate_->HandleClientSizeChanged(GetClientAreaBounds().size());
  if (!ref)
    return;

  current_window_size_message_++;
  sent_window_size_changing_ = false;
}

bool HWNDMessageHandler::GetClientAreaInsets(gfx::Insets* insets,
                                             HMONITOR monitor) const {
  if (delegate_->GetClientAreaInsets(insets, monitor))
    return true;
  DCHECK(insets->IsEmpty());

  // Returning false causes the default handling in OnNCCalcSize() to
  // be invoked.
  if (!delegate_->HasNonClientView() || HasSystemFrame())
    return false;

  if (IsMaximized()) {
    // Windows automatically adds a standard width border to all sides when a
    // window is maximized.
    int frame_thickness = ui::GetFrameThickness(monitor);
    if (!delegate_->HasFrame())
      frame_thickness -= 1;
    *insets = gfx::Insets(frame_thickness);
    return true;
  }

  *insets = gfx::Insets();
  return true;
}

void HWNDMessageHandler::ResetWindowRegion(bool force, bool redraw) {
  // A native frame uses the native window region, and we don't want to mess
  // with it.
  // WS_EX_LAYERED automatically makes clicks on transparent pixels fall
  // through, but that isn't the case when using Direct3D to draw transparent
  // windows. So we route translucent windows throught to the delegate to
  // allow for a custom hit mask.
  if (!is_translucent_ && !custom_window_region_.is_valid() &&
      (IsFrameSystemDrawn() || !delegate_->HasNonClientView())) {
    if (force)
      SetWindowRgn(hwnd(), nullptr, redraw);
    return;
  }

  // Changing the window region is going to force a paint. Only change the
  // window region if the region really differs.
  base::win::ScopedRegion current_rgn(CreateRectRgn(0, 0, 0, 0));
  GetWindowRgn(hwnd(), current_rgn.get());

  RECT window_rect;
  GetWindowRect(hwnd(), &window_rect);
  base::win::ScopedRegion new_region;
  if (custom_window_region_.is_valid()) {
    new_region.reset(CreateRectRgn(0, 0, 0, 0));
    CombineRgn(new_region.get(), custom_window_region_.get(), nullptr,
               RGN_COPY);
  } else if (IsMaximized()) {
    HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof mi;
    GetMonitorInfo(monitor, &mi);
    RECT work_rect = mi.rcWork;
    OffsetRect(&work_rect, -window_rect.left, -window_rect.top);
    new_region.reset(CreateRectRgnIndirect(&work_rect));
  } else {
    SkPath window_mask;
    delegate_->GetWindowMask(gfx::Size(window_rect.right - window_rect.left,
                                       window_rect.bottom - window_rect.top),
                             &window_mask);
    if (!window_mask.isEmpty())
      new_region.reset(gfx::CreateHRGNFromSkPath(window_mask));
  }

  const bool has_current_region = current_rgn != nullptr;
  const bool has_new_region = new_region != nullptr;
  if (has_current_region != has_new_region ||
      (has_current_region && !EqualRgn(current_rgn.get(), new_region.get()))) {
    // SetWindowRgn takes ownership of the HRGN.
    SetWindowRgn(hwnd(), new_region.release(), redraw);
  }
}

LRESULT HWNDMessageHandler::DefWindowProcWithRedrawLock(UINT message,
                                                        WPARAM w_param,
                                                        LPARAM l_param) {
  ScopedRedrawLock lock(this);
  // The Widget and HWND can be destroyed in the call to DefWindowProc, so use
  // the WeakPtrFactory to avoid unlocking (and crashing) after destruction.
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  LRESULT result = DefWindowProc(hwnd(), message, w_param, l_param);
  if (!ref)
    lock.CancelUnlockOperation();
  return result;
}

void HWNDMessageHandler::LockUpdates() {
  if (++lock_updates_count_ == 1) {
    SetWindowLong(hwnd(), GWL_STYLE,
                  GetWindowLong(hwnd(), GWL_STYLE) & ~WS_VISIBLE);
  }
}

void HWNDMessageHandler::UnlockUpdates() {
  if (--lock_updates_count_ <= 0) {
    SetWindowLong(hwnd(), GWL_STYLE,
                  GetWindowLong(hwnd(), GWL_STYLE) | WS_VISIBLE);
    lock_updates_count_ = 0;
  }
}

void HWNDMessageHandler::ForceRedrawWindow(int attempts) {
  if (ui::IsWorkstationLocked()) {
    // Presents will continue to fail as long as the input desktop is
    // unavailable.
    if (--attempts <= 0)
      return;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HWNDMessageHandler::ForceRedrawWindow,
                       msg_handler_weak_factory_.GetWeakPtr(), attempts),
        base::Milliseconds(500));
    return;
  }
  InvalidateRect(hwnd(), nullptr, FALSE);
}

bool HWNDMessageHandler::IsFrameSystemDrawn() const {
  FrameMode frame_mode = delegate_->GetFrameMode();
  return frame_mode == FrameMode::SYSTEM_DRAWN ||
         frame_mode == FrameMode::SYSTEM_DRAWN_NO_CONTROLS;
}

bool HWNDMessageHandler::HasSystemFrame() const {
  return delegate_->HasFrame() && IsFrameSystemDrawn();
}

// Message handlers ------------------------------------------------------------

void HWNDMessageHandler::OnActivateApp(BOOL active, DWORD thread_id) {
  if (delegate_->HasNonClientView() && !active &&
      thread_id != GetCurrentThreadId()) {
    // Update the native frame if it is rendering the non-client area.
    if (HasSystemFrame())
      DefWindowProcWithRedrawLock(WM_NCACTIVATE, FALSE, 0);
  }
}

BOOL HWNDMessageHandler::OnAppCommand(HWND window,
                                      int command,
                                      WORD device,
                                      WORD keystate) {
  BOOL handled = !!delegate_->HandleAppCommand(command);
  SetMsgHandled(handled);
  // Make sure to return TRUE if the event was handled or in some cases the
  // system will execute the default handler which can cause bugs like going
  // forward or back two pages instead of one.
  return handled;
}

void HWNDMessageHandler::OnCancelMode() {
  delegate_->HandleCancelMode();
  // Need default handling, otherwise capture and other things aren't canceled.
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnCaptureChanged(HWND window) {
  delegate_->HandleCaptureLost();
}

void HWNDMessageHandler::OnClose() {
  delegate_->HandleClose();
}

void HWNDMessageHandler::OnCommand(UINT notification_code,
                                   int command,
                                   HWND window) {
  // If the notification code is > 1 it means it is control specific and we
  // should ignore it.
  if (notification_code > 1 || delegate_->HandleAppCommand(command))
    SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnCreate(CREATESTRUCT* create_struct) {
  if (is_translucent_) {
    // This is part of the magic to emulate layered windows with Aura
    // see the explanation elsewere when we set is_translucent_.
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd(), &margins);

    ::SetProp(hwnd(), ui::kWindowTranslucent, reinterpret_cast<HANDLE>(1));
  }

  fullscreen_handler_->set_hwnd(hwnd());

  base::win::AllowDarkModeForWindow(hwnd(), true);

  // This message initializes the window so that focus border are shown for
  // windows.
  SendMessage(hwnd(), WM_CHANGEUISTATE, MAKELPARAM(UIS_CLEAR, UISF_HIDEFOCUS),
              0);

  if (!delegate_->HasFrame()) {
    SetWindowLong(hwnd(), GWL_STYLE,
                  GetWindowLong(hwnd(), GWL_STYLE) & ~WS_CAPTION);
    SendFrameChanged();
  }

  // Get access to a modifiable copy of the system menu.
  GetSystemMenu(hwnd(), false);

  if (!pointer_events_for_touch_)
    RegisterTouchWindow(hwnd(), TWF_WANTPALM);

  // We need to allow the delegate to size its contents since the window may not
  // receive a size notification when its initial bounds are specified at window
  // creation time.
  ClientAreaSizeChanged();

  delegate_->HandleCreate();

  session_change_observer_ =
      std::make_unique<ui::SessionChangeObserver>(base::BindRepeating(
          &HWNDMessageHandler::OnSessionChange, base::Unretained(this)));

  // If the window was initialized with a specific size/location then we know
  // the DPI and thus must initialize dpi_ now. See https://crbug.com/1282804
  // for details.
  if (initial_bounds_valid_)
    dpi_ = display::win::ScreenWin::GetDPIForHWND(hwnd());

  // TODO(beng): move more of NWW::OnCreate here.
  return 0;
}

void HWNDMessageHandler::OnDestroy() {
  ::RemoveProp(hwnd(), ui::kWindowTranslucent);
  session_change_observer_.reset(nullptr);
  delegate_->HandleDestroying();
  // If the window going away is a fullscreen window then remove its references
  // from the full screen window map.
  auto& map = fullscreen_monitor_map_.Get();
  const auto i = base::ranges::find(
      map, this, &FullscreenWindowMonitorMap::value_type::second);
  if (i != map.end())
    map.erase(i);

  // If we have ever returned a UIA object via WM_GETOBJECT, signal that all
  // objects associated with this HWND can be discarded. See:
  // https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiareturnrawelementprovider#remarks
  if (did_return_uia_object_)
    UiaReturnRawElementProvider(hwnd(), 0, 0, nullptr);
}

void HWNDMessageHandler::OnDisplayChange(UINT bits_per_pixel,
                                         const gfx::Size& screen_size) {
  TRACE_EVENT0("ui", "HWNDMessageHandler::OnDisplayChange");

  // Typically, in the case of display changes, ScreenWin's OnDisplayChange
  // handler will get called first, but sometimes it doesn't. This catches
  // that case, when monitors are added or removed, without a lot of extra
  // updates of the global ScreenWin DisplayInfos state. See
  // https://crbug.com/1413940 for more info.
  display::win::ScreenWin::UpdateDisplayInfosIfNeeded();

  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  delegate_->HandleDisplayChange();

  // HandleDisplayChange() may result in |this| being deleted.
  if (!ref)
    return;

  // Force a WM_NCCALCSIZE to occur to ensure that we handle auto hide
  // taskbars correctly.
  SendFrameChanged();
}

LRESULT HWNDMessageHandler::OnDpiChanged(UINT msg,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  if (LOWORD(w_param) != HIWORD(w_param))
    NOTIMPLEMENTED() << "Received non-square scaling factors";

  TRACE_EVENT("ui", "HWNDMessageHandler::OnDpiChanged",
              [&](perfetto::EventContext ctx) {
                perfetto::protos::pbzero::ChromeWindowHandleEventInfo* args =
                    ctx.event()->set_chrome_window_handle_event_info();
                args->set_dpi(LOWORD(w_param));
              });

  int dpi;
  float scaling_factor;
  if (display::Display::HasForceDeviceScaleFactor()) {
    scaling_factor = display::Display::GetForcedDeviceScaleFactor();
    dpi = display::win::GetDPIFromScalingFactor(scaling_factor);
  } else {
    dpi = LOWORD(w_param);
    scaling_factor = display::win::ScreenWin::GetScaleFactorForDPI(dpi);
  }

  // The first WM_DPICHANGED originates from EnableChildWindowDpiMessage during
  // initialization. We don't want to propagate this as the client is already
  // set at the current scale factor and may cause the window to display too
  // soon. See http://crbug.com/625076.
  if (dpi_ == 0) {
    // See https://crbug.com/1252564 for why we need to ignore the first
    // OnDpiChanged message in this way.
    dpi_ = dpi;
    return 0;
  }

  dpi_ = dpi;
  // Make sure the displays have up to date scale factors before updating the
  // window's scale factor. Otherwise, we can be in an inconsistent state,
  // in which the display a window is on has a different scale factor than the
  // window, when the window handles the scale factor change.
  // See https://crbug.com/1368455 for more info.
  display::win::ScreenWin::UpdateDisplayInfos();
  SetBoundsInternal(gfx::Rect(*reinterpret_cast<RECT*>(l_param)), false);
  delegate_->HandleWindowScaleFactorChanged(scaling_factor);
  return 0;
}

void HWNDMessageHandler::OnEnterMenuLoop(BOOL from_track_popup_menu) {
  if (menu_depth_++ == 0)
    delegate_->HandleMenuLoop(true);
}

void HWNDMessageHandler::OnEnterSizeMove() {
  delegate_->HandleBeginWMSizeMove();
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnEraseBkgnd(HDC dc) {
  gfx::Insets insets;
  if (delegate_->GetDwmFrameInsetsInPixels(&insets) && !insets.IsEmpty() &&
      needs_dwm_frame_clear_) {
    // This is necessary to avoid white flashing in the titlebar area around the
    // minimize/maximize/close buttons.
    needs_dwm_frame_clear_ = false;
    RECT client_rect;
    GetClientRect(hwnd(), &client_rect);
    base::win::ScopedGDIObject<HBRUSH> brush(CreateSolidBrush(0));
    // The DC and GetClientRect operate in client area coordinates.
    RECT rect = {0, 0, client_rect.right, insets.top()};
    FillRect(dc, &rect, brush.get());
  }
  // Needed to prevent resize flicker.
  return 1;
}

void HWNDMessageHandler::OnExitMenuLoop(BOOL is_shortcut_menu) {
  if (--menu_depth_ == 0)
    delegate_->HandleMenuLoop(false);
  DCHECK_GE(0, menu_depth_);
}

void HWNDMessageHandler::OnExitSizeMove() {
  delegate_->HandleEndWMSizeMove();
  SetMsgHandled(FALSE);
  // If the window was moved to a monitor which has a fullscreen window active,
  // we need to reduce the size of the fullscreen window by 1px.
  CheckAndHandleBackgroundFullscreenOnMonitor(hwnd());
}

void HWNDMessageHandler::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  gfx::Size min_window_size;
  gfx::Size max_window_size;
  delegate_->GetMinMaxSize(&min_window_size, &max_window_size);
  min_window_size = delegate_->DIPToScreenSize(min_window_size);
  max_window_size = delegate_->DIPToScreenSize(max_window_size);

  // Add the native frame border size to the minimum and maximum size if the
  // view reports its size as the client size.
  if (delegate_->WidgetSizeIsClientSize()) {
    RECT client_rect, window_rect;
    GetClientRect(hwnd(), &client_rect);
    GetWindowRect(hwnd(), &window_rect);
    CR_DEFLATE_RECT(&window_rect, &client_rect);
    min_window_size.Enlarge(window_rect.right - window_rect.left,
                            window_rect.bottom - window_rect.top);
    // Either axis may be zero, so enlarge them independently.
    if (max_window_size.width())
      max_window_size.Enlarge(window_rect.right - window_rect.left, 0);
    if (max_window_size.height())
      max_window_size.Enlarge(0, window_rect.bottom - window_rect.top);
  }
  minmax_info->ptMinTrackSize.x = min_window_size.width();
  minmax_info->ptMinTrackSize.y = min_window_size.height();
  if (max_window_size.width() || max_window_size.height()) {
    if (!max_window_size.width())
      max_window_size.set_width(GetSystemMetrics(SM_CXMAXTRACK));
    if (!max_window_size.height())
      max_window_size.set_height(GetSystemMetrics(SM_CYMAXTRACK));
    minmax_info->ptMaxTrackSize.x = max_window_size.width();
    minmax_info->ptMaxTrackSize.y = max_window_size.height();
  }
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnGetObject(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  LRESULT reference_result = static_cast<LRESULT>(0L);

  // Only the lower 32 bits of l_param are valid when checking the object id
  // because it sometimes gets sign-extended incorrectly (but not always).
  DWORD obj_id = static_cast<DWORD>(static_cast<DWORD_PTR>(l_param));

  const bool is_uia_request = static_cast<DWORD>(UiaRootObjectId) == obj_id;
  const bool is_uia_active =
      is_uia_request && ::ui::AXPlatform::GetInstance().IsUiaProviderEnabled();
  const bool is_msaa_request = static_cast<DWORD>(OBJID_CLIENT) == obj_id;

  if (is_uia_request) {
    ::ui::AXPlatform::GetInstance().OnUiaProviderRequested(is_uia_active);
  }

  if ((is_uia_request || is_msaa_request) &&
      delegate_->GetNativeViewAccessible()) {
    // Expose either the UIA or the MSAA implementation, but not both, depending
    // on the state of the feature flag.
    if (is_uia_active) {
      // Retrieve UIA object for the root view.
      Microsoft::WRL::ComPtr<IRawElementProviderSimple> root;
      ax_fragment_root_->GetNativeViewAccessible()->QueryInterface(
          IID_PPV_ARGS(&root));

      // Return the UIA object via UiaReturnRawElementProvider(). See:
      // https://docs.microsoft.com/en-us/windows/win32/winauto/wm-getobject
      did_return_uia_object_ = true;
      reference_result =
          UiaReturnRawElementProvider(hwnd(), w_param, l_param, root.Get());
    } else if (is_msaa_request) {
      // Retrieve MSAA dispatch object for the root view.
      Microsoft::WRL::ComPtr<IAccessible> root(
          delegate_->GetNativeViewAccessible());
      reference_result =
          LresultFromObject(IID_IAccessible, w_param, root.Get());
    }
  } else if (::GetFocus() == hwnd() && ax_system_caret_ &&
             static_cast<DWORD>(OBJID_CARET) == obj_id) {
    Microsoft::WRL::ComPtr<IAccessible> ax_system_caret_accessible =
        ax_system_caret_->GetCaret();
    reference_result = LresultFromObject(IID_IAccessible, w_param,
                                         ax_system_caret_accessible.Get());
  }

  return reference_result;
}

LRESULT HWNDMessageHandler::OnImeMessages(UINT message,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  LRESULT result = 0;
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  const bool msg_handled =
      delegate_->HandleIMEMessage(message, w_param, l_param, &result);
  if (ref.get())
    SetMsgHandled(msg_handled);
  return result;
}

void HWNDMessageHandler::OnInitMenu(HMENU menu) {
  bool is_fullscreen = IsFullscreen();
  bool is_minimized = IsMinimized();
  bool is_maximized = IsMaximized();
  bool is_restored = !is_fullscreen && !is_minimized && !is_maximized;

  ScopedRedrawLock lock(this);
  EnableMenuItemByCommand(
      menu, SC_RESTORE,
      delegate_->CanResize() && (is_minimized || is_maximized));
  EnableMenuItemByCommand(menu, SC_MOVE, is_restored);
  EnableMenuItemByCommand(menu, SC_SIZE, delegate_->CanResize() && is_restored);
  EnableMenuItemByCommand(
      menu, SC_MAXIMIZE,
      delegate_->CanMaximize() && !is_fullscreen && !is_maximized);
  EnableMenuItemByCommand(menu, SC_MINIMIZE,
                          delegate_->CanMinimize() && !is_minimized);

  if (is_maximized && delegate_->CanResize())
    ::SetMenuDefaultItem(menu, SC_RESTORE, FALSE);
  else if (!is_maximized && delegate_->CanMaximize())
    ::SetMenuDefaultItem(menu, SC_MAXIMIZE, FALSE);
}

void HWNDMessageHandler::OnInputLangChange(DWORD character_set,
                                           HKL input_language_id) {
  delegate_->HandleInputLanguageChange(character_set, input_language_id);
}

LRESULT HWNDMessageHandler::OnKeyEvent(UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param) {
  CHROME_MSG msg = {hwnd(), message, w_param, l_param,
                    static_cast<DWORD>(GetMessageTime())};
  ui::KeyEvent key(msg);
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  delegate_->HandleKeyEvent(&key);
  if (!ref)
    return 0;
  if (!key.handled())
    SetMsgHandled(FALSE);
  return 0;
}

void HWNDMessageHandler::OnKillFocus(HWND focused_window) {
  // Headless windows are believed to always have focus, so avoid
  // reporting native focus changes.
  if (!IsHeadless()) {
    delegate_->HandleNativeBlur(focused_window);
  }
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnMouseActivate(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  // Please refer to the comments in the header for the touch_down_contexts_
  // member for the if statement below.
  if (touch_down_contexts_)
    return MA_NOACTIVATE;

  // On Windows, if we select the menu item by touch and if the window at the
  // location is another window on the same thread, that window gets a
  // WM_MOUSEACTIVATE message and ends up activating itself, which is not
  // correct. We workaround this by setting a property on the window at the
  // current cursor location. We check for this property in our
  // WM_MOUSEACTIVATE handler and don't activate the window if the property is
  // set.
  if (::GetProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow)) {
    ::RemoveProp(hwnd(), ui::kIgnoreTouchMouseActivateForWindow);
    return MA_NOACTIVATE;
  }

  // TODO(beng): resolve this with the GetWindowLong() check on the subsequent
  //             line.
  if (delegate_->HasNonClientView()) {
    if (delegate_->CanActivate())
      return MA_ACTIVATE;
    if (delegate_->WantsMouseEventsWhenInactive())
      return MA_NOACTIVATE;
    return MA_NOACTIVATEANDEAT;
  }
  if (GetWindowLong(hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE)
    return MA_NOACTIVATE;
  SetMsgHandled(FALSE);
  return MA_ACTIVATE;
}

LRESULT HWNDMessageHandler::OnMouseRange(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  return HandleMouseEventInternal(message, w_param, l_param, true);
}

// On some systems with a high-resolution track pad and running Windows 10,
// using the scrolling gesture (two-finger scroll) on the track pad
// causes it to also generate a WM_POINTERDOWN message if the window
// isn't focused. This leads to a WM_POINTERACTIVATE message and the window
// gaining focus and coming to the front. This code detects a
// WM_POINTERACTIVATE coming from the track pad and kills the activation
// of the window. NOTE: most other trackpad messages come in as mouse
// messages, including WM_MOUSEWHEEL instead of WM_POINTERWHEEL.
LRESULT HWNDMessageHandler::OnPointerActivate(UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param) {
  POINTER_INPUT_TYPE pointer_type;
  if (::GetPointerType(GET_POINTERID_WPARAM(w_param), &pointer_type) &&
      pointer_type == PT_TOUCHPAD) {
    return PA_NOACTIVATE;
  }
  SetMsgHandled(FALSE);
  return -1;
}

LRESULT HWNDMessageHandler::OnPointerEvent(UINT message,
                                           WPARAM w_param,
                                           LPARAM l_param) {
  POINTER_INPUT_TYPE pointer_type;
  // If the WM_POINTER messages are not sent from a stylus device, then we do
  // not handle them to make sure we do not change the current behavior of
  // touch and mouse inputs.
  if (!::GetPointerType(GET_POINTERID_WPARAM(w_param), &pointer_type)) {
    SetMsgHandled(FALSE);
    return -1;
  }

  // |HandlePointerEventTypePenClient| assumes all pen events happen on the
  // client area, so WM_NCPOINTER messages sent to it would eventually be
  // dropped and the native frame wouldn't be able to respond to pens.
  // |HandlePointerEventTypeTouchOrNonClient| handles non-client area messages
  // properly. Since we don't need to distinguish between pens and fingers in
  // non-client area, route the messages to that method.
  if (pointer_type == PT_PEN &&
      (message == WM_NCPOINTERDOWN ||
       message == WM_NCPOINTERUP ||
       message == WM_NCPOINTERUPDATE)) {
    pointer_type = PT_TOUCH;
  }

  switch (pointer_type) {
    case PT_PEN:
      return HandlePointerEventTypePenClient(message, w_param, l_param);
    case PT_TOUCH:
      if (pointer_events_for_touch_) {
        return HandlePointerEventTypeTouchOrNonClient(
            message, w_param, l_param);
      }
      [[fallthrough]];
    default:
      break;
  }
  SetMsgHandled(FALSE);
  return -1;
}

LRESULT HWNDMessageHandler::OnInputEvent(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  if (!using_wm_input_)
    return -1;

  HRAWINPUT input_handle = reinterpret_cast<HRAWINPUT>(l_param);

  // Get the size of the input record.
  UINT size = 0;
  UINT result = ::GetRawInputData(input_handle, RID_INPUT, nullptr, &size,
                                  sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(0u, result);

  // Retrieve the input record.
  auto buffer = std::make_unique<uint8_t[]>(size);
  RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.get());
  result = ::GetRawInputData(input_handle, RID_INPUT, buffer.get(), &size,
                             sizeof(RAWINPUTHEADER));
  if (result == static_cast<UINT>(-1)) {
    PLOG(ERROR) << "GetRawInputData() failed";
    return 0;
  }
  DCHECK_EQ(size, result);

  if (input->header.dwType == RIM_TYPEMOUSE &&
      input->data.mouse.usButtonFlags != RI_MOUSE_WHEEL) {
    POINT cursor_pos = {0};
    ::GetCursorPos(&cursor_pos);
    ScreenToClient(hwnd(), &cursor_pos);
    ui::MouseEvent event(
        ui::EventType::kMouseMoved, gfx::PointF(cursor_pos.x, cursor_pos.y),
        gfx::PointF(cursor_pos.x, cursor_pos.y), ui::EventTimeForNow(),
        GetFlagsFromRawInputMessage(input), 0);
    if (!(input->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
      ui::MouseEvent::DispatcherApi(&event).set_movement(
          gfx::Vector2dF(input->data.mouse.lLastX, input->data.mouse.lLastY));
    }
    delegate_->HandleMouseEvent(&event);
  }

  return ::DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
}

void HWNDMessageHandler::OnMove(const gfx::Point& point) {
  delegate_->HandleMove();
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnMoving(UINT param, const RECT* new_bounds) {
  delegate_->HandleMove();
}

LRESULT HWNDMessageHandler::OnNCActivate(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  // Per MSDN, w_param is either TRUE or FALSE. However, MSDN also hints that:
  // "If the window is minimized when this message is received, the application
  // should pass the message to the DefWindowProc function."
  // It is found out that the high word of w_param might be set when the window
  // is minimized or restored. To handle this, w_param's high word should be
  // cleared before it is converted to BOOL.
  BOOL active = static_cast<BOOL>(LOWORD(w_param));

  const bool paint_as_active = delegate_->ShouldPaintAsActive();

  if (!delegate_->HasNonClientView()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  if (!delegate_->CanActivate())
    return TRUE;

  if (delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    // TODO(beng, et al): Hack to redraw this window and child windows
    //     synchronously upon activation. Not all child windows are redrawing
    //     themselves leading to issues like http://crbug.com/74604
    //     We redraw out-of-process HWNDs asynchronously to avoid hanging the
    //     whole app if a child HWND belonging to a hung plugin is encountered.
    RedrawWindow(hwnd(), nullptr, nullptr,
                 RDW_NOCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW);
    EnumChildWindows(hwnd(), EnumChildWindowsForRedraw, NULL);
  }

  // The frame may need to redraw as a result of the activation change.
  // We can get WM_NCACTIVATE before we're actually visible. If we're not
  // visible, no need to paint.
  if (IsVisible())
    delegate_->SchedulePaint();

  // Calling DefWindowProc is only necessary if there's a system frame being
  // drawn. Otherwise it can draw an incorrect title bar and cause visual
  // corruption.
  if (!delegate_->HasFrame() ||
      delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    SetMsgHandled(TRUE);
    return TRUE;
  }

  return DefWindowProcWithRedrawLock(WM_NCACTIVATE, paint_as_active || active,
                                     0);
}

LRESULT HWNDMessageHandler::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We only override the default handling if we need to specify a custom
  // non-client edge width. Note that in most cases "no insets" means no
  // custom width, but in fullscreen mode or when the NonClientFrameView
  // requests it, we want a custom width of 0.

  // Let User32 handle the first nccalcsize for captioned windows
  // so it updates its internal structures (specifically caption-present)
  // Without this Tile & Cascade windows won't work.
  // See http://code.google.com/p/chromium/issues/detail?id=900
  if (is_first_nccalc_) {
    is_first_nccalc_ = false;
    if (GetWindowLong(hwnd(), GWL_STYLE) & WS_CAPTION) {
      SetMsgHandled(FALSE);
      return 0;
    }
  }

  RECT* client_rect =
      mode ? &(reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param)->rgrc[0])
           : reinterpret_cast<RECT*>(l_param);

  HMONITOR monitor = MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONULL);
  if (!monitor) {
    // We might end up here if the window was previously minimized and the
    // user clicks on the taskbar button to restore it in the previous
    // position. In that case WM_NCCALCSIZE is sent before the window
    // coordinates are restored to their previous values, so our (left,top)
    // would probably be (-32000,-32000) like all minimized windows. So the
    // above MonitorFromWindow call fails, but if we check the window rect
    // given with WM_NCCALCSIZE (which is our previous restored window
    // position) we will get the correct monitor handle.
    monitor = MonitorFromRect(client_rect, MONITOR_DEFAULTTONULL);
    if (!monitor) {
      // This is probably an extreme case that we won't hit, but if we don't
      // intersect any monitor, let us not adjust the client rect since our
      // window will not be visible anyway.
      return 0;
    }
  }

  gfx::Insets insets;
  bool got_insets = GetClientAreaInsets(&insets, monitor);
  if (!got_insets && !IsFullscreen() && !(mode && !delegate_->HasFrame())) {
    SetMsgHandled(FALSE);
    return 0;
  }

  client_rect->left += insets.left();
  client_rect->top += insets.top();
  client_rect->bottom -= insets.bottom();
  client_rect->right -= insets.right();
  if (IsMaximized()) {
    // Find all auto-hide taskbars along the screen edges and adjust in by the
    // thickness of the auto-hide taskbar on each such edge, so the window isn't
    // treated as a "fullscreen app", which would cause the taskbars to
    // disappear.
    const int autohide_edges = GetAppbarAutohideEdges(monitor);
    if (autohide_edges & ViewsDelegate::EDGE_LEFT)
      client_rect->left += kAutoHideTaskbarThicknessPx;
    if (autohide_edges & ViewsDelegate::EDGE_TOP) {
      if (IsFrameSystemDrawn()) {
        // Tricky bit.  Due to a bug in DwmDefWindowProc()'s handling of
        // WM_NCHITTEST, having any nonclient area atop the window causes the
        // caption buttons to draw onscreen but not respond to mouse
        // hover/clicks.
        // So for a taskbar at the screen top, we can't push the
        // client_rect->top down; instead, we move the bottom up by one pixel,
        // which is the smallest change we can make and still get a client area
        // less than the screen size. This is visibly ugly, but there seems to
        // be no better solution.
        --client_rect->bottom;
      } else {
        client_rect->top += kAutoHideTaskbarThicknessPx;
      }
    }
    if (autohide_edges & ViewsDelegate::EDGE_RIGHT)
      client_rect->right -= kAutoHideTaskbarThicknessPx;
    if (autohide_edges & ViewsDelegate::EDGE_BOTTOM)
      client_rect->bottom -= kAutoHideTaskbarThicknessPx;

    // We cannot return WVR_REDRAW when there is nonclient area, or Windows
    // exhibits bugs where client pixels and child HWNDs are mispositioned by
    // the width/height of the upper-left nonclient area.
    return 0;
  }

  // If the window bounds change, we're going to relayout and repaint anyway.
  // Returning WVR_REDRAW avoids an extra paint before that of the old client
  // pixels in the (now wrong) location, and thus makes actions like resizing a
  // window from the left edge look slightly less broken.
  // We special case when left or top insets are 0, since these conditions
  // actually require another repaint to correct the layout after glass gets
  // turned on and off.
  if (insets.left() == 0 || insets.top() == 0)
    return 0;
  return mode ? WVR_REDRAW : 0;
}

LRESULT HWNDMessageHandler::OnNCCreate(LPCREATESTRUCT lpCreateStruct) {
  SetMsgHandled(FALSE);
  if (delegate_->HasFrame()) {
    using EnableNonClientDpiScalingPtr = decltype(::EnableNonClientDpiScaling)*;
    static const auto enable_non_client_dpi_scaling_func =
        reinterpret_cast<EnableNonClientDpiScalingPtr>(
            base::win::GetUser32FunctionPointer("EnableNonClientDpiScaling"));
    called_enable_non_client_dpi_scaling_ =
        !!(enable_non_client_dpi_scaling_func &&
           enable_non_client_dpi_scaling_func(hwnd()));
  }
  return FALSE;
}

LRESULT HWNDMessageHandler::OnNCHitTest(const gfx::Point& point) {
  if (!delegate_->HasNonClientView()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  // Some views may overlap the non client area of the window.
  // This means that we should look for these views before handing the
  // hittest message off to DWM or DefWindowProc.
  // If the hittest returned from the search for a view returns HTCLIENT
  // then it means that we have a view overlapping the non client area.
  // In all other cases we can fallback to the system default handling.

  // Allow the NonClientView to handle the hittest to see if we have a view
  // overlapping the non client area of the window.
  POINT temp = {point.x(), point.y()};
  MapWindowPoints(HWND_DESKTOP, hwnd(), &temp, 1);
  int component = delegate_->GetNonClientComponent(gfx::Point(temp));
  if (component == HTCLIENT)
    return component;

  // If the DWM is rendering the window controls, we need to give the DWM's
  // default window procedure first chance to handle hit testing.
  if (HasSystemFrame() &&
      delegate_->GetFrameMode() != FrameMode::SYSTEM_DRAWN_NO_CONTROLS) {
    LRESULT result;
    if (DwmDefWindowProc(hwnd(), WM_NCHITTEST, 0,
                         MAKELPARAM(point.x(), point.y()), &result)) {
      return result;
    }
  }

  // If the point is specified as custom or system nonclient item, return it.
  if (component != HTNOWHERE)
    return component;

  // Otherwise, we let Windows do all the native frame non-client handling for
  // us.
  LRESULT hit_test_code =
      DefWindowProc(hwnd(), WM_NCHITTEST, 0, MAKELPARAM(point.x(), point.y()));
  return hit_test_code;
}

void HWNDMessageHandler::OnNCPaint(HRGN rgn) {
  RECT window_rect;
  GetWindowRect(hwnd(), &window_rect);
  RECT dirty_region;
  // A value of 1 indicates paint all.
  if (!rgn || rgn == reinterpret_cast<HRGN>(1)) {
    dirty_region.left = 0;
    dirty_region.top = 0;
    dirty_region.right = window_rect.right - window_rect.left;
    dirty_region.bottom = window_rect.bottom - window_rect.top;
  } else {
    RECT rgn_bounding_box;
    GetRgnBox(rgn, &rgn_bounding_box);
    if (!IntersectRect(&dirty_region, &rgn_bounding_box, &window_rect)) {
      SetMsgHandled(FALSE);
      return;  // Dirty region doesn't intersect window bounds, bail.
    }

    // rgn_bounding_box is in screen coordinates. Map it to window coordinates.
    OffsetRect(&dirty_region, -window_rect.left, -window_rect.top);
  }

  // We only do non-client painting if we're not using the system frame.
  // It's required to avoid some native painting artifacts from appearing when
  // the window is resized.
  if (!delegate_->HasNonClientView() || IsFrameSystemDrawn()) {
    // The default WM_NCPAINT handler under Aero Glass doesn't clear the
    // nonclient area, so it'll remain the default white color. That area is
    // invisible initially (covered by the window border) but can become
    // temporarily visible on maximizing or fullscreening, so clear it here.
    HDC dc = GetWindowDC(hwnd());
    RECT client_rect;
    ::GetClientRect(hwnd(), &client_rect);
    ::MapWindowPoints(hwnd(), nullptr, reinterpret_cast<POINT*>(&client_rect),
                      2);
    ::OffsetRect(&client_rect, -window_rect.left, -window_rect.top);
    // client_rect now is in window space.

    base::win::ScopedRegion base(::CreateRectRgnIndirect(&dirty_region));
    base::win::ScopedRegion client(::CreateRectRgnIndirect(&client_rect));
    base::win::ScopedRegion nonclient(::CreateRectRgn(0, 0, 0, 0));
    ::CombineRgn(nonclient.get(), base.get(), client.get(), RGN_DIFF);

    ::SelectClipRgn(dc, nonclient.get());
    HBRUSH brush = CreateSolidBrush(0);
    ::FillRect(dc, &dirty_region, brush);
    ::DeleteObject(brush);
    ::ReleaseDC(hwnd(), dc);
    SetMsgHandled(FALSE);
    return;
  }

  gfx::Size root_view_size = delegate_->GetRootViewSize();
  if (gfx::Size(window_rect.right - window_rect.left,
                window_rect.bottom - window_rect.top) != root_view_size) {
    // If the size of the window differs from the size of the root view it
    // means we're being asked to paint before we've gotten a WM_SIZE. This can
    // happen when the user is interactively resizing the window. To avoid
    // mass flickering we don't do anything here. Once we get the WM_SIZE we'll
    // reset the region of the window which triggers another WM_NCPAINT and
    // all is well.
    return;
  }
  delegate_->HandlePaintAccelerated(gfx::Rect(dirty_region));

  // When using a custom frame, we want to avoid calling DefWindowProc() since
  // that may render artifacts.
  SetMsgHandled(delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN);
}

LRESULT HWNDMessageHandler::OnNCUAHDrawCaption(UINT message,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN);
  return 0;
}

LRESULT HWNDMessageHandler::OnNCUAHDrawFrame(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN);
  return 0;
}

void HWNDMessageHandler::OnPaint(HDC dc) {
  // Call BeginPaint()/EndPaint() around the paint handling, as that seems
  // to do more to actually validate the window's drawing region. This only
  // appears to matter for Windows that have the WS_EX_COMPOSITED style set
  // but will be valid in general too.
  PAINTSTRUCT ps;
  HDC display_dc = BeginPaint(hwnd(), &ps);

  if (!display_dc) {
    // Failing to get a DC during BeginPaint() means we won't be able to
    // actually get any pixels to the screen and is very bad. This is often
    // caused by handle exhaustion. Collecting some GDI statistics may aid
    // tracking down the cause.
    base::debug::CollectGDIUsageAndDie();
  }

  if (!IsRectEmpty(&ps.rcPaint)) {
    HBRUSH brush = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (HasChildRenderingWindow()) {
      // If there's a child window that's being rendered to then clear the
      // area outside it (as WS_CLIPCHILDREN is set) with transparent black.
      // Otherwise, other portions of the backing store for the window can
      // flicker opaque black. http://crbug.com/586454

      FillRect(ps.hdc, &ps.rcPaint, brush);
    } else if (exposed_pixels_ != gfx::Size()) {
      // Fill in newly exposed window client area with black to ensure Windows
      // doesn't put something else there (eg. copying existing pixels). This
      // isn't needed if we've just cleared the whole client area outside the
      // child window above.
      RECT cr;
      if (GetClientRect(hwnd(), &cr)) {
        // GetClientRect() always returns a rect with top/left at 0.
        const gfx::Size client_area = gfx::Rect(cr).size();

        // It's possible that |exposed_pixels_| height and/or width is larger
        // than the client area if the window frame size changed. This isn't an
        // issue since FillRect() is clipped by |ps.rcPaint|.
        if (exposed_pixels_.height() > 0) {
          RECT rect = {0, client_area.height() - exposed_pixels_.height(),
                       client_area.width(), client_area.height()};
          FillRect(ps.hdc, &rect, brush);
        }
        if (exposed_pixels_.width() > 0) {
          RECT rect = {client_area.width() - exposed_pixels_.width(), 0,
                       client_area.width(),
                       client_area.height() - exposed_pixels_.height()};
          FillRect(ps.hdc, &rect, brush);
        }
      }
    }

    delegate_->HandlePaintAccelerated(gfx::Rect(ps.rcPaint));
  }

  exposed_pixels_ = gfx::Size();

  EndPaint(hwnd(), &ps);
}

LRESULT HWNDMessageHandler::OnReflectedMessage(UINT message,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT HWNDMessageHandler::OnScrollMessage(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  CHROME_MSG msg = {hwnd(), message, w_param, l_param,
                    static_cast<DWORD>(GetMessageTime())};
  ui::ScrollEvent event(msg);
  delegate_->HandleScrollEvent(&event);
  return 0;
}

LRESULT HWNDMessageHandler::OnSetCursor(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  // Ignore system generated cursors likely caused by window management mouse
  // moves while the pen is active over the window client area.
  if (is_pen_active_in_client_area_)
    return 1;

  // current_cursor_ must be a ui::WinCursor, so that custom image cursors are
  // properly ref-counted. cursor below is only used for system cursors and
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
      SetCursor(current_cursor_);
      return 1;
    case LOWORD(HTERROR):  // Use HTERROR's LOWORD value for valid comparison.
      SetMsgHandled(FALSE);
      break;
    default:
      // Use the default value, IDC_ARROW.
      break;
  }
  ::SetCursor(LoadCursor(nullptr, cursor));
  return 1;
}

void HWNDMessageHandler::OnSetFocus(HWND last_focused_window) {
  // Headless windows are believed to always have focus, so avoid
  // reporting native focus changes.
  if (!IsHeadless()) {
    delegate_->HandleNativeFocus(last_focused_window);
  }
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnSetIcon(UINT size_type, HICON new_icon) {
  // Use a ScopedRedrawLock to avoid weird non-client painting.
  return DefWindowProcWithRedrawLock(WM_SETICON, size_type,
                                     reinterpret_cast<LPARAM>(new_icon));
}

LRESULT HWNDMessageHandler::OnSetText(const wchar_t* text) {
  // Use a ScopedRedrawLock to avoid weird non-client painting.
  return DefWindowProcWithRedrawLock(WM_SETTEXT, NULL,
                                     reinterpret_cast<LPARAM>(text));
}

void HWNDMessageHandler::OnSettingChange(UINT flags, const wchar_t* section) {
  if (!GetParent(hwnd()) && (flags == SPI_SETWORKAREA)) {
    // Fire a dummy SetWindowPos() call, so we'll trip the code in
    // OnWindowPosChanging() below that notices work area changes.
    ::SetWindowPos(hwnd(), nullptr, 0, 0, 0, 0,
                   SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW |
                       SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    SetMsgHandled(TRUE);
  } else {
    if (flags == SPI_SETWORKAREA)
      delegate_->HandleWorkAreaChanged();
    SetMsgHandled(FALSE);
  }

  // If the work area is changing, then it could be as a result of the taskbar
  // broadcasting the WM_SETTINGCHANGE message due to changes in auto hide
  // settings, etc. Force a WM_NCCALCSIZE to occur to ensure that we handle
  // this correctly.
  if (flags == SPI_SETWORKAREA)
    SendFrameChanged();
}

void HWNDMessageHandler::OnSize(UINT param, const gfx::Size& size) {
  if (DidMinimizedChange(last_size_param_, param) && IsTopLevelWindow(hwnd()))
    delegate_->HandleWindowMinimizedOrRestored(param != SIZE_MINIMIZED);
  last_size_param_ = param;

  RedrawWindow(hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been laid out.
  ResetWindowRegion(false, true);
}

void HWNDMessageHandler::OnSizing(UINT param, RECT* rect) {
  // If the aspect ratio was not specified for the window, do nothing.
  if (!aspect_ratio_.has_value())
    return;

  // Validate the window edge param because we are seeing DCHECKs caused by
  // invalid values. See https://crbug.com/1418231.
  if (param < WMSZ_LEFT || param > WMSZ_BOTTOMRIGHT) {
    return;
  }
  gfx::Rect window_rect(*rect);
  SizeWindowToAspectRatio(param, &window_rect);

  // TODO(apacible): Account for window borders as part of the aspect ratio.
  // https://crbug/869487.
  *rect = window_rect.ToRECT();
}

void HWNDMessageHandler::OnSysCommand(UINT notification_code,
                                      const gfx::Point& point) {
  // Windows uses the 4 lower order bits of |notification_code| for type-
  // specific information so we must exclude this when comparing.
  static const int sc_mask = 0xFFF0;
  // Ignore size/move/maximize in fullscreen mode.
  if (IsFullscreen() && (((notification_code & sc_mask) == SC_SIZE) ||
                         ((notification_code & sc_mask) == SC_MOVE) ||
                         ((notification_code & sc_mask) == SC_MAXIMIZE)))
    return;

  const bool window_control_action =
      (notification_code & sc_mask) == SC_MINIMIZE ||
      (notification_code & sc_mask) == SC_MAXIMIZE ||
      (notification_code & sc_mask) == SC_RESTORE;
  const bool custom_controls_frame_mode =
      delegate_->GetFrameMode() == FrameMode::SYSTEM_DRAWN_NO_CONTROLS ||
      delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN;
  if (custom_controls_frame_mode && window_control_action)
    delegate_->ResetWindowControls();

  if (delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    const bool window_bounds_change =
        (notification_code & sc_mask) == SC_MOVE ||
        (notification_code & sc_mask) == SC_SIZE;
    if (window_bounds_change || window_control_action)
      DestroyAXSystemCaret();
    if (window_bounds_change && !IsVisible()) {
      // Circumvent ScopedRedrawLocks and force visibility before entering a
      // resize or move modal loop to get continuous sizing/moving feedback.
      SetWindowLong(hwnd(), GWL_STYLE,
                    GetWindowLong(hwnd(), GWL_STYLE) | WS_VISIBLE);
    }
  }

  // Handle SC_KEYMENU, which means that the user has pressed the ALT
  // key and released it, so we should focus the menu bar.
  if ((notification_code & sc_mask) == SC_KEYMENU && point.x() == 0) {
    int modifiers = ui::EF_NONE;
    if (ui::win::IsShiftPressed())
      modifiers |= ui::EF_SHIFT_DOWN;
    if (ui::win::IsCtrlPressed())
      modifiers |= ui::EF_CONTROL_DOWN;
    // Retrieve the status of shift and control keys to prevent consuming
    // shift+alt keys, which are used by Windows to change input languages.
    ui::Accelerator accelerator(ui::KeyboardCodeForWindowsKeyCode(VK_MENU),
                                modifiers);
    delegate_->HandleAccelerator(accelerator);
    return;
  }

  if (delegate_->HandleCommand(static_cast<int>(notification_code)))
    return;

  bool is_mouse_menu = (notification_code & sc_mask) == SC_MOUSEMENU;
  if (is_mouse_menu) {
    // `handling_mouse_menu_` set/reset here and below isn't reentrancy safe but
    // we assume the nested native loop running as part of DefWindowProc() will
    // not trigger a nested SC_MOUSEMENU as that's not possible in practice.
    CHECK(!handling_mouse_menu_);
    handling_mouse_menu_ = true;
  }

  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  // Since redraws occur in drag-induced nested message loops which occur here,
  // application tasks need to run. This is safe because HWNDMessageHandler
  // should be in the only C++ frame on the stack and is reentrancy safe in this
  // situation.
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
  // If the delegate can't handle it, the system implementation will be called.
  DefWindowProc(hwnd(), WM_SYSCOMMAND, notification_code,
                MAKELPARAM(point.x(), point.y()));
  if (is_mouse_menu && ref)
    handling_mouse_menu_ = false;
}

void HWNDMessageHandler::OnThemeChanged() {
  ui::NativeThemeWin::CloseHandles();
}

void HWNDMessageHandler::OnTimeChange() {
  // Call NowFromSystemTime() to force base::Time to re-initialize the clock
  // from system time. Otherwise base::Time::Now() might continue to reflect the
  // old system clock for some amount of time. See https://crbug.com/672906#c5
  base::Time::NowFromSystemTime();
}

LRESULT HWNDMessageHandler::OnTouchEvent(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  if (pointer_events_for_touch_) {
    // Release any associated memory with this event.
    CloseTouchInputHandle(reinterpret_cast<HTOUCHINPUT>(l_param));

    // Claim the event is handled. This shouldn't ever happen
    // because we don't register touch windows when we are using
    // pointer events.
    return 0;
  }

  // Handle touch events only on Aura for now.
  WORD num_points = LOWORD(w_param);
  base::HeapArray<TOUCHINPUT> input =
      base::HeapArray<TOUCHINPUT>::WithSize(num_points);
  if (ui::GetTouchInputInfoWrapper(reinterpret_cast<HTOUCHINPUT>(l_param),
                                   num_points, input.data(),
                                   sizeof(TOUCHINPUT))) {
    // input[i].dwTime doesn't necessarily relate to the system time at all,
    // so use base::TimeTicks::Now().
    const base::TimeTicks event_time = base::TimeTicks::Now();
    TouchEvents touch_events;
    TouchIDs stale_touches(touch_ids_);

    for (size_t i = 0; i < num_points; ++i) {
      stale_touches.erase(input[i].dwID);
      POINT point;
      point.x = TOUCH_COORD_TO_PIXEL(input[i].x);
      point.y = TOUCH_COORD_TO_PIXEL(input[i].y);
      ScreenToClient(hwnd(), &point);

      last_touch_or_pen_message_time_ = ::GetMessageTime();

      gfx::Point touch_point(point.x, point.y);
      auto touch_id = static_cast<ui::PointerId>(
          id_generator_.GetGeneratedID(input[i].dwID));

      if (input[i].dwFlags & TOUCHEVENTF_DOWN) {
        touch_ids_.insert(input[i].dwID);
        GenerateTouchEvent(ui::EventType::kTouchPressed, touch_point, touch_id,
                           event_time, &touch_events);
        ui::ComputeEventLatencyOSFromTOUCHINPUT(ui::EventType::kTouchPressed,
                                                input[i], event_time);
        touch_down_contexts_++;
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&HWNDMessageHandler::ResetTouchDownContext,
                           msg_handler_weak_factory_.GetWeakPtr()),
            kTouchDownContextResetTimeout);
      } else {
        if (input[i].dwFlags & TOUCHEVENTF_MOVE) {
          GenerateTouchEvent(ui::EventType::kTouchMoved, touch_point, touch_id,
                             event_time, &touch_events);
          ui::ComputeEventLatencyOSFromTOUCHINPUT(ui::EventType::kTouchMoved,
                                                  input[i], event_time);
        }

        if (input[i].dwFlags & TOUCHEVENTF_UP) {
          touch_ids_.erase(input[i].dwID);
          GenerateTouchEvent(ui::EventType::kTouchReleased, touch_point,
                             touch_id, event_time, &touch_events);
          ui::ComputeEventLatencyOSFromTOUCHINPUT(ui::EventType::kTouchReleased,
                                                  input[i], event_time);
          id_generator_.ReleaseNumber(input[i].dwID);
        }
      }
    }
    // If a touch has been dropped from the list (without a TOUCH_EVENTF_UP)
    // we generate a simulated TOUCHEVENTF_UP event.
    for (auto touch_number : stale_touches) {
      // Log that we've hit this code. When usage drops off, we can remove
      // this "workaround". See https://crbug.com/811273
      UMA_HISTOGRAM_BOOLEAN("TouchScreen.MissedTOUCHEVENTF_UP", true);
      auto touch_id = static_cast<ui::PointerId>(
          id_generator_.GetGeneratedID(touch_number));
      touch_ids_.erase(touch_number);
      GenerateTouchEvent(ui::EventType::kTouchReleased, gfx::Point(0, 0),
                         touch_id, event_time, &touch_events);
      id_generator_.ReleaseNumber(touch_number);
    }

    // Handle the touch events asynchronously. We need this because touch
    // events on windows don't fire if we enter a modal loop in the context of
    // a touch event.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&HWNDMessageHandler::HandleTouchEvents,
                       msg_handler_weak_factory_.GetWeakPtr(), touch_events));
  }
  CloseTouchInputHandle(reinterpret_cast<HTOUCHINPUT>(l_param));
  SetMsgHandled(FALSE);
  return 0;
}

void HWNDMessageHandler::OnWindowPosChanging(WINDOWPOS* window_pos) {
  TRACE_EVENT0("ui", "HWNDMessageHandler::OnWindowPosChanging");

  if (ignore_window_pos_changes_) {
    // If somebody's trying to toggle our visibility, change the nonclient area,
    // change our Z-order, or activate us, we should probably let it go through.
    if (!(window_pos->flags & ((IsVisible() ? SWP_HIDEWINDOW : SWP_SHOWWINDOW) |
                               SWP_FRAMECHANGED)) &&
        (window_pos->flags & (SWP_NOZORDER | SWP_NOACTIVATE))) {
      // Just sizing/moving the window; ignore.
      window_pos->flags |= SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW;
      window_pos->flags &=
          static_cast<unsigned int>(~(SWP_SHOWWINDOW | SWP_HIDEWINDOW));
    }
  } else if (!GetParent(hwnd())) {
    RECT window_rect;
    const bool have_new_window_rect =
        !(window_pos->flags & SWP_NOMOVE) && !(window_pos->flags & SWP_NOSIZE);
    if (have_new_window_rect) {
      // We should use new window rect for detecting monitor and it's
      // parameters, if it is available. If we use |GetWindowRect()| instead,
      // we can break our same monitor detection logic (see |same_monitor|
      // below) and consequently Windows "Move to other monitor" shortcuts
      // (Win+Shift+Arrows). See crbug.com/656001.
      window_rect.left = window_pos->x;
      window_rect.top = window_pos->y;
      window_rect.right = window_pos->x + window_pos->cx;
      window_rect.bottom = window_pos->y + window_pos->cy;
    }

    HMONITOR monitor;
    gfx::Rect monitor_rect, work_area;
    if ((have_new_window_rect || GetWindowRect(hwnd(), &window_rect)) &&
        GetMonitorAndRects(window_rect, &monitor, &monitor_rect, &work_area)) {
      bool work_area_changed = (monitor_rect == last_monitor_rect_) &&
                               (work_area != last_work_area_);
      const bool same_monitor = monitor && (monitor == last_monitor_);

      gfx::Rect expected_maximized_bounds = work_area;
      if (IsMaximized()) {
        // Windows automatically adds a standard width border to all sides when
        // window is maximized. We should take this into account.
        gfx::Insets client_area_insets;
        if (GetClientAreaInsets(&client_area_insets, monitor))
          // Ceil the insets after scaling to make them exclude fractional parts
          // after scaling, since the result is negative.
          expected_maximized_bounds.Inset(
              gfx::ScaleToCeiledInsets(client_area_insets, -1));
      }
      // Sometimes Windows incorrectly changes bounds of maximized windows after
      // attaching or detaching additional displays. In this case user can see
      // non-client area of the window (that should be hidden in normal case).
      // We should restore window position if problem occurs.
      const bool incorrect_maximized_bounds =
          IsMaximized() && have_new_window_rect &&
          (expected_maximized_bounds.x() != window_pos->x ||
           expected_maximized_bounds.y() != window_pos->y ||
           expected_maximized_bounds.width() != window_pos->cx ||
           expected_maximized_bounds.height() != window_pos->cy);

      // If the size of a background fullscreen window changes again, then we
      // should reset the |background_fullscreen_hack_| flag.
      if (background_fullscreen_hack_ &&
          (!(window_pos->flags & SWP_NOSIZE) &&
           (monitor_rect.height() - window_pos->cy != 1))) {
        background_fullscreen_hack_ = false;
      }
      const bool fullscreen_without_hack =
          IsFullscreen() && !background_fullscreen_hack_;

      // If the browser window is arranged by Snap, then we should not change
      // its position but let Windows do it.
      if (same_monitor &&
          (incorrect_maximized_bounds || fullscreen_without_hack ||
           work_area_changed) &&
          !IsWindowArranged(hwnd())) {
        // A rect for the monitor we're on changed.  Normally Windows notifies
        // us about this (and thus we're reaching here due to the SetWindowPos()
        // call in OnSettingChange() above), but with some software (e.g.
        // nVidia's nView desktop manager) the work area can change asynchronous
        // to any notification, and we're just sent a SetWindowPos() call with a
        // new (frequently incorrect) position/size.  In either case, the best
        // response is to throw away the existing position/size information in
        // |window_pos| and recalculate it based on the new work rect.
        gfx::Rect new_window_rect;
        if (IsFullscreen()) {
          new_window_rect = monitor_rect;
        } else if (IsMaximized()) {
          new_window_rect = expected_maximized_bounds;
        } else {
          new_window_rect = gfx::Rect(window_rect);
          new_window_rect.AdjustToFit(work_area);
        }
        window_pos->x = new_window_rect.x();
        window_pos->y = new_window_rect.y();
        window_pos->cx = new_window_rect.width();
        window_pos->cy = new_window_rect.height();
        // WARNING!  Don't set SWP_FRAMECHANGED here, it breaks moving the child
        // HWNDs for some reason.
        window_pos->flags &= static_cast<unsigned int>(
            ~(SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW));
        window_pos->flags |= SWP_NOCOPYBITS;

        // Now ignore all immediately-following SetWindowPos() changes.  Windows
        // likes to (incorrectly) recalculate what our position/size should be
        // and send us further updates.
        ignore_window_pos_changes_ = true;
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&HWNDMessageHandler::StopIgnoringPosChanges,
                           msg_handler_weak_factory_.GetWeakPtr()));
      }
      last_monitor_ = monitor;
      last_monitor_rect_ = monitor_rect;
      last_work_area_ = work_area;
    }
  }

  RECT window_rect;
  gfx::Size old_size;
  if (GetWindowRect(hwnd(), &window_rect))
    old_size = gfx::Rect(window_rect).size();
  gfx::Size new_size = gfx::Size(window_pos->cx, window_pos->cy);
  if ((old_size != new_size && !(window_pos->flags & SWP_NOSIZE)) ||
      window_pos->flags & SWP_FRAMECHANGED) {
    // If the window is getting larger then fill the exposed area on the next
    // WM_PAINT.
    exposed_pixels_ = new_size - old_size;

    delegate_->HandleWindowSizeChanging();
    sent_window_size_changing_ = true;

    // It's possible that if Aero snap is being entered then the window size
    // won't actually change. Post a message to ensure swaps will be re-enabled
    // in that case.
    PostMessage(hwnd(), WM_WINDOWSIZINGFINISHED, ++current_window_size_message_,
                0);
    // Copying the old bits can sometimes cause a flash of black when
    // resizing. See https://crbug.com/739724
    if (is_translucent_)
      window_pos->flags |= SWP_NOCOPYBITS;
  } else {
    // The window size isn't changing so there are no exposed pixels.
    exposed_pixels_ = gfx::Size();
  }

  if (ScopedFullscreenVisibility::IsHiddenForFullscreen(hwnd())) {
    // Prevent the window from being made visible if we've been asked to do so.
    // See comment in header as to why we might want this.
    window_pos->flags &= static_cast<unsigned int>(~SWP_SHOWWINDOW);
  }

  if (window_pos->flags & SWP_HIDEWINDOW)
    SetDwmFrameExtension(DwmFrameState::kOff);

  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnWindowPosChanged(WINDOWPOS* window_pos) {
  TRACE_EVENT0("ui", "HWNDMessageHandler::OnWindowPosChanged");

  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  if (DidClientAreaSizeChange(window_pos))
    ClientAreaSizeChanged();
  if (!ref)
    return;
  if (window_pos->flags & SWP_FRAMECHANGED)
    SetDwmFrameExtension(DwmFrameState::kOn);
  if (window_pos->flags & SWP_SHOWWINDOW) {
    delegate_->HandleVisibilityChanged(true);
    SetDwmFrameExtension(DwmFrameState::kOn);
  } else if (window_pos->flags & SWP_HIDEWINDOW) {
    delegate_->HandleVisibilityChanged(false);
  }
  UpdateDwmFrame();
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnWindowSizingFinished(UINT message,
                                                   WPARAM w_param,
                                                   LPARAM l_param) {
  // Check if a newer WM_WINDOWPOSCHANGING or WM_WINDOWPOSCHANGED have been
  // received after this message was posted.
  if (current_window_size_message_ != w_param)
    return 0;
  delegate_->HandleWindowSizeUnchanged();
  sent_window_size_changing_ = false;

  // The window size didn't actually change, so nothing was exposed that needs
  // to be filled black.
  exposed_pixels_ = gfx::Size();

  return 0;
}

void HWNDMessageHandler::OnSessionChange(WPARAM status_code,
                                         const bool* is_current_session) {
  // Direct3D presents are ignored while the screen is locked, so force the
  // window to be redrawn on unlock.
  if (status_code == WTS_SESSION_UNLOCK)
    ForceRedrawWindow(10);
}

void HWNDMessageHandler::HandleTouchEvents(const TouchEvents& touch_events) {
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  for (size_t i = 0; i < touch_events.size() && ref; ++i)
    delegate_->HandleTouchEvent(const_cast<ui::TouchEvent*>(&touch_events[i]));
}

void HWNDMessageHandler::ResetTouchDownContext() {
  touch_down_contexts_--;
}

LRESULT HWNDMessageHandler::HandleMouseEventInternal(UINT message,
                                                     WPARAM w_param,
                                                     LPARAM l_param,
                                                     bool track_mouse) {
  // Ignore system generated mouse messages while the pen is active over the
  // window client area.
  if (is_pen_active_in_client_area_) {
    INPUT_MESSAGE_SOURCE input_message_source;
    static const auto get_current_input_message_source =
        reinterpret_cast<decltype(&::GetCurrentInputMessageSource)>(
            base::win::GetUser32FunctionPointer(
                "GetCurrentInputMessageSource"));
    if (get_current_input_message_source &&
        get_current_input_message_source(&input_message_source) &&
        input_message_source.deviceType == IMDT_UNAVAILABLE) {
      return 0;
    }
    is_pen_active_in_client_area_ = false;
  }

  // We handle touch events in Aura. Windows generates synthesized mouse
  // messages whenever there's a touch, but it doesn't give us the actual touch
  // messages if it thinks the touch point is in non-client space.
  if (message != WM_MOUSEWHEEL && message != WM_MOUSEHWHEEL &&
      ui::IsMouseEventFromTouch(message)) {
    LRESULT hittest = SendMessage(hwnd(), WM_NCHITTEST, 0, l_param);
    // Always DefWindowProc on the titlebar. We could let the event fall through
    // and the special handling in HandleMouseInputForCaption would take care of
    // this, but in the touch case Windows does a better job.
    if (hittest == HTCAPTION || hittest == HTSYSMENU)
      SetMsgHandled(FALSE);
    // We must let Windows handle the caption buttons if it's drawing them, or
    // they won't work.
    if (delegate_->GetFrameMode() == FrameMode::SYSTEM_DRAWN &&
        (hittest == HTCLOSE || hittest == HTMINBUTTON ||
         hittest == HTMAXBUTTON)) {
      SetMsgHandled(FALSE);
    }
    // Let resize events fall through. Ignore everything else, as we're either
    // letting Windows handle it above or we've already handled the equivalent
    // touch message.
    if (!IsHitTestOnResizeHandle(hittest))
      return 0;
  }

  // Certain logitech drivers send the WM_MOUSEHWHEEL message to the parent
  // followed by WM_MOUSEWHEEL messages to the child window causing a vertical
  // scroll. We treat these WM_MOUSEWHEEL messages as WM_MOUSEHWHEEL
  // messages.
  if (message == WM_MOUSEHWHEEL)
    last_mouse_hwheel_time_ = ::GetMessageTime();

  if (message == WM_MOUSEWHEEL &&
      ::GetMessageTime() == last_mouse_hwheel_time_) {
    message = WM_MOUSEHWHEEL;
  }

  if (message == WM_RBUTTONUP && is_right_mouse_pressed_on_caption_) {
    // TODO(pkasting): Maybe handle this in DesktopWindowTreeHostWin, where we
    // handle alt-space, or in the frame itself.
    is_right_mouse_pressed_on_caption_ = false;
    ReleaseCapture();
    // |point| is in window coordinates, but WM_NCHITTEST and TrackPopupMenu()
    // expect screen coordinates.
    POINT screen_point = CR_POINT_INITIALIZER_FROM_LPARAM(l_param);
    MapWindowPoints(hwnd(), HWND_DESKTOP, &screen_point, 1);
    w_param = static_cast<WPARAM>(SendMessage(
        hwnd(), WM_NCHITTEST, 0, MAKELPARAM(screen_point.x, screen_point.y)));
    if (w_param == HTCAPTION || w_param == HTSYSMENU) {
      ShowSystemMenuAtScreenPixelLocation(hwnd(), gfx::Point(screen_point));
      return 0;
    }
  } else if (message == WM_NCLBUTTONDOWN &&
             delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    switch (w_param) {
      case HTCLOSE:
      case HTMINBUTTON:
      case HTMAXBUTTON: {
        // When the mouse is pressed down in these specific non-client areas,
        // we need to tell the RootView to send the mouse pressed event (which
        // sets capture, allowing subsequent WM_LBUTTONUP (note, _not_
        // WM_NCLBUTTONUP) to fire so that the appropriate WM_SYSCOMMAND can be
        // sent by the applicable button's callback. We _have_ to do this this
        // way rather than letting Windows just send the syscommand itself (as
        // would happen if we never did this dance) because for some insane
        // reason DefWindowProc for WM_NCLBUTTONDOWN also renders the pressed
        // window control button appearance, in the Windows classic style, over
        // our view! Ick! By handling this message we prevent Windows from
        // doing this undesirable thing, but that means we need to roll the
        // sys-command handling ourselves.
        // Combine |w_param| with common key state message flags.
        w_param |= ui::win::IsCtrlPressed() ? MK_CONTROL : 0;
        w_param |= ui::win::IsShiftPressed() ? MK_SHIFT : 0;
      }
    }
  } else if (message == WM_NCRBUTTONDOWN &&
             (w_param == HTCAPTION || w_param == HTSYSMENU)) {
    is_right_mouse_pressed_on_caption_ = true;
    // We SetCapture() to ensure we only show the menu when the button
    // down and up are both on the caption. Note: this causes the button up to
    // be WM_RBUTTONUP instead of WM_NCRBUTTONUP.
    SetCapture();
  }

  LONG message_time = GetMessageTime();
  CHROME_MSG msg = {hwnd(),
                    message,
                    w_param,
                    l_param,
                    static_cast<DWORD>(message_time),
                    {CR_GET_X_LPARAM(l_param), CR_GET_Y_LPARAM(l_param)}};
  ui::MouseEvent event(msg);
  if (IsSynthesizedMouseMessage(message, message_time, l_param))
    event.SetFlags(event.flags() | ui::EF_FROM_TOUCH);

  if (event.type() == ui::EventType::kMouseMoved && !HasCapture() &&
      track_mouse) {
    // Windows only fires WM_MOUSELEAVE events if the application begins
    // "tracking" mouse events for a given HWND during WM_MOUSEMOVE events.
    // We need to call |TrackMouseEvents| to listen for WM_MOUSELEAVE.
    TrackMouseEvents((message == WM_NCMOUSEMOVE) ? TME_NONCLIENT | TME_LEAVE
                                                 : TME_LEAVE);
  } else if (event.type() == ui::EventType::kMouseExited) {
    // Reset our tracking flags so future mouse movement over this
    // NativeWidget results in a new tracking session. Fall through for
    // OnMouseEvent.
    active_mouse_tracking_flags_ = 0;
  } else if (event.type() == ui::EventType::kMousewheel) {
    ui::MouseWheelEvent mouse_wheel_event(msg);
    // Reroute the mouse wheel to the window under the pointer if applicable.
    return (ui::RerouteMouseWheel(hwnd(), w_param, l_param) ||
            delegate_->HandleMouseEvent(&mouse_wheel_event))
               ? 0
               : 1;
  }

  // Suppress |EventType::kMouseMoved| and |EventType::kMouseDragged| events
  // from WM_MOUSE* messages when using WM_INPUT.
  if (using_wm_input_ && (event.type() == ui::EventType::kMouseMoved ||
                          event.type() == ui::EventType::kMouseDragged)) {
    return 0;
  }

  // There are cases where the code handling the message destroys the window,
  // so use the weak ptr to check if destruction occurred or not.
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  bool handled = false;

  if (event.type() == ui::EventType::kMouseDragged) {
    constexpr int kMaxDragEventsToIgnore00Move = 6;
    POINT point;
    point.x = event.x();
    point.y = event.y();
    ::ClientToScreen(hwnd(), &point);
    num_drag_events_after_press_++;
    // Windows sometimes sends spurious WM_MOUSEMOVEs at 0,0. If this happens
    // after a mouse down on a tab, it can cause a detach of the tab to 0,0.
    // A past study indicated that most of the spurious moves are among the
    // first 6 moves after a press, and there's a very long tail.
    // If we get kMaxDragEventsToIgnore00Move mouse move events before the
    // spurious 0,0 move event, the user is probably really dragging, and we
    // want to allow moves to 0,0.
    if (point.x == 0 && point.y == 0 &&
        num_drag_events_after_press_ <= kMaxDragEventsToIgnore00Move) {
      POINT cursor_pos;
      ::GetCursorPos(&cursor_pos);

      // This constant tries to balance between detecting valid moves to 0,0
      // and avoiding the detach bug happening to tabs near 0,0.
      constexpr int kMinSpuriousDistance = 30;
      auto distance = sqrt(pow(static_cast<float>(abs(cursor_pos.x)), 2) +
                           pow(static_cast<float>(abs(cursor_pos.y)), 2));
      if (distance > kMinSpuriousDistance) {
        SetMsgHandled(true);
        return 0;
      }
    }
  } else if (event.type() == ui::EventType::kMousePressed) {
    num_drag_events_after_press_ = 0;
  }

  // Don't send right mouse button up to the delegate when displaying system
  // command menu. This prevents left clicking in the upper left hand corner of
  // an app window and then right clicking from sending the right click to the
  // renderer and bringing up a web contents context menu.
  if (!handling_mouse_menu_ || message != WM_RBUTTONUP)
    handled = delegate_->HandleMouseEvent(&event);

  if (!ref.get())
    return 0;

  if (!handled && message == WM_NCLBUTTONDOWN && w_param != HTSYSMENU &&
      w_param != HTCAPTION &&
      delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
    // TODO(msw): Eliminate undesired painting, or re-evaluate this workaround.
    // DefWindowProc for WM_NCLBUTTONDOWN does weird non-client painting, so we
    // need to call it inside a ScopedRedrawLock. This may cause other negative
    // side-effects (ex/ stifling non-client mouse releases).
    DefWindowProcWithRedrawLock(message, w_param, l_param);
    handled = true;
  }

  // We need special processing for mouse input on the caption.
  // Please refer to the HandleMouseInputForCaption() function for more
  // information.
  if (!handled)
    handled = HandleMouseInputForCaption(message, w_param, l_param);

  if (ref.get())
    SetMsgHandled(handled);
  return 0;
}

LRESULT HWNDMessageHandler::HandlePointerEventTypeTouchOrNonClient(
    UINT message, WPARAM w_param, LPARAM l_param) {
  UINT32 pointer_id = GET_POINTERID_WPARAM(w_param);
  using GetPointerTouchInfoFn = BOOL(WINAPI*)(UINT32, POINTER_TOUCH_INFO*);
  POINTER_TOUCH_INFO pointer_touch_info;
  static const auto get_pointer_touch_info =
      reinterpret_cast<GetPointerTouchInfoFn>(
          base::win::GetUser32FunctionPointer("GetPointerTouchInfo"));
  if (!get_pointer_touch_info ||
      !get_pointer_touch_info(pointer_id, &pointer_touch_info)) {
    SetMsgHandled(FALSE);
    return -1;
  }

  last_touch_or_pen_message_time_ = ::GetMessageTime();
  // Ignore enter/leave events, otherwise they will be converted in
  // |GetTouchEventType| to EventType::kTouchPressed/EventType::kTouchReleased
  // events, which is not correct.
  if (message == WM_POINTERENTER || message == WM_POINTERLEAVE) {
    SetMsgHandled(TRUE);
    return 0;
  }

  // Increment |touch_down_contexts_| on a pointer down. This variable
  // is used to debounce the WM_MOUSEACTIVATE events.
  if (message == WM_POINTERDOWN || message == WM_NCPOINTERDOWN) {
    touch_down_contexts_++;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HWNDMessageHandler::ResetTouchDownContext,
                       msg_handler_weak_factory_.GetWeakPtr()),
        kTouchDownContextResetTimeout);
  }

  POINTER_INFO pointer_info = pointer_touch_info.pointerInfo;
  POINTER_FLAGS pointer_flags = pointer_info.pointerFlags;

  // When there are touch move events but no finger is pressing on the screen,
  // which most likely happens for smart board, we should ignore these events
  // for now. POINTER_FLAG_INCONTACT indicates this pointer is in contact with
  // the digitizer surface, which means pressing the screen.
  if ((message == WM_POINTERUPDATE) &&
      !(pointer_flags & POINTER_FLAG_INCONTACT)) {
    SetMsgHandled(TRUE);
    return 0;
  }

  POINT client_point = pointer_info.ptPixelLocationRaw;
  ScreenToClient(hwnd(), &client_point);
  gfx::Point touch_point = gfx::Point(client_point.x, client_point.y);
  ui::EventType event_type = GetTouchEventType(pointer_flags);
  const base::TimeTicks event_time = ui::EventTimeForNow();
  auto mapped_pointer_id =
      static_cast<ui::PointerId>(id_generator_.GetGeneratedID(pointer_id));

  // The pressure from POINTER_TOUCH_INFO is normalized to a range between 0
  // and 1024, but we define the pressure of the range of [0,1].
  float pressure = static_cast<float>(pointer_touch_info.pressure) / 1024;
  float radius_x =
      (pointer_touch_info.rcContact.right - pointer_touch_info.rcContact.left) /
      2.0;
  float radius_y =
      (pointer_touch_info.rcContact.bottom - pointer_touch_info.rcContact.top) /
      2.0;
  auto rotation_angle = static_cast<int>(pointer_touch_info.orientation);
  rotation_angle %= 180;
  if (rotation_angle < 0)
    rotation_angle += 180;

  ui::TouchEvent event(
      event_type, touch_point, event_time,
      ui::PointerDetails(ui::EventPointerType::kTouch, mapped_pointer_id,
                         radius_x, radius_y, pressure, rotation_angle),
      ui::GetModifiersFromKeyState());
  ui::ComputeEventLatencyOSFromPOINTER_INFO(event_type, pointer_info,
                                            event_time);
  event.latency()->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, event_time);

  // Release the pointer id for touch release events.
  if (event_type == ui::EventType::kTouchReleased) {
    id_generator_.ReleaseNumber(pointer_id);
  }

  // There are cases where the code handling the message destroys the
  // window, so use the weak ptr to check if destruction occurred or not.
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  delegate_->HandleTouchEvent(&event);

  if (ref) {
    // Mark touch released events handled. These will usually turn into tap
    // gestures, and doing this avoids propagating the event to other windows.
    if (delegate_->GetFrameMode() == FrameMode::SYSTEM_DRAWN) {
      // WM_NCPOINTERUP must be DefWindowProc'ed in order for the system caption
      // buttons to work correctly.
      if (message == WM_POINTERUP)
        event.SetHandled();
    } else {
      // Messages on HTCAPTION should be DefWindowProc'ed, as we let Windows
      // take care of dragging the window and double-tapping to maximize.
      const bool on_titlebar =
          SendMessage(hwnd(), WM_NCHITTEST, 0, l_param) == HTCAPTION;
      // Unlike above, we must mark both WM_POINTERUP and WM_NCPOINTERUP as
      // handled, in order for the custom caption buttons to work correctly.
      if (event_type == ui::EventType::kTouchReleased && !on_titlebar) {
        event.SetHandled();
      }
    }
    SetMsgHandled(event.handled());
  }
  return 0;
}

LRESULT HWNDMessageHandler::HandlePointerEventTypePen(
    UINT message,
    UINT32 pointer_id,
    POINTER_PEN_INFO pointer_pen_info) {
  POINT client_point = pointer_pen_info.pointerInfo.ptPixelLocationRaw;
  ScreenToClient(hwnd(), &client_point);
  gfx::Point point = gfx::Point(client_point.x, client_point.y);

  std::unique_ptr<ui::Event> event = pen_processor_.GenerateEvent(
      message, pointer_id, pointer_pen_info, point);

  // There are cases where the code handling the message destroys the
  // window, so use the weak ptr to check if destruction occurred or not.
  base::WeakPtr<HWNDMessageHandler> ref(msg_handler_weak_factory_.GetWeakPtr());
  if (event) {
    if (event->IsTouchEvent()) {
      delegate_->HandleTouchEvent(event->AsTouchEvent());
    } else {
      CHECK(event->IsMouseEvent());
      delegate_->HandleMouseEvent(event->AsMouseEvent());
    }

    last_touch_or_pen_message_time_ = ::GetMessageTime();
    is_pen_active_in_client_area_ = true;
  }

  if (ref)
    SetMsgHandled(handle_pen_events_in_client_area_);

  // When not dragging, always mark pen events as handled so as not to generate
  // WM_MOUSE compatibility events.
  if (message == WM_POINTERUP) {
    handle_pen_events_in_client_area_ = true;
  }

  return 0;
}

LRESULT HWNDMessageHandler::HandlePointerEventTypePenClient(UINT message,
                                                            WPARAM w_param,
                                                            LPARAM l_param) {
  UINT32 pointer_id = GET_POINTERID_WPARAM(w_param);
  POINTER_PEN_INFO pointer_pen_info;
  if (!GetPointerPenInfo(pointer_id, &pointer_pen_info)) {
    SetMsgHandled(FALSE);
    return -1;
  }

  return HandlePointerEventTypePen(message, pointer_id, pointer_pen_info);
}

bool HWNDMessageHandler::IsSynthesizedMouseMessage(unsigned int message,
                                                   int message_time,
                                                   LPARAM l_param) {
  if (ui::IsMouseEventFromTouch(message))
    return true;
  // Ignore mouse messages which occur at the same location as the current
  // cursor position and within a time difference of 500 ms from the last
  // touch message.
  if (last_touch_or_pen_message_time_ &&
      message_time >= last_touch_or_pen_message_time_ &&
      ((message_time - last_touch_or_pen_message_time_) <=
       kSynthesizedMouseMessagesTimeDifference)) {
    POINT mouse_location = CR_POINT_INITIALIZER_FROM_LPARAM(l_param);
    ::ClientToScreen(hwnd(), &mouse_location);
    POINT cursor_pos = {0};
    ::GetCursorPos(&cursor_pos);
    return memcmp(&cursor_pos, &mouse_location, sizeof(POINT)) == 0;
  }
  return false;
}

void HWNDMessageHandler::PerformDwmTransition() {
  CHECK(IsFrameSystemDrawn());

  dwm_transition_desired_ = false;
  delegate_->HandleFrameChanged();
  SendFrameChanged();
}

void HWNDMessageHandler::UpdateDwmFrame() {
  TRACE_EVENT0("ui", "HWNDMessageHandler::UpdateDwmFrame");

  gfx::Insets insets;
  if (delegate_->GetDwmFrameInsetsInPixels(&insets)) {
    MARGINS margins = {insets.left(), insets.right(), insets.top(),
                       insets.bottom()};
    DwmExtendFrameIntoClientArea(hwnd(), &margins);
  }
}

void HWNDMessageHandler::GenerateTouchEvent(ui::EventType event_type,
                                            const gfx::Point& point,
                                            ui::PointerId id,
                                            base::TimeTicks time_stamp,
                                            TouchEvents* touch_events) {
  ui::TouchEvent event(event_type, point, time_stamp,
                       ui::PointerDetails(ui::EventPointerType::kTouch, id));

  event.SetFlags(ui::GetModifiersFromKeyState());

  event.latency()->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time_stamp);

  touch_events->push_back(event);
}

bool HWNDMessageHandler::HandleMouseInputForCaption(unsigned int message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  // If we are receive a WM_NCLBUTTONDOWN messsage for the caption, the
  // following things happen.
  // 1. This message is defproced which will post a WM_SYSCOMMAND message with
  //    SC_MOVE and a constant 0x0002 which is documented on msdn as
  //    SC_DRAGMOVE.
  // 2. The WM_SYSCOMMAND message is defproced in our handler which works
  //    correctly in all cases except the case when we click on the caption
  //    and hold. The defproc appears to try and detect whether a mouse move
  //    is going to happen presumably via the DragEnter or a similar
  //    implementation which in its modal loop appears to only peek for
  //    mouse input briefly.
  // 3. Our workhorse message pump relies on the tickler posted message to get
  //    control during modal loops which does not happen in the above case for
  //    a while leading to the problem where activity on the page pauses
  //    briefly or at times stops for a while.
  // To fix this we don't defproc the WM_NCLBUTTONDOWN message and instead wait
  // for the subsequent WM_NCMOUSEMOVE/WM_MOUSEMOVE message. Once we receive
  // these messages and the mouse actually moved, we defproc the
  // WM_NCLBUTTONDOWN message.
  bool handled = false;
  switch (message) {
    case WM_NCLBUTTONDOWN: {
      if (w_param == HTCAPTION) {
        left_button_down_on_caption_ = true;
        // Cache the location where the click occurred. We use this in the
        // WM_NCMOUSEMOVE message to determine if the mouse actually moved.F
        caption_left_button_click_pos_.set_x(CR_GET_X_LPARAM(l_param));
        caption_left_button_click_pos_.set_y(CR_GET_Y_LPARAM(l_param));
        handled = true;
      }
      break;
    }

    // WM_NCMOUSEMOVE is received for normal drags which originate on the
    // caption and stay there.
    // WM_MOUSEMOVE can be received for drags which originate on the caption
    // and move towards the client area.
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE: {
      if (!left_button_down_on_caption_)
        break;

      bool should_handle_pending_ncl_button_down = true;
      // Check if the mouse actually moved.
      if (message == WM_NCMOUSEMOVE) {
        if (caption_left_button_click_pos_.x() == CR_GET_X_LPARAM(l_param) &&
            caption_left_button_click_pos_.y() == CR_GET_Y_LPARAM(l_param)) {
          should_handle_pending_ncl_button_down = false;
        }
      }
      if (should_handle_pending_ncl_button_down) {
        l_param = MAKELPARAM(caption_left_button_click_pos_.x(),
                             caption_left_button_click_pos_.y());
        // TODO(msw): Eliminate undesired painting, or re-evaluate this
        // workaround.
        // DefWindowProc for WM_NCLBUTTONDOWN does weird non-client painting,
        // so we need to call it inside a ScopedRedrawLock. This may cause
        // other negative side-effects
        // (ex/ stifling non-client mouse releases).
        // We may be deleted in the context of DefWindowProc. Don't refer to
        // any member variables after the DefWindowProc call.
        left_button_down_on_caption_ = false;

        if (delegate_->GetFrameMode() == FrameMode::CUSTOM_DRAWN) {
          DefWindowProcWithRedrawLock(WM_NCLBUTTONDOWN, HTCAPTION, l_param);
        } else {
          DefWindowProc(hwnd(), WM_NCLBUTTONDOWN, HTCAPTION, l_param);
        }
      }
      break;
    }

    case WM_NCMOUSELEAVE: {
      // If the DWM is rendering the window controls, we need to give the DWM's
      // default window procedure the chance to repaint the window border icons
      if (HasSystemFrame())
        handled = DwmDefWindowProc(hwnd(), WM_NCMOUSELEAVE, 0, 0, nullptr) != 0;
      break;
    }

    default:
      left_button_down_on_caption_ = false;
      break;
  }
  return handled;
}

void HWNDMessageHandler::SetBoundsInternal(const gfx::Rect& bounds_in_pixels,
                                           bool force_size_changed) {
  gfx::Size old_size = GetClientAreaBounds().size();

  SetWindowPos(hwnd(), nullptr, bounds_in_pixels.x(), bounds_in_pixels.y(),
               bounds_in_pixels.width(), bounds_in_pixels.height(),
               SWP_NOACTIVATE | SWP_NOZORDER);

  // If HWND size is not changed, we will not receive standard size change
  // notifications. If |force_size_changed| is |true|, we should pretend size is
  // changed.
  if (old_size == bounds_in_pixels.size() && force_size_changed &&
      !background_fullscreen_hack_) {
    auto ref = msg_handler_weak_factory_.GetWeakPtr();
    delegate_->HandleClientSizeChanged(GetClientAreaBounds().size());
    if (!ref) {
      return;
    }
    ResetWindowRegion(false, true);
  }
}

void HWNDMessageHandler::CheckAndHandleBackgroundFullscreenOnMonitor(
    HWND window) {
  HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);

  FullscreenWindowMonitorMap::iterator iter =
      fullscreen_monitor_map_.Get().find(monitor);
  if (iter != fullscreen_monitor_map_.Get().end()) {
    DCHECK(iter->second);
    if (window != iter->second->hwnd())
      iter->second->OnBackgroundFullscreen();
  }
}

void HWNDMessageHandler::OnBackgroundFullscreen() {
  DCHECK(IsFullscreen());
  // Reduce the bounds of the window by 1px to ensure that Windows does
  // not treat this like a fullscreen window.
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTOPRIMARY),
                 &monitor_info);
  gfx::Rect shrunk_rect(monitor_info.rcMonitor);
  shrunk_rect.set_height(shrunk_rect.height() - 1);
  background_fullscreen_hack_ = true;
  SetBoundsInternal(shrunk_rect, false);
  // Inform the taskbar that this window is no longer a fullscreen window so it
  // can bring itself to the top of the Z-Order. The taskbar heuristics to
  // detect fullscreen windows are not reliable. Marking it explicitly seems to
  // work around these problems.
  fullscreen_handler()->MarkFullscreen(false);
}

void HWNDMessageHandler::DestroyAXSystemCaret() {
  ax_system_caret_ = nullptr;
}

void HWNDMessageHandler::SizeWindowToAspectRatio(UINT param,
                                                 gfx::Rect* window_rect) {
  gfx::Size min_window_size;
  gfx::Size max_window_size;
  delegate_->GetMinMaxSize(&min_window_size, &max_window_size);
  min_window_size = delegate_->DIPToScreenSize(min_window_size);
  max_window_size = delegate_->DIPToScreenSize(max_window_size);

  std::optional<gfx::Size> max_size_param;
  if (!max_window_size.IsEmpty())
    max_size_param = max_window_size;

  gfx::SizeRectToAspectRatioWithExcludedMargin(
      GetWindowResizeEdge(param), aspect_ratio_.value(), min_window_size,
      max_size_param, excluded_margin_, *window_rect);
}

POINT HWNDMessageHandler::GetCursorPos() const {
  if (mock_cursor_position_.has_value())
    return mock_cursor_position_.value().ToPOINT();

  POINT cursor_pos = {};
  ::GetCursorPos(&cursor_pos);

  return cursor_pos;
}

// static
bool HWNDMessageHandler::IsTopLevelWindow(HWND window) {
  LONG style = ::GetWindowLong(window, GWL_STYLE);
  if (!(style & WS_CHILD)) {
    return true;
  }
  HWND parent = ::GetParent(window);
  return !parent || (parent == ::GetDesktopWindow());
}

// static
bool HWNDMessageHandler::GetMonitorAndRects(const RECT& rect,
                                            HMONITOR* monitor,
                                            gfx::Rect* monitor_rect,
                                            gfx::Rect* work_area) {
  DCHECK(monitor);
  DCHECK(monitor_rect);
  DCHECK(work_area);
  *monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONULL);
  if (!*monitor) {
    return false;
  }
  MONITORINFO monitor_info = {0};
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(*monitor, &monitor_info);
  *monitor_rect = gfx::Rect(monitor_info.rcMonitor);
  *work_area = gfx::Rect(monitor_info.rcWork);
  return true;
}

// Declared in pen_event_handler_util.h.
void UseDefaultHandlerForPenEventsUntilPenUp() {
  HWNDMessageHandler::UseDefaultHandlerForPenEventsUntilPenUp();
}

}  // namespace views
