// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_window.h"

#include <algorithm>
#include <vector>

#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test_x11.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_window_event_manager.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_path.h"
#include "ui/platform_window/common/platform_window_defaults.h"

namespace ui {

namespace {

// Special value of the _NET_WM_DESKTOP property which indicates that the window
// should appear on all workspaces/desktops.
const int kAllWorkspaces = 0xFFFFFFFF;

constexpr char kX11WindowRolePopup[] = "popup";
constexpr char kX11WindowRoleBubble[] = "bubble";
constexpr unsigned char kDarkGtkThemeVariant[] = "dark";

// In some situations, views tries to make a zero sized window, and that
// makes us crash. Make sure we have valid sizes.
gfx::Rect SanitizeBounds(const gfx::Rect& bounds) {
  gfx::Size sanitized_size(std::max(bounds.width(), 1),
                           std::max(bounds.height(), 1));
  gfx::Rect sanitized_bounds(bounds.origin(), sanitized_size);
  return sanitized_bounds;
}

void SerializeImageRepresentation(const gfx::ImageSkiaRep& rep,
                                  std::vector<unsigned long>* data) {
  int width = rep.GetWidth();
  data->push_back(width);

  int height = rep.GetHeight();
  data->push_back(height);

  const SkBitmap& bitmap = rep.GetBitmap();

  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      data->push_back(bitmap.getColor(x, y));
}

int XI2ModeToXMode(int xi2_mode) {
  switch (xi2_mode) {
    case XINotifyNormal:
      return NotifyNormal;
    case XINotifyGrab:
    case XINotifyPassiveGrab:
      return NotifyGrab;
    case XINotifyUngrab:
    case XINotifyPassiveUngrab:
      return NotifyUngrab;
    case XINotifyWhileGrabbed:
      return NotifyWhileGrabbed;
    default:
      NOTREACHED();
      return NotifyNormal;
  }
}

bool SyncSetCounter(XDisplay* display, XID counter, int64_t value) {
  XSyncValue sync_value;
  XSyncIntsToValue(&sync_value, value & 0xFFFFFFFF, value >> 32);
  return XSyncSetCounter(display, counter, sync_value) == x11::True;
}

// Returns the whole path from |window| to the root.
std::vector<::Window> GetParentsList(XDisplay* xdisplay, ::Window window) {
  ::Window parent_win, root_win;
  Window* child_windows;
  unsigned int num_child_windows;
  std::vector<::Window> result;

  while (window) {
    result.push_back(window);
    if (!XQueryTree(xdisplay, window, &root_win, &parent_win, &child_windows,
                    &num_child_windows))
      break;
    if (child_windows)
      XFree(child_windows);
    window = parent_win;
  }
  return result;
}

}  // namespace

XWindow::Configuration::Configuration()
    : type(WindowType::kWindow),
      opacity(WindowOpacity::kInferOpacity),
      icon(nullptr),
      activatable(true),
      force_show_in_taskbar(false),
      keep_on_top(false),
      visible_on_all_workspaces(false),
      remove_standard_frame(true),
      prefer_dark_theme(false) {}

XWindow::Configuration::Configuration(const Configuration&) = default;

XWindow::Configuration::~Configuration() = default;

XWindow::XWindow()
    : xdisplay_(gfx::GetXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)) {
  DCHECK(xdisplay_);
  DCHECK_NE(x_root_window_, x11::None);
}

XWindow::~XWindow() = default;

void XWindow::Init(const Configuration& config) {
  activatable_ = config.activatable;

  unsigned long attribute_mask = CWBackPixel | CWBitGravity;
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = x11::None;
  swa.bit_gravity = NorthWestGravity;
  swa.background_pixel = config.background_color.has_value()
                             ? config.background_color.value()
                             : WhitePixel(xdisplay_, DefaultScreen(xdisplay_));

  XAtom window_type;
  switch (config.type) {
    case WindowType::kMenu:
      swa.override_redirect = x11::True;
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_MENU");
      break;
    case WindowType::kTooltip:
      swa.override_redirect = x11::True;
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_TOOLTIP");
      break;
    case WindowType::kPopup:
      swa.override_redirect = x11::True;
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_NOTIFICATION");
      break;
    case WindowType::kDrag:
      swa.override_redirect = x11::True;
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_DND");
      break;
    default:
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_NORMAL");
      break;
  }
  // An in-activatable window should not interact with the system wm.
  if (!activatable_)
    swa.override_redirect = x11::True;

#if !defined(USE_X11)
  // It seems like there is a difference how tests are instantiated in case of
  // non-Ozone X11 and Ozone. See more details in
  // EnableTestConfigForPlatformWindows. The reason why this must be here is
  // that we removed X11WindowBase in favor of the XWindow. The X11WindowBase
  // was only used with PlatformWindow, which meant non-Ozone X11 did not use it
  // and set override_redirect based only on |activatable_| variable or
  // WindowType. But now as XWindow is subclassed by X11Window, which is also a
  // PlatformWindow, and non-Ozone X11 uses it, we have to add this workaround
  // here. Otherwise, tests for non-Ozone X11 fail.
  // TODO(msisov): figure out usage of this for non-Ozone X11.
  if (UseTestConfigForPlatformWindows())
    swa.override_redirect = true;
#endif

  override_redirect_ = swa.override_redirect == x11::True;
  if (override_redirect_)
    attribute_mask |= CWOverrideRedirect;

  bool enable_transparent_visuals;
  switch (config.opacity) {
    case WindowOpacity::kOpaqueWindow:
      enable_transparent_visuals = false;
      break;
    case WindowOpacity::kTranslucentWindow:
      enable_transparent_visuals = true;
      break;
    case WindowOpacity::kInferOpacity:
      enable_transparent_visuals = config.type == WindowType::kDrag;
  }

  Visual* visual = CopyFromParent;
  SetVisualId(config.visual_id);
  int depth = CopyFromParent;
  Colormap colormap = CopyFromParent;
  ui::XVisualManager* visual_manager = ui::XVisualManager::GetInstance();
  if (!visual_id_ ||
      !visual_manager->GetVisualInfo(visual_id_, &visual, &depth, &colormap,
                                     &visual_has_alpha_)) {
    visual_manager->ChooseVisualForWindow(enable_transparent_visuals, &visual,
                                          &depth, &colormap,
                                          &visual_has_alpha_);
  }

  if (colormap != CopyFromParent) {
    attribute_mask |= CWColormap;
    swa.colormap = colormap;
  }

  // x.org will BadMatch if we don't set a border when the depth isn't the
  // same as the parent depth.
  attribute_mask |= CWBorderPixel;
  swa.border_pixel = 0;

  bounds_in_pixels_ = SanitizeBounds(config.bounds);
  xwindow_ = XCreateWindow(xdisplay_, x_root_window_, bounds_in_pixels_.x(),
                           bounds_in_pixels_.y(), bounds_in_pixels_.width(),
                           bounds_in_pixels_.height(),
                           0,  // border width
                           depth, InputOutput, visual, attribute_mask, &swa);
  OnXWindowCreated();

  // TODO(erg): Maybe need to set a ViewProp here like in RWHL::RWHL().

  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask | EnterWindowMask |
                    LeaveWindowMask | ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  xwindow_events_ =
      std::make_unique<ui::XScopedEventSelector>(xwindow_, event_mask);
  XFlush(xdisplay_);

  if (ui::IsXInput2Available())
    ui::TouchFactory::GetInstance()->SetupXI2ForXWindow(xwindow_);

  // TODO(erg): We currently only request window deletion events. We also
  // should listen for activation events and anything else that GTK+ listens
  // for, and do something useful.
  // Request the _NET_WM_SYNC_REQUEST protocol which is used for synchronizing
  // between chrome and desktop compositor (or WM) during resizing.
  // The resizing behavior with _NET_WM_SYNC_REQUEST is:
  // 1. Desktop compositor (or WM) sends client message _NET_WM_SYNC_REQUEST
  //    with a 64 bits counter to notify about an incoming resize.
  // 2. Desktop compositor resizes chrome browser window.
  // 3. Desktop compositor waits on an alert on value change of XSyncCounter on
  //    chrome window.
  // 4. Chrome handles the ConfigureNotify event, and renders a new frame with
  //    the new size.
  // 5. Chrome increases the XSyncCounter on chrome window
  // 6. Desktop compositor gets the alert of counter change, and draws a new
  //    frame with new content from chrome.
  // 7. Desktop compositor responses user mouse move events, and starts a new
  //    resize process, go to step 1.
  XAtom protocols[] = {
      gfx::GetAtom("WM_DELETE_WINDOW"),
      gfx::GetAtom("_NET_WM_PING"),
      gfx::GetAtom("_NET_WM_SYNC_REQUEST"),
  };
  XSetWMProtocols(xdisplay_, xwindow_, protocols, base::size(protocols));

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, nullptr, nullptr, nullptr, 0, nullptr,
                   nullptr, nullptr);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(long) >= sizeof(pid_t),
                "pid_t should not be larger than long");
  long pid = getpid();
  XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_PID"), XA_CARDINAL,
                  32, PropModeReplace, reinterpret_cast<unsigned char*>(&pid),
                  1);

  XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_WINDOW_TYPE"),
                  XA_ATOM, 32, PropModeReplace,
                  reinterpret_cast<unsigned char*>(&window_type), 1);

  // The changes to |window_properties_| here will be sent to the X server just
  // before the window is mapped.

  // Remove popup windows from taskbar unless overridden.
  if ((config.type == WindowType::kPopup ||
       config.type == WindowType::kBubble) &&
      !config.force_show_in_taskbar) {
    window_properties_.insert(gfx::GetAtom("_NET_WM_STATE_SKIP_TASKBAR"));
  }

  // If the window should stay on top of other windows, add the
  // _NET_WM_STATE_ABOVE property.
  is_always_on_top_ = config.keep_on_top;
  if (is_always_on_top_)
    window_properties_.insert(gfx::GetAtom("_NET_WM_STATE_ABOVE"));

  workspace_ = base::nullopt;
  if (config.visible_on_all_workspaces) {
    window_properties_.insert(gfx::GetAtom("_NET_WM_STATE_STICKY"));
    ui::SetIntProperty(xwindow_, "_NET_WM_DESKTOP", "CARDINAL", kAllWorkspaces);
  } else if (!config.workspace.empty()) {
    int workspace;
    if (base::StringToInt(config.workspace, &workspace))
      ui::SetIntProperty(xwindow_, "_NET_WM_DESKTOP", "CARDINAL", workspace);
  }

  if (!config.wm_class_name.empty() || !config.wm_class_class.empty()) {
    ui::SetWindowClassHint(xdisplay_, xwindow_, config.wm_class_name,
                           config.wm_class_class);
  }

  const char* wm_role_name = nullptr;
  // If the widget isn't overriding the role, provide a default value for popup
  // and bubble types.
  if (!config.wm_role_name.empty()) {
    wm_role_name = config.wm_role_name.c_str();
  } else {
    switch (config.type) {
      case WindowType::kPopup:
        wm_role_name = kX11WindowRolePopup;
        break;
      case WindowType::kBubble:
        wm_role_name = kX11WindowRoleBubble;
        break;
      default:
        break;
    }
  }
  if (wm_role_name)
    ui::SetWindowRole(xdisplay_, xwindow_, std::string(wm_role_name));

  if (config.remove_standard_frame) {
    // Setting _GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED tells gnome-shell to not force
    // fullscreen on the window when it matches the desktop size.
    ui::SetHideTitlebarWhenMaximizedProperty(xwindow_,
                                             ui::HIDE_TITLEBAR_WHEN_MAXIMIZED);
  }

  if (config.prefer_dark_theme) {
    XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_GTK_THEME_VARIANT"),
                    gfx::GetAtom("UTF8_STRING"), 8, PropModeReplace,
                    kDarkGtkThemeVariant, base::size(kDarkGtkThemeVariant) - 1);
  }

  if (ui::IsSyncExtensionAvailable()) {
    XSyncValue value;
    XSyncIntToValue(&value, 0);
    update_counter_ = XSyncCreateCounter(xdisplay_, value);
    extended_update_counter_ = XSyncCreateCounter(xdisplay_, value);
    XID counters[]{update_counter_, extended_update_counter_};

    // Set XSyncCounter as window property _NET_WM_SYNC_REQUEST_COUNTER. the
    // compositor will listen on them during resizing.
    XChangeProperty(
        xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_SYNC_REQUEST_COUNTER"),
        XA_CARDINAL, 32, PropModeReplace,
        reinterpret_cast<const unsigned char*>(counters), base::size(counters));
  }

  // Always composite Chromium windows if a compositing WM is used.  Sometimes,
  // WMs will not composite fullscreen windows as an optimization, but this can
  // lead to tearing of fullscreen videos.
  ui::SetIntProperty(xwindow_, "_NET_WM_BYPASS_COMPOSITOR", "CARDINAL", 2);

  if (config.icon)
    SetXWindowIcons(gfx::ImageSkia(), *config.icon);
}

void XWindow::Map(bool inactive) {
  // Before we map the window, set size hints. Otherwise, some window managers
  // will ignore toplevel XMoveWindow commands.
  XSizeHints size_hints;
  size_hints.flags = 0;
  long supplied_return;
  XGetWMNormalHints(xdisplay_, xwindow_, &size_hints, &supplied_return);
  size_hints.flags |= PPosition;
  size_hints.x = bounds_in_pixels_.x();
  size_hints.y = bounds_in_pixels_.y();
  XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

  ignore_keyboard_input_ = inactive;
  unsigned long wm_user_time_ms =
      ignore_keyboard_input_ ? 0
                             : X11EventSource::GetInstance()->GetTimestamp();
  if (inactive || wm_user_time_ms != 0) {
    XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_USER_TIME"),
                    XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&wm_user_time_ms),
                    1);
  }

  UpdateMinAndMaxSize();

  if (window_properties_.empty()) {
    XDeleteProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_STATE"));
  } else {
    ui::SetAtomArrayProperty(xwindow_, "_NET_WM_STATE", "ATOM",
                             std::vector<XAtom>(std::begin(window_properties_),
                                                std::end(window_properties_)));
  }

  XMapWindow(xdisplay_, xwindow_);
  window_mapped_in_client_ = true;

  // TODO(thomasanderson): Find out why this flush is necessary.
  XFlush(xdisplay_);
}

void XWindow::Close() {
  if (xwindow_ == x11::None)
    return;

  CancelResize();
  UnconfineCursor();

  XDestroyWindow(xdisplay_, xwindow_);
  xwindow_ = x11::None;

  if (update_counter_ != x11::None) {
    XSyncDestroyCounter(xdisplay_, update_counter_);
    XSyncDestroyCounter(xdisplay_, extended_update_counter_);
    update_counter_ = x11::None;
    extended_update_counter_ = x11::None;
  }
}

void XWindow::Maximize() {
  // Some WMs do not respect maximization hints on unmapped windows, so we
  // save this one for later too.
  should_maximize_after_map_ = !window_mapped_in_client_;

  SetWMSpecState(true, gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

void XWindow::Minimize() {
  if (window_mapped_in_client_)
    XIconifyWindow(xdisplay_, xwindow_, 0);
  else
    SetWMSpecState(true, gfx::GetAtom("_NET_WM_STATE_HIDDEN"), x11::None);
}

void XWindow::Unmaximize() {
  should_maximize_after_map_ = false;
  SetWMSpecState(false, gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

bool XWindow::Hide() {
  if (!window_mapped_in_client_)
    return false;

  XWithdrawWindow(xdisplay_, xwindow_, 0);
  window_mapped_in_client_ = false;
  return true;
}

void XWindow::Unhide() {
  SetWMSpecState(false, gfx::GetAtom("_NET_WM_STATE_HIDDEN"), x11::None);
}

void XWindow::SetFullscreen(bool fullscreen) {
  SetWMSpecState(fullscreen, gfx::GetAtom("_NET_WM_STATE_FULLSCREEN"),
                 x11::None);
}

void XWindow::Activate() {
  if (!IsXWindowVisible() || !activatable_)
    return;

  BeforeActivationStateChanged();

  ignore_keyboard_input_ = false;

  // wmii says that it supports _NET_ACTIVE_WINDOW but does not.
  // https://code.google.com/p/wmii/issues/detail?id=266
  static bool wm_supports_active_window =
      ui::GuessWindowManager() != ui::WM_WMII &&
      ui::WmSupportsHint(gfx::GetAtom("_NET_ACTIVE_WINDOW"));

  ::Time timestamp = X11EventSource::GetInstance()->GetTimestamp();

  // override_redirect windows ignore _NET_ACTIVE_WINDOW.
  // https://crbug.com/940924
  if (wm_supports_active_window && !override_redirect_) {
    XEvent xclient;
    memset(&xclient, 0, sizeof(xclient));
    xclient.type = ClientMessage;
    xclient.xclient.window = xwindow_;
    xclient.xclient.message_type = gfx::GetAtom("_NET_ACTIVE_WINDOW");
    xclient.xclient.format = 32;
    xclient.xclient.data.l[0] = 1;  // Specified we are an app.
    xclient.xclient.data.l[1] = timestamp;
    // TODO(thomasanderson): if another chrome window is active, specify that in
    // data.l[2].  The EWMH spec claims this may make the WM more likely to
    // service our _NET_ACTIVE_WINDOW request.
    xclient.xclient.data.l[2] = x11::None;
    xclient.xclient.data.l[3] = 0;
    xclient.xclient.data.l[4] = 0;

    XSendEvent(xdisplay_, x_root_window_, x11::False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xclient);
  } else {
    XRaiseWindow(xdisplay_, xwindow_);
    // Directly ask the X server to give focus to the window. Note that the call
    // would have raised an X error if the window is not mapped.
    auto ignore_errors = [](XDisplay*, XErrorEvent*) -> int { return 0; };
    auto old_error_handler = XSetErrorHandler(ignore_errors);
    XSetInputFocus(xdisplay_, xwindow_, RevertToParent, timestamp);
    // At this point, we know we will receive focus, and some
    // webdriver tests depend on a window being IsActive() immediately
    // after an Activate(), so just set this state now.
    has_pointer_focus_ = false;
    has_window_focus_ = true;
    window_mapped_in_server_ = true;
    XSetErrorHandler(old_error_handler);
  }

  AfterActivationStateChanged();
}

void XWindow::Deactivate() {
  BeforeActivationStateChanged();

  // Ignore future input events.
  ignore_keyboard_input_ = true;

  XLowerWindow(xdisplay_, xwindow_);

  AfterActivationStateChanged();
}

bool XWindow::IsActive() const {
  // Focus and stacking order are independent in X11.  Since we cannot guarantee
  // a window is topmost iff it has focus, just use the focus state to determine
  // if a window is active.  Note that Activate() and Deactivate() change the
  // stacking order in addition to changing the focus state.
  bool is_active =
      (has_window_focus_ || has_pointer_focus_) && !ignore_keyboard_input_;

  // is_active => window_mapped_in_server_
  // !window_mapped_in_server_ => !is_active
  DCHECK(!is_active || window_mapped_in_server_);

  // |has_window_focus_| and |has_pointer_focus_| are mutually exclusive.
  DCHECK(!has_window_focus_ || !has_pointer_focus_);

  return is_active;
}
void XWindow::SetSize(const gfx::Size& size_in_pixels) {
  XResizeWindow(xdisplay_, xwindow_, size_in_pixels.width(),
                size_in_pixels.height());
  bounds_in_pixels_.set_size(size_in_pixels);
}

void XWindow::SetBounds(const gfx::Rect& requested_bounds_in_pixels) {
  gfx::Rect bounds_in_pixels(requested_bounds_in_pixels);
  bool origin_changed = bounds_in_pixels_.origin() != bounds_in_pixels.origin();
  bool size_changed = bounds_in_pixels_.size() != bounds_in_pixels.size();
  XWindowChanges changes = {0};
  unsigned value_mask = 0;

  if (size_changed) {
    // Update the minimum and maximum sizes in case they have changed.
    UpdateMinAndMaxSize();

    if (bounds_in_pixels.width() < min_size_in_pixels_.width() ||
        bounds_in_pixels.height() < min_size_in_pixels_.height() ||
        (!max_size_in_pixels_.IsEmpty() &&
         (bounds_in_pixels.width() > max_size_in_pixels_.width() ||
          bounds_in_pixels.height() > max_size_in_pixels_.height()))) {
      gfx::Size size_in_pixels = bounds_in_pixels.size();
      if (!max_size_in_pixels_.IsEmpty())
        size_in_pixels.SetToMin(max_size_in_pixels_);
      size_in_pixels.SetToMax(min_size_in_pixels_);
      bounds_in_pixels.set_size(size_in_pixels);
    }

    changes.width = bounds_in_pixels.width();
    changes.height = bounds_in_pixels.height();
    value_mask |= CWHeight | CWWidth;
  }

  if (origin_changed) {
    changes.x = bounds_in_pixels.x();
    changes.y = bounds_in_pixels.y();
    value_mask |= CWX | CWY;
  }

  if (value_mask)
    XConfigureWindow(xdisplay_, xwindow_, value_mask, &changes);

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_in_pixels_| later.
  bounds_in_pixels_ = bounds_in_pixels;
  ResetWindowRegion();
}

bool XWindow::IsXWindowVisible() const {
  // On Windows, IsVisible() returns true for minimized windows.  On X11, a
  // minimized window is not mapped, so an explicit IsMinimized() check is
  // necessary.
  return window_mapped_in_client_ || IsMinimized();
}

bool XWindow::IsMinimized() const {
  return ui::HasWMSpecProperty(window_properties_,
                               gfx::GetAtom("_NET_WM_STATE_HIDDEN"));
}

bool XWindow::IsMaximized() const {
  return (ui::HasWMSpecProperty(window_properties_,
                                gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT")) &&
          ui::HasWMSpecProperty(window_properties_,
                                gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ")));
}

bool XWindow::IsFullscreen() const {
  return ui::HasWMSpecProperty(window_properties_,
                               gfx::GetAtom("_NET_WM_STATE_FULLSCREEN"));
}

gfx::Rect XWindow::GetOutterBounds() const {
  gfx::Rect outer_bounds(bounds_in_pixels_);
  outer_bounds.Inset(-native_window_frame_borders_in_pixels_);
  return outer_bounds;
}

void XWindow::GrabPointer() {
  // If the pointer is already in |xwindow_|, we will not get a crossing event
  // with a mode of NotifyGrab, so we must record the grab state manually.
  has_pointer_grab_ |= !ui::GrabPointer(xwindow_, true, x11::None);
}

void XWindow::ReleasePointerGrab() {
  ui::UngrabPointer();
  has_pointer_grab_ = false;
}

void XWindow::StackXWindowAbove(::Window window) {
  DCHECK(window != x11::None);

  // Find all parent windows up to the root.
  std::vector<::Window> window_below_parents =
      GetParentsList(xdisplay_, window);
  std::vector<::Window> window_above_parents =
      GetParentsList(xdisplay_, xwindow_);

  // Find their common ancestor.
  auto it_below_window = window_below_parents.rbegin();
  auto it_above_window = window_above_parents.rbegin();
  for (; it_below_window != window_below_parents.rend() &&
         it_above_window != window_above_parents.rend() &&
         *it_below_window == *it_above_window;
       ++it_below_window, ++it_above_window) {
  }

  if (it_below_window != window_below_parents.rend() &&
      it_above_window != window_above_parents.rend()) {
    // First stack |xwindow| below so Z-order of |window| stays the same.
    ::Window windows[] = {*it_below_window, *it_above_window};
    if (XRestackWindows(xdisplay_, windows, 2) == 0) {
      // Now stack them properly.
      std::swap(windows[0], windows[1]);
      XRestackWindows(xdisplay_, windows, 2);
    }
  }
}

void XWindow::StackXWindowAtTop() {
  XRaiseWindow(xdisplay_, xwindow_);
}

void XWindow::SetCursor(::Cursor cursor) {
  XDefineCursor(xdisplay_, xwindow_, cursor);
}

bool XWindow::SetTitle(base::string16 title) {
  if (window_title_ == title)
    return false;

  window_title_ = title;
  std::string utf8str = base::UTF16ToUTF8(title);
  XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_NAME"),
                  gfx::GetAtom("UTF8_STRING"), 8, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(utf8str.c_str()),
                  utf8str.size());
  XTextProperty xtp;
  char* c_utf8_str = const_cast<char*>(utf8str.c_str());
  if (Xutf8TextListToTextProperty(xdisplay_, &c_utf8_str, 1, XUTF8StringStyle,
                                  &xtp) == x11::Success) {
    XSetWMName(xdisplay_, xwindow_, &xtp);
    XFree(xtp.value);
  }
  return true;
}

void XWindow::SetXWindowOpacity(float opacity) {
  // X server opacity is in terms of 32 bit unsigned int space, and counts from
  // the opposite direction.
  // XChangeProperty() expects "cardinality" to be long.

  // Scale opacity to [0 .. 255] range.
  unsigned long opacity_8bit =
      static_cast<unsigned long>(opacity * 255.0f) & 0xFF;
  // Use opacity value for all channels.
  const unsigned long channel_multiplier = 0x1010101;
  unsigned long cardinality = opacity_8bit * channel_multiplier;

  if (cardinality == 0xffffffff) {
    XDeleteProperty(xdisplay_, xwindow_,
                    gfx::GetAtom("_NET_WM_WINDOW_OPACITY"));
  } else {
    XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_WINDOW_OPACITY"),
                    XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&cardinality), 1);
  }
}

void XWindow::SetXWindowAspectRatio(const gfx::SizeF& aspect_ratio) {
  XSizeHints size_hints;
  size_hints.flags = 0;
  long supplied_return;

  XGetWMNormalHints(xdisplay_, xwindow_, &size_hints, &supplied_return);
  // Unforce aspect ratio is parameter length is 0, otherwise set normally.
  if (!aspect_ratio.IsEmpty()) {
    size_hints.flags |= PAspect;
    size_hints.min_aspect.x = size_hints.max_aspect.x = aspect_ratio.width();
    size_hints.min_aspect.y = size_hints.max_aspect.y = aspect_ratio.height();
  }
  XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);
}

void XWindow::SetXWindowIcons(const gfx::ImageSkia& window_icon,
                              const gfx::ImageSkia& app_icon) {
  // TODO(erg): The way we handle icons across different versions of chrome
  // could be substantially improved. The Windows version does its own thing
  // and only sometimes comes down this code path. The icon stuff in
  // ChromeViewsDelegate is hard coded to use HICONs. Likewise, we're hard
  // coded to be given two images instead of an arbitrary collection of images
  // so that we can pass to the WM.
  //
  // All of this could be made much, much better.
  std::vector<unsigned long> data;

  if (!window_icon.isNull())
    SerializeImageRepresentation(window_icon.GetRepresentation(1.0f), &data);

  if (!app_icon.isNull())
    SerializeImageRepresentation(app_icon.GetRepresentation(1.0f), &data);

  if (!data.empty())
    ui::SetAtomArrayProperty(xwindow_, "_NET_WM_ICON", "CARDINAL", data);
}

void XWindow::SetXWindowVisibleOnAllWorkspaces(bool visible) {
  SetWMSpecState(visible, gfx::GetAtom("_NET_WM_STATE_STICKY"), x11::None);

  int new_desktop = 0;
  if (visible) {
    new_desktop = kAllWorkspaces;
  } else {
    if (!ui::GetCurrentDesktop(&new_desktop))
      return;
  }

  workspace_ = kAllWorkspaces;
  XEvent xevent;
  memset(&xevent, 0, sizeof(xevent));
  xevent.type = ClientMessage;
  xevent.xclient.window = xwindow_;
  xevent.xclient.message_type = gfx::GetAtom("_NET_WM_DESKTOP");
  xevent.xclient.format = 32;
  xevent.xclient.data.l[0] = new_desktop;
  xevent.xclient.data.l[1] = 0;
  xevent.xclient.data.l[2] = 0;
  xevent.xclient.data.l[3] = 0;
  xevent.xclient.data.l[4] = 0;
  XSendEvent(xdisplay_, x_root_window_, x11::False,
             SubstructureRedirectMask | SubstructureNotifyMask, &xevent);
}

bool XWindow::IsXWindowVisibleOnAllWorkspaces() const {
  // We don't need a check for _NET_WM_STATE_STICKY because that would specify
  // that the window remain in a fixed position even if the viewport scrolls.
  // This is different from the type of workspace that's associated with
  // _NET_WM_DESKTOP.
  return workspace_ == kAllWorkspaces;
}

void XWindow::MoveCursorTo(const gfx::Point& location_in_pixels) {
  XWarpPointer(xdisplay_, x11::None, x_root_window_, 0, 0, 0, 0,
               bounds_in_pixels_.x() + location_in_pixels.x(),
               bounds_in_pixels_.y() + location_in_pixels.y());
}

void XWindow::ResetWindowRegion() {
  XRegion* xregion = nullptr;
  if (!use_custom_shape() && !IsMaximized() && !IsFullscreen()) {
    SkPath window_mask;
    GetWindowMaskForXWindow(bounds().size(), &window_mask);
    // Some frame views define a custom (non-rectangular) window mask. If
    // so, use it to define the window shape. If not, fall through.
    if (window_mask.countPoints() > 0)
      xregion = gfx::CreateRegionFromSkPath(window_mask);
  }
  UpdateWindowRegion(xregion);
}

void XWindow::OnWorkspaceUpdated() {
  auto old_workspace = workspace_;
  int workspace;
  if (ui::GetWindowDesktop(xwindow_, &workspace))
    workspace_ = workspace;
  else
    workspace_ = base::nullopt;

  if (workspace_ != old_workspace)
    OnXWindowWorkspaceChanged();
}

void XWindow::SetAlwaysOnTop(bool always_on_top) {
  is_always_on_top_ = always_on_top;
  SetWMSpecState(always_on_top, gfx::GetAtom("_NET_WM_STATE_ABOVE"), x11::None);
}

void XWindow::SetFlashFrameHint(bool flash_frame) {
  if (urgency_hint_set_ == flash_frame)
    return;

  gfx::XScopedPtr<XWMHints> hints(XGetWMHints(xdisplay_, xwindow_));
  if (!hints) {
    // The window hasn't had its hints set yet.
    hints.reset(XAllocWMHints());
  }

  if (flash_frame)
    hints->flags |= XUrgencyHint;
  else
    hints->flags &= ~XUrgencyHint;

  XSetWMHints(xdisplay_, xwindow_, hints.get());

  urgency_hint_set_ = flash_frame;
}

void XWindow::UpdateMinAndMaxSize() {
  base::Optional<gfx::Size> minimum_in_pixels = GetMinimumSizeForXWindow();
  base::Optional<gfx::Size> maximum_in_pixels = GetMaximumSizeForXWindow();
  if ((!minimum_in_pixels ||
       min_size_in_pixels_ == minimum_in_pixels.value()) &&
      (!maximum_in_pixels || max_size_in_pixels_ == maximum_in_pixels.value()))
    return;

  min_size_in_pixels_ = minimum_in_pixels.value();
  max_size_in_pixels_ = maximum_in_pixels.value();

  XSizeHints hints;
  hints.flags = 0;
  long supplied_return;
  XGetWMNormalHints(xdisplay_, xwindow_, &hints, &supplied_return);

  if (min_size_in_pixels_.IsEmpty()) {
    hints.flags &= ~PMinSize;
  } else {
    hints.flags |= PMinSize;
    hints.min_width = min_size_in_pixels_.width();
    hints.min_height = min_size_in_pixels_.height();
  }

  if (max_size_in_pixels_.IsEmpty()) {
    hints.flags &= ~PMaxSize;
  } else {
    hints.flags |= PMaxSize;
    hints.max_width = max_size_in_pixels_.width();
    hints.max_height = max_size_in_pixels_.height();
  }

  XSetWMNormalHints(xdisplay_, xwindow_, &hints);
}

void XWindow::BeforeActivationStateChanged() {
  was_active_ = IsActive();
  had_pointer_ = has_pointer_;
  had_pointer_grab_ = has_pointer_grab_;
  had_window_focus_ = has_window_focus_;
}

void XWindow::AfterActivationStateChanged() {
  if (had_pointer_grab_ && !has_pointer_grab_)
    OnXWindowLostPointerGrab();

  bool had_pointer_capture = had_pointer_ || had_pointer_grab_;
  bool has_pointer_capture = has_pointer_ || has_pointer_grab_;
  if (had_pointer_capture && !has_pointer_capture)
    OnXWindowLostCapture();

  bool is_active = IsActive();
  if (!was_active_ && is_active)
    SetFlashFrameHint(false);

  if (was_active_ != is_active)
    OnXWindowIsActiveChanged(is_active);
}

void XWindow::SetUseNativeFrame(bool use_native_frame) {
  use_native_frame_ = use_native_frame;
  ui::SetUseOSWindowFrame(xwindow_, use_native_frame);
  ResetWindowRegion();
}

void XWindow::OnCrossingEvent(bool enter,
                              bool focus_in_window_or_ancestor,
                              int mode,
                              int detail) {
  // NotifyInferior on a crossing event means the pointer moved into or out of a
  // child window, but the pointer is still within |xwindow_|.
  if (detail == NotifyInferior)
    return;

  BeforeActivationStateChanged();

  if (mode == NotifyGrab)
    has_pointer_grab_ = enter;
  else if (mode == NotifyUngrab)
    has_pointer_grab_ = false;

  has_pointer_ = enter;
  if (focus_in_window_or_ancestor && !has_window_focus_) {
    // If we reach this point, we know the focus is in an ancestor or the
    // pointer root.  The definition of |has_pointer_focus_| is (An ancestor
    // window or the PointerRoot is focused) && |has_pointer_|.  Therefore, we
    // can just use |has_pointer_| in the assignment.  The transitions for when
    // the focus changes are handled in OnFocusEvent().
    has_pointer_focus_ = has_pointer_;
  }

  AfterActivationStateChanged();
}

void XWindow::OnFocusEvent(bool focus_in, int mode, int detail) {
  // NotifyInferior on a focus event means the focus moved into or out of a
  // child window, but the focus is still within |xwindow_|.
  if (detail == NotifyInferior)
    return;

  bool notify_grab = mode == NotifyGrab || mode == NotifyUngrab;

  BeforeActivationStateChanged();

  // For every focus change, the X server sends normal focus events which are
  // useful for tracking |has_window_focus_|, but supplements these events with
  // NotifyPointer events which are only useful for tracking pointer focus.

  // For |has_pointer_focus_| and |has_window_focus_|, we continue tracking
  // state during a grab, but ignore grab/ungrab events themselves.
  if (!notify_grab && detail != NotifyPointer)
    has_window_focus_ = focus_in;

  if (!notify_grab && has_pointer_) {
    switch (detail) {
      case NotifyAncestor:
      case NotifyVirtual:
        // If we reach this point, we know |has_pointer_| was true before and
        // after this event.  Since the definition of |has_pointer_focus_| is
        // (An ancestor window or the PointerRoot is focused) && |has_pointer_|,
        // we only need to worry about transitions on the first conjunct.
        // Therefore, |has_pointer_focus_| will become true when:
        // 1. Focus moves from |xwindow_| to an ancestor
        //    (FocusOut with NotifyAncestor)
        // 2. Focus moves from a descendant of |xwindow_| to an ancestor
        //    (FocusOut with NotifyVirtual)
        // |has_pointer_focus_| will become false when:
        // 1. Focus moves from an ancestor to |xwindow_|
        //    (FocusIn with NotifyAncestor)
        // 2. Focus moves from an ancestor to a child of |xwindow_|
        //    (FocusIn with NotifyVirtual)
        has_pointer_focus_ = !focus_in;
        break;
      case NotifyPointer:
        // The remaining cases for |has_pointer_focus_| becoming true are:
        // 3. Focus moves from |xwindow_| to the PointerRoot
        // 4. Focus moves from a descendant of |xwindow_| to the PointerRoot
        // 5. Focus moves from None to the PointerRoot
        // 6. Focus moves from Other to the PointerRoot
        // 7. Focus moves from None to an ancestor of |xwindow_|
        // 8. Focus moves from Other to an ancestor of |xwindow_|
        // In each case, we will get a FocusIn with a detail of NotifyPointer.
        // The remaining cases for |has_pointer_focus_| becoming false are:
        // 3. Focus moves from the PointerRoot to |xwindow_|
        // 4. Focus moves from the PointerRoot to a descendant of |xwindow|
        // 5. Focus moves from the PointerRoot to None
        // 6. Focus moves from an ancestor of |xwindow_| to None
        // 7. Focus moves from the PointerRoot to Other
        // 8. Focus moves from an ancestor of |xwindow_| to Other
        // In each case, we will get a FocusOut with a detail of NotifyPointer.
        has_pointer_focus_ = focus_in;
        break;
      case NotifyNonlinear:
      case NotifyNonlinearVirtual:
        // We get Nonlinear(Virtual) events when
        // 1. Focus moves from Other to |xwindow_|
        //    (FocusIn with NotifyNonlinear)
        // 2. Focus moves from Other to a descendant of |xwindow_|
        //    (FocusIn with NotifyNonlinearVirtual)
        // 3. Focus moves from |xwindow_| to Other
        //    (FocusOut with NotifyNonlinear)
        // 4. Focus moves from a descendant of |xwindow_| to Other
        //    (FocusOut with NotifyNonlinearVirtual)
        // |has_pointer_focus_| should be false before and after this event.
        has_pointer_focus_ = false;
        break;
      default:
        break;
    }
  }

  ignore_keyboard_input_ = false;

  AfterActivationStateChanged();
}

bool XWindow::IsTargetedBy(const XEvent& xev) const {
  ::Window target_window =
      (xev.type == GenericEvent)
          ? static_cast<XIDeviceEvent*>(xev.xcookie.data)->event
          : xev.xany.window;
  return target_window == xwindow_;
}

void XWindow::WmMoveResize(int hittest, const gfx::Point& location) const {
  int direction = HitTestToWmMoveResizeDirection(hittest);
  if (direction == -1)
    return;

  DoWMMoveResize(xdisplay_, x_root_window_, xwindow_, location, direction);
}

// In Ozone, there are no ui::*Event constructors receiving XEvent* as input,
// in this case ui::PlatformEvent is expected. Furthermore,
// X11EventSourceLibevent is used in that case, which already translates
// Mouse/Key/Touch/Scroll events into ui::Events so they should not be handled
// by PlatformWindow, which is supposed to use XWindow in Ozone builds. So
// handling these events is disabled for Ozone.
void XWindow::ProcessEvent(XEvent* xev) {
  UpdateWMUserTime(xev);

  // We can lose track of the window's position when the window is reparented.
  // When the parent window is moved, we won't get an event, so the window's
  // position relative to the root window will get out-of-sync.  We can re-sync
  // when getting pointer events (EnterNotify, LeaveNotify, ButtonPress,
  // ButtonRelease, MotionNotify) which include the pointer location both
  // relative to this window and relative to the root window, so we can
  // calculate this window's position from that information.
  gfx::Point window_point = ui::EventLocationFromXEvent(*xev);
  gfx::Point root_point = ui::EventSystemLocationFromXEvent(*xev);
  if (!window_point.IsOrigin() && !root_point.IsOrigin()) {
    gfx::Point window_origin = gfx::Point() + (root_point - window_point);
    if (bounds_in_pixels_.origin() != window_origin) {
      bounds_in_pixels_.set_origin(window_origin);
      NotifyBoundsChanged(bounds_in_pixels_);
    }
  }

  // May want to factor CheckXEventForConsistency(xev); into a common location
  // since it is called here.
  switch (xev->type) {
    case EnterNotify:
    case LeaveNotify: {
#if defined(USE_X11)
      // Ignore EventNotify and LeaveNotify events from children of |xwindow_|.
      // NativeViewGLSurfaceGLX adds a child to |xwindow_|.
      if (xev->xcrossing.detail != NotifyInferior) {
        DCHECK(xev);
        ui::MouseEvent mouse_event(xev);
        OnXWindowEvent(&mouse_event);
      } else
#endif
      {
        bool is_enter = xev->type == EnterNotify;
        OnCrossingEvent(is_enter, xev->xcrossing.focus, xev->xcrossing.mode,
                        xev->xcrossing.detail);
      }
      break;
    }
    case Expose: {
      gfx::Rect damage_rect_in_pixels(xev->xexpose.x, xev->xexpose.y,
                                      xev->xexpose.width, xev->xexpose.height);
      OnXWindowDamageEvent(damage_rect_in_pixels);
      break;
    }
#if !defined(USE_OZONE)
    case KeyPress:
    case KeyRelease: {
      ui::KeyEvent key_event(xev);
      OnXWindowEvent(&key_event);
      break;
    }
    case ButtonPress:
    case ButtonRelease: {
      ui::EventType event_type = ui::EventTypeFromNative(xev);
      switch (event_type) {
        case ui::ET_MOUSEWHEEL: {
          ui::MouseWheelEvent mouseev(xev);
          OnXWindowEvent(&mouseev);
          break;
        }
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSE_RELEASED: {
          ui::MouseEvent mouseev(xev);
          OnXWindowEvent(&mouseev);
          break;
        }
        case ui::ET_UNKNOWN:
          // No event is created for X11-release events for mouse-wheel buttons.
          break;
        default:
          NOTREACHED() << event_type;
      }
      break;
    }
#endif
    case x11::FocusIn:
    case x11::FocusOut:
      OnFocusEvent(xev->type == x11::FocusIn, xev->xfocus.mode,
                   xev->xfocus.detail);
      break;
    case ConfigureNotify:
      OnConfigureEvent(xev);
      break;
    case GenericEvent: {
      ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
      if (!factory->ShouldProcessXI2Event(xev))
        break;

      XIEnterEvent* enter_event = static_cast<XIEnterEvent*>(xev->xcookie.data);
      switch (static_cast<XIEvent*>(xev->xcookie.data)->evtype) {
        case XI_Enter:
        case XI_Leave: {
          bool is_enter = enter_event->evtype == XI_Enter;
          OnCrossingEvent(is_enter, enter_event->focus,
                          XI2ModeToXMode(enter_event->mode),
                          enter_event->detail);
          return;
        }
        case XI_FocusIn:
        case XI_FocusOut: {
          OnFocusEvent(enter_event->evtype == XI_FocusIn,
                       XI2ModeToXMode(enter_event->mode), enter_event->detail);
          return;
        }
        default:
          break;
      }
#if !defined(USE_OZONE)
      int num_coalesced = 0;
      ui::EventType type = ui::EventTypeFromNative(xev);
      XEvent last_event;

      switch (type) {
        case ui::ET_TOUCH_MOVED:
          num_coalesced = ui::CoalescePendingMotionEvents(xev, &last_event);
          if (num_coalesced > 0)
            xev = &last_event;
          FALLTHROUGH;
        case ui::ET_TOUCH_PRESSED:
        case ui::ET_TOUCH_RELEASED: {
          ui::TouchEvent touchev(xev);
          OnXWindowEvent(&touchev);
          break;
        }
        case ui::ET_MOUSE_MOVED:
        case ui::ET_MOUSE_DRAGGED:
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSE_RELEASED:
        case ui::ET_MOUSE_ENTERED:
        case ui::ET_MOUSE_EXITED: {
          if (type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_DRAGGED) {
            // If this is a motion event, we want to coalesce all pending motion
            // events that are at the top of the queue.
            num_coalesced = ui::CoalescePendingMotionEvents(xev, &last_event);
            if (num_coalesced > 0)
              xev = &last_event;
          }
          ui::MouseEvent mouseev(xev);
          // If after CoalescePendingMotionEvents the type of xev is resolved to
          // UNKNOWN, don't dispatch the event.
          // TODO(804418): investigate why ColescePendingMotionEvents can
          // include mouse wheel events as well. Investigation showed that
          // events on Linux are checked with cmt-device path, and can include
          // DT_CMT_SCROLL_ data. See more discussion in
          // https://crrev.com/c/853953
          if (mouseev.type() != ui::ET_UNKNOWN)
            OnXWindowEvent(&mouseev);
          break;
        }
        case ui::ET_MOUSEWHEEL: {
          ui::MouseWheelEvent mouseev(xev);
          OnXWindowEvent(&mouseev);
          break;
        }
        case ui::ET_SCROLL_FLING_START:
        case ui::ET_SCROLL_FLING_CANCEL:
        case ui::ET_SCROLL: {
          ui::ScrollEvent scrollev(xev);
          // We need to filter zero scroll offset here. Because
          // MouseWheelEventQueue assumes we'll never get a zero scroll offset
          // event and we need delta to determine which element to scroll on
          // phaseBegan.
          if (scrollev.x_offset() != 0.0 || scrollev.y_offset() != 0.0)
            OnXWindowEvent(&scrollev);
          break;
        }
        case ui::ET_KEY_PRESSED:
        case ui::ET_KEY_RELEASED: {
          ui::KeyEvent key_event(xev);
          OnXWindowEvent(&key_event);
          break;
        }
        case ui::ET_UNKNOWN:
          break;
        default:
          NOTREACHED();
      }

      // If we coalesced an event we need to free its cookie.
      if (num_coalesced > 0)
        XFreeEventData(xev->xgeneric.display, &last_event.xcookie);
#endif

      break;
    }
    case MapNotify: {
      OnWindowMapped();
      break;
    }
    case UnmapNotify: {
      window_mapped_in_server_ = false;
      has_pointer_ = false;
      has_pointer_grab_ = false;
      has_pointer_focus_ = false;
      has_window_focus_ = false;
      OnXWindowUnmapped();
      break;
    }
    case ClientMessage: {
      Atom message_type = xev->xclient.message_type;
      if (message_type == gfx::GetAtom("WM_PROTOCOLS")) {
        Atom protocol = static_cast<Atom>(xev->xclient.data.l[0]);
        if (protocol == gfx::GetAtom("WM_DELETE_WINDOW")) {
          // We have received a close message from the window manager.
          OnXWindowCloseRequested();
        } else if (protocol == gfx::GetAtom("_NET_WM_PING")) {
          XEvent reply_event = *xev;
          reply_event.xclient.window = x_root_window_;

          XSendEvent(xdisplay_, reply_event.xclient.window, x11::False,
                     SubstructureRedirectMask | SubstructureNotifyMask,
                     &reply_event);
        } else if (protocol == gfx::GetAtom("_NET_WM_SYNC_REQUEST")) {
          pending_counter_value_ =
              xev->xclient.data.l[2] +
              (static_cast<int64_t>(xev->xclient.data.l[3]) << 32);
          pending_counter_value_is_extended_ = xev->xclient.data.l[4] != 0;
        }
      } else {
        OnXWindowDragDropEvent(xev);
      }
      break;
    }
    case MappingNotify: {
      switch (xev->xmapping.request) {
        case MappingModifier:
        case MappingKeyboard:
          XRefreshKeyboardMapping(&xev->xmapping);
          break;
        case MappingPointer:
          ui::DeviceDataManagerX11::GetInstance()->UpdateButtonMap();
          break;
        default:
          NOTIMPLEMENTED() << " Unknown request: " << xev->xmapping.request;
          break;
      }
      break;
    }
#if !defined(USE_OZONE)
    case MotionNotify: {
      // Discard all but the most recent motion event that targets the same
      // window with unchanged state.
      XEvent last_event;
      while (XPending(xev->xany.display)) {
        XEvent next_event;
        XPeekEvent(xev->xany.display, &next_event);
        if (next_event.type == MotionNotify &&
            next_event.xmotion.window == xev->xmotion.window &&
            next_event.xmotion.subwindow == xev->xmotion.subwindow &&
            next_event.xmotion.state == xev->xmotion.state) {
          XNextEvent(xev->xany.display, &last_event);
          xev = &last_event;
        } else {
          break;
        }
      }

      ui::MouseEvent mouseev(xev);
      OnXWindowEvent(&mouseev);
      break;
    }
#endif
    case PropertyNotify: {
      XAtom changed_atom = xev->xproperty.atom;
      if (changed_atom == gfx::GetAtom("_NET_WM_STATE")) {
        OnWMStateUpdated();
      } else if (changed_atom == gfx::GetAtom("_NET_FRAME_EXTENTS")) {
        OnFrameExtentsUpdated();
      } else if (changed_atom == gfx::GetAtom("_NET_WM_DESKTOP")) {
        OnWorkspaceUpdated();
      }
      break;
    }
    case SelectionNotify: {
      OnXWindowSelectionEvent(xev);
      break;
    }
  }
}

void XWindow::UpdateWMUserTime(XEvent* xev) {
  if (!IsActive())
    return;

  ui::EventType type = ui::EventTypeFromXEvent(*xev);
  if (type == ui::ET_MOUSE_PRESSED || type == ui::ET_KEY_PRESSED ||
      type == ui::ET_TOUCH_PRESSED) {
    unsigned long wm_user_time_ms = static_cast<unsigned long>(
        (ui::EventTimeFromXEvent(*xev) - base::TimeTicks()).InMilliseconds());
    XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_USER_TIME"),
                    XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&wm_user_time_ms),
                    1);
  }
}

void XWindow::OnWindowMapped() {
  window_mapped_in_server_ = true;
  OnXWindowMapped();
  // Some WMs only respect maximize hints after the window has been mapped.
  // Check whether we need to re-do a maximization.
  if (should_maximize_after_map_) {
    Maximize();
    should_maximize_after_map_ = false;
  }
}

void XWindow::OnConfigureEvent(XEvent* xev) {
  DCHECK_EQ(xwindow_, xev->xconfigure.window);
  DCHECK_EQ(xwindow_, xev->xconfigure.event);

  if (pending_counter_value_) {
    DCHECK(!configure_counter_value_);
    configure_counter_value_ = pending_counter_value_;
    configure_counter_value_is_extended_ = pending_counter_value_is_extended_;
    pending_counter_value_is_extended_ = 0;
    pending_counter_value_ = 0;
  }

  // It's possible that the X window may be resized by some other means than
  // from within aura (e.g. the X window manager can change the size). Make
  // sure the root window size is maintained properly.
  int translated_x_in_pixels = xev->xconfigure.x;
  int translated_y_in_pixels = xev->xconfigure.y;
  if (!xev->xconfigure.send_event && !xev->xconfigure.override_redirect) {
    Window unused;
    XTranslateCoordinates(xdisplay_, xwindow_, x_root_window_, 0, 0,
                          &translated_x_in_pixels, &translated_y_in_pixels,
                          &unused);
  }
  gfx::Rect bounds_in_pixels(translated_x_in_pixels, translated_y_in_pixels,
                             xev->xconfigure.width, xev->xconfigure.height);
  bool size_changed = bounds_in_pixels_.size() != bounds_in_pixels.size();
  bool origin_changed = bounds_in_pixels_.origin() != bounds_in_pixels.origin();
  previous_bounds_in_pixels_ = bounds_in_pixels_;
  bounds_in_pixels_ = bounds_in_pixels;

  if (size_changed)
    DispatchResize();
  else if (origin_changed)
    NotifyBoundsChanged(bounds_in_pixels_);
}

void XWindow::SetWMSpecState(bool enabled, XAtom state1, XAtom state2) {
  if (window_mapped_in_client_) {
    ui::SetWMSpecState(xwindow_, enabled, state1, state2);
  } else {
    // The updated state will be set when the window is (re)mapped.
    base::flat_set<XAtom> new_window_properties = window_properties_;
    for (XAtom atom : {state1, state2}) {
      if (enabled)
        new_window_properties.insert(atom);
      else
        new_window_properties.erase(atom);
    }
    UpdateWindowProperties(new_window_properties);
  }
}

void XWindow::OnWMStateUpdated() {
  // The EWMH spec requires window managers to remove the _NET_WM_STATE property
  // when a window is unmapped.  However, Chromium code wants the state to
  // persist across a Hide() and Show().  So if the window is currently
  // unmapped, leave the state unchanged so it will be restored when the window
  // is remapped.
  std::vector<XAtom> atom_list;
  if (ui::GetAtomArrayProperty(xwindow_, "_NET_WM_STATE", &atom_list) ||
      window_mapped_in_client_) {
    UpdateWindowProperties(
        base::flat_set<XAtom>(std::begin(atom_list), std::end(atom_list)));
  }
}

void XWindow::UpdateWindowProperties(
    const base::flat_set<XAtom>& new_window_properties) {
  was_minimized_ = IsMinimized();

  window_properties_ = new_window_properties;

  // Ignore requests by the window manager to enter or exit fullscreen (e.g. as
  // a result of pressing a window manager accelerator key). Chrome does not
  // handle window manager initiated fullscreen. In particular, Chrome needs to
  // do preprocessing before the x window's fullscreen state is toggled.

  is_always_on_top_ = ui::HasWMSpecProperty(
      window_properties_, gfx::GetAtom("_NET_WM_STATE_ABOVE"));
  OnXWindowStateChanged();
  ResetWindowRegion();
}

void XWindow::OnFrameExtentsUpdated() {
  std::vector<int> insets;
  if (ui::GetIntArrayProperty(xwindow_, "_NET_FRAME_EXTENTS", &insets) &&
      insets.size() == 4) {
    // |insets| are returned in the order: [left, right, top, bottom].
    native_window_frame_borders_in_pixels_ =
        gfx::Insets(insets[2], insets[0], insets[3], insets[1]);
  } else {
    native_window_frame_borders_in_pixels_ = gfx::Insets();
  }
}

void XWindow::NotifySwapAfterResize() {
  if (configure_counter_value_is_extended_) {
    if ((current_counter_value_ % 2) == 1) {
      // An increase 3 means that the frame was not drawn as fast as possible.
      // This can trigger different handling from the compositor.
      // Setting an even number to |extended_update_counter_| will trigger a
      // new resize.
      current_counter_value_ += 3;
      SyncSetCounter(xdisplay_, extended_update_counter_,
                     current_counter_value_);
    }
    return;
  }

  if (configure_counter_value_ != 0) {
    SyncSetCounter(xdisplay_, update_counter_, configure_counter_value_);
    configure_counter_value_ = 0;
  }
}

// Removes |delayed_resize_task_| from the task queue (if it's in
// the queue) and adds it back at the end of the queue.
void XWindow::DispatchResize() {
  if (update_counter_ == x11::None || configure_counter_value_ == 0) {
    // WM doesn't support _NET_WM_SYNC_REQUEST.
    // Or we are too slow, so _NET_WM_SYNC_REQUEST is disabled by the
    // compositor.
    delayed_resize_task_.Reset(base::BindOnce(&XWindow::DelayedResize,
                                              weak_factory_.GetWeakPtr(),
                                              bounds_in_pixels_));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, delayed_resize_task_.callback());
    return;
  }

  if (configure_counter_value_is_extended_) {
    current_counter_value_ = configure_counter_value_;
    configure_counter_value_ = 0;
    // Make sure the counter is even number.
    if ((current_counter_value_ % 2) == 1)
      ++current_counter_value_;
  }

  // If _NET_WM_SYNC_REQUEST is used to synchronize with compositor during
  // resizing, the compositor will not resize the window, until last resize is
  // handled, so we don't need accumulate resize events.
  DelayedResize(bounds_in_pixels_);
}

void XWindow::DelayedResize(const gfx::Rect& bounds_in_pixels) {
  if (configure_counter_value_is_extended_ &&
      (current_counter_value_ % 2) == 0) {
    // Increase the |extended_update_counter_|, so the compositor will know we
    // are not frozen and re-enable _NET_WM_SYNC_REQUEST, if it was disabled.
    // Increase the |extended_update_counter_| to an odd number will not trigger
    // a new resize.
    SyncSetCounter(xdisplay_, extended_update_counter_,
                   ++current_counter_value_);
  }
  NotifyBoundsChanged(bounds_in_pixels);
  CancelResize();
}

void XWindow::CancelResize() {
  delayed_resize_task_.Cancel();
}

void XWindow::ConfineCursorTo(const gfx::Rect& bounds) {
  UnconfineCursor();

  if (bounds.IsEmpty())
    return;

  gfx::Rect barrier = bounds + bounds_in_pixels_.OffsetFromOrigin();

  // Top horizontal barrier.
  pointer_barriers_[0] = XFixesCreatePointerBarrier(
      xdisplay_, x_root_window_, barrier.x(), barrier.y(), barrier.right(),
      barrier.y(), BarrierPositiveY, 0, XIAllDevices);
  // Bottom horizontal barrier.
  pointer_barriers_[1] = XFixesCreatePointerBarrier(
      xdisplay_, x_root_window_, barrier.x(), barrier.bottom(), barrier.right(),
      barrier.bottom(), BarrierNegativeY, 0, XIAllDevices);
  // Left vertical barrier.
  pointer_barriers_[2] = XFixesCreatePointerBarrier(
      xdisplay_, x_root_window_, barrier.x(), barrier.y(), barrier.x(),
      barrier.bottom(), BarrierPositiveX, 0, XIAllDevices);
  // Right vertical barrier.
  pointer_barriers_[3] = XFixesCreatePointerBarrier(
      xdisplay_, x_root_window_, barrier.right(), barrier.y(), barrier.right(),
      barrier.bottom(), BarrierNegativeX, 0, XIAllDevices);

  has_pointer_barriers_ = true;
}

void XWindow::LowerWindow() {
  XLowerWindow(xdisplay_, xwindow_);
}

bool XWindow::ContainsPointInRegion(const gfx::Point& point) const {
  if (!shape())
    return true;

  return XPointInRegion(shape(), point.x(), point.y()) == x11::True;
}

void XWindow::SetXWindowShape(std::unique_ptr<NativeShapeRects> native_shape,
                              const gfx::Transform& transform) {
  XRegion* xregion = nullptr;
  if (native_shape) {
    SkRegion native_region;
    for (const gfx::Rect& rect : *native_shape)
      native_region.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
    if (!transform.IsIdentity() && !native_region.isEmpty()) {
      SkPath path_in_dip;
      if (native_region.getBoundaryPath(&path_in_dip)) {
        SkPath path_in_pixels;
        path_in_dip.transform(transform.matrix(), &path_in_pixels);
        xregion = gfx::CreateRegionFromSkPath(path_in_pixels);
      } else {
        xregion = XCreateRegion();
      }
    } else {
      xregion = gfx::CreateRegionFromSkRegion(native_region);
    }
  }

  custom_window_shape_ = !!xregion;
  window_shape_.reset(xregion);
  ResetWindowRegion();
}

void XWindow::UnconfineCursor() {
  if (!has_pointer_barriers_)
    return;

  for (XID pointer_barrier : pointer_barriers_)
    XFixesDestroyPointerBarrier(xdisplay_, pointer_barrier);
  pointer_barriers_.fill(x11::None);

  has_pointer_barriers_ = false;
}

void XWindow::SetVisualId(base::Optional<int> visual_id) {
  if (!visual_id.has_value())
    return;

  DCHECK_GE(visual_id.value(), 0);
  visual_id_ = visual_id.value();
}

void XWindow::UpdateWindowRegion(XRegion* xregion) {
  // If a custom window shape was supplied then apply it.
  if (use_custom_shape()) {
    XShapeCombineRegion(xdisplay_, xwindow_, ShapeBounding, 0, 0,
                        window_shape_.get(), false);
    return;
  }

  window_shape_.reset(xregion);
  if (window_shape_) {
    XShapeCombineRegion(xdisplay_, xwindow_, ShapeBounding, 0, 0,
                        window_shape_.get(), false);
    return;
  }

  // If we didn't set the shape for any reason, reset the shaping information.
  // How this is done depends on the border style, due to quirks and bugs in
  // various window managers.
  if (use_native_frame()) {
    // If the window has system borders, the mask must be set to null (not a
    // rectangle), because several window managers (eg, KDE, XFCE, XMonad) will
    // not put borders on a window with a custom shape.
    XShapeCombineMask(xdisplay_, xwindow_, ShapeBounding, 0, 0, x11::None,
                      ShapeSet);
  } else {
    // Conversely, if the window does not have system borders, the mask must be
    // manually set to a rectangle that covers the whole window (not null). This
    // is due to a bug in KWin <= 4.11.5 (KDE bug #330573) where setting a null
    // shape causes the hint to disable system borders to be ignored (resulting
    // in a double border).
    XRectangle r = {0, 0,
                    static_cast<unsigned short>(bounds_in_pixels_.width()),
                    static_cast<unsigned short>(bounds_in_pixels_.height())};
    XShapeCombineRectangles(xdisplay_, xwindow_, ShapeBounding, 0, 0, &r, 1,
                            ShapeSet, YXBanded);
  }
}

void XWindow::NotifyBoundsChanged(const gfx::Rect& new_bounds_in_px) {
  ResetWindowRegion();
  OnXWindowBoundsChanged(new_bounds_in_px);
}

}  // namespace ui
