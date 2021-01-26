// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_window.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "net/base/network_interfaces.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test_x11.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/wm_role_names_linux.h"
#include "ui/base/x/x11_menu_registrar.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_path.h"
#include "ui/gfx/x/x11_window_event_manager.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/platform_window/common/platform_window_defaults.h"

namespace ui {

namespace {

// Special value of the _NET_WM_DESKTOP property which indicates that the window
// should appear on all workspaces/desktops.
const int32_t kAllWorkspaces = -1;

constexpr char kX11WindowRolePopup[] = "popup";
constexpr char kX11WindowRoleBubble[] = "bubble";
constexpr char kDarkGtkThemeVariant[] = "dark";

constexpr long kSystemTrayRequestDock = 0;

constexpr int kXembedInfoProtocolVersion = 0;
constexpr int kXembedFlagMap = 1 << 0;
constexpr int kXembedInfoFlags = kXembedFlagMap;

enum CrossingFlags : uint8_t {
  CROSSING_FLAG_FOCUS = 1 << 0,
  CROSSING_FLAG_SAME_SCREEN = 1 << 1,
};

// In some situations, views tries to make a zero sized window, and that
// makes us crash. Make sure we have valid sizes.
gfx::Rect SanitizeBounds(const gfx::Rect& bounds) {
  gfx::Size sanitized_size(std::max(bounds.width(), 1),
                           std::max(bounds.height(), 1));
  gfx::Rect sanitized_bounds(bounds.origin(), sanitized_size);
  return sanitized_bounds;
}

void SerializeImageRepresentation(const gfx::ImageSkiaRep& rep,
                                  std::vector<uint32_t>* data) {
  uint32_t width = rep.GetWidth();
  data->push_back(width);

  uint32_t height = rep.GetHeight();
  data->push_back(height);

  const SkBitmap& bitmap = rep.GetBitmap();

  for (uint32_t y = 0; y < height; ++y)
    for (uint32_t x = 0; x < width; ++x)
      data->push_back(bitmap.getColor(x, y));
}

x11::NotifyMode XI2ModeToXMode(x11::Input::NotifyMode xi2_mode) {
  switch (xi2_mode) {
    case x11::Input::NotifyMode::Normal:
      return x11::NotifyMode::Normal;
    case x11::Input::NotifyMode::Grab:
    case x11::Input::NotifyMode::PassiveGrab:
      return x11::NotifyMode::Grab;
    case x11::Input::NotifyMode::Ungrab:
    case x11::Input::NotifyMode::PassiveUngrab:
      return x11::NotifyMode::Ungrab;
    case x11::Input::NotifyMode::WhileGrabbed:
      return x11::NotifyMode::WhileGrabbed;
    default:
      NOTREACHED();
      return x11::NotifyMode::Normal;
  }
}

x11::NotifyDetail XI2DetailToXDetail(x11::Input::NotifyDetail xi2_detail) {
  switch (xi2_detail) {
    case x11::Input::NotifyDetail::Ancestor:
      return x11::NotifyDetail::Ancestor;
    case x11::Input::NotifyDetail::Virtual:
      return x11::NotifyDetail::Virtual;
    case x11::Input::NotifyDetail::Inferior:
      return x11::NotifyDetail::Inferior;
    case x11::Input::NotifyDetail::Nonlinear:
      return x11::NotifyDetail::Nonlinear;
    case x11::Input::NotifyDetail::NonlinearVirtual:
      return x11::NotifyDetail::NonlinearVirtual;
    case x11::Input::NotifyDetail::Pointer:
      return x11::NotifyDetail::Pointer;
    case x11::Input::NotifyDetail::PointerRoot:
      return x11::NotifyDetail::PointerRoot;
    case x11::Input::NotifyDetail::None:
      return x11::NotifyDetail::None;
  }
}

void SyncSetCounter(x11::Connection* connection,
                    x11::Sync::Counter counter,
                    int64_t value) {
  x11::Sync::Int64 sync_value{.hi = value >> 32, .lo = value & 0xFFFFFFFF};
  connection->sync().SetCounter({counter, sync_value});
}

// Returns the whole path from |window| to the root.
std::vector<x11::Window> GetParentsList(x11::Connection* connection,
                                        x11::Window window) {
  std::vector<x11::Window> result;
  while (window != x11::Window::None) {
    result.push_back(window);
    if (auto reply = connection->QueryTree({window}).Sync())
      window = reply->parent;
    else
      break;
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
    : connection_(x11::Connection::Get()), x_root_window_(GetX11RootWindow()) {
  DCHECK(connection_);
  DCHECK_NE(x_root_window_, x11::Window::None);
}

XWindow::~XWindow() {
  DCHECK_EQ(xwindow_, x11::Window::None)
      << "XWindow destructed without calling "
         "Close() to release allocated resources.";
}

void XWindow::Init(const Configuration& config) {
  // Ensure that the X11MenuRegistrar exists. The X11MenuRegistrar is
  // necessary to properly track menu windows.
  X11MenuRegistrar::Get();

  activatable_ = config.activatable;

  x11::CreateWindowRequest req;
  req.bit_gravity = x11::Gravity::NorthWest;
  req.background_pixel = config.background_color.has_value()
                             ? config.background_color.value()
                             : connection_->default_screen().white_pixel;

  x11::Atom window_type;
  switch (config.type) {
    case WindowType::kMenu:
      req.override_redirect = x11::Bool32(true);
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_MENU");
      break;
    case WindowType::kTooltip:
      req.override_redirect = x11::Bool32(true);
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_TOOLTIP");
      break;
    case WindowType::kPopup:
      req.override_redirect = x11::Bool32(true);
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_NOTIFICATION");
      break;
    case WindowType::kDrag:
      req.override_redirect = x11::Bool32(true);
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_DND");
      break;
    default:
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_NORMAL");
      break;
  }
  // An in-activatable window should not interact with the system wm.
  if (!activatable_ || config.override_redirect)
    req.override_redirect = x11::Bool32(true);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  req.override_redirect = x11::Bool32(UseTestConfigForPlatformWindows());
#endif

  override_redirect_ = req.override_redirect.has_value();

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

  if (config.wm_role_name == kStatusIconWmRoleName) {
    std::string atom_name =
        "_NET_SYSTEM_TRAY_S" +
        base::NumberToString(connection_->DefaultScreenId());
    auto selection = connection_->GetSelectionOwner({x11::GetAtom(atom_name)});
    if (auto reply = selection.Sync()) {
      GetProperty(reply->owner, x11::GetAtom("_NET_SYSTEM_TRAY_VISUAL"),
                  &visual_id_);
    }
  }

  x11::VisualId visual_id = visual_id_;
  uint8_t depth = 0;
  x11::ColorMap colormap{};
  XVisualManager* visual_manager = XVisualManager::GetInstance();
  if (visual_id_ == x11::VisualId{} ||
      !visual_manager->GetVisualInfo(visual_id_, &depth, &colormap,
                                     &visual_has_alpha_)) {
    visual_manager->ChooseVisualForWindow(enable_transparent_visuals,
                                          &visual_id, &depth, &colormap,
                                          &visual_has_alpha_);
  }

  // x.org will BadMatch if we don't set a border when the depth isn't the
  // same as the parent depth.
  req.border_pixel = 0;

  bounds_in_pixels_ = SanitizeBounds(config.bounds);
  req.parent = x_root_window_;
  req.x = bounds_in_pixels_.x();
  req.y = bounds_in_pixels_.y();
  req.width = bounds_in_pixels_.width();
  req.height = bounds_in_pixels_.height();
  req.depth = depth;
  req.c_class = x11::WindowClass::InputOutput;
  req.visual = visual_id;
  req.colormap = colormap;
  xwindow_ = connection_->GenerateId<x11::Window>();
  req.wid = xwindow_;
  connection_->CreateWindow(req);

  // It can be a status icon window. If it fails to initialize, don't provide
  // him with a native window handle, close self and let the client destroy
  // ourselves.
  if (config.wm_role_name == kStatusIconWmRoleName &&
      !InitializeAsStatusIcon()) {
    Close();
    return;
  }

  OnXWindowCreated();

  // TODO(erg): Maybe need to set a ViewProp here like in RWHL::RWHL().

  auto event_mask =
      x11::EventMask::ButtonPress | x11::EventMask::ButtonRelease |
      x11::EventMask::FocusChange | x11::EventMask::KeyPress |
      x11::EventMask::KeyRelease | x11::EventMask::EnterWindow |
      x11::EventMask::LeaveWindow | x11::EventMask::Exposure |
      x11::EventMask::VisibilityChange | x11::EventMask::StructureNotify |
      x11::EventMask::PropertyChange | x11::EventMask::PointerMotion;
  xwindow_events_ =
      std::make_unique<x11::XScopedEventSelector>(xwindow_, event_mask);
  connection_->Flush();

  if (IsXInput2Available())
    TouchFactory::GetInstance()->SetupXI2ForXWindow(xwindow_);

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
  std::vector<x11::Atom> protocols = {
      x11::GetAtom("WM_DELETE_WINDOW"),
      x11::GetAtom("_NET_WM_PING"),
      x11::GetAtom("_NET_WM_SYNC_REQUEST"),
  };
  SetArrayProperty(xwindow_, x11::GetAtom("WM_PROTOCOLS"), x11::Atom::ATOM,
                   protocols);

  // We need a WM_CLIENT_MACHINE value so we integrate with the desktop
  // environment.
  SetStringProperty(xwindow_, x11::Atom::WM_CLIENT_MACHINE, x11::Atom::STRING,
                    net::GetHostName());

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(uint32_t) >= sizeof(pid_t),
                "pid_t should not be larger than uint32_t");
  uint32_t pid = getpid();
  SetProperty(xwindow_, x11::GetAtom("_NET_WM_PID"), x11::Atom::CARDINAL, pid);

  SetProperty(xwindow_, x11::GetAtom("_NET_WM_WINDOW_TYPE"), x11::Atom::ATOM,
              window_type);

  // The changes to |window_properties_| here will be sent to the X server just
  // before the window is mapped.

  // Remove popup windows from taskbar unless overridden.
  if ((config.type == WindowType::kPopup ||
       config.type == WindowType::kBubble) &&
      !config.force_show_in_taskbar) {
    window_properties_.insert(x11::GetAtom("_NET_WM_STATE_SKIP_TASKBAR"));
  }

  // If the window should stay on top of other windows, add the
  // _NET_WM_STATE_ABOVE property.
  is_always_on_top_ = config.keep_on_top;
  if (is_always_on_top_)
    window_properties_.insert(x11::GetAtom("_NET_WM_STATE_ABOVE"));

  workspace_ = base::nullopt;
  if (config.visible_on_all_workspaces) {
    window_properties_.insert(x11::GetAtom("_NET_WM_STATE_STICKY"));
    SetProperty(xwindow_, x11::GetAtom("_NET_WM_DESKTOP"), x11::Atom::CARDINAL,
                kAllWorkspaces);
  } else if (!config.workspace.empty()) {
    int32_t workspace;
    if (base::StringToInt(config.workspace, &workspace))
      SetProperty(xwindow_, x11::GetAtom("_NET_WM_DESKTOP"),
                  x11::Atom::CARDINAL, workspace);
  }

  if (!config.wm_class_name.empty() || !config.wm_class_class.empty()) {
    SetWindowClassHint(connection_, xwindow_, config.wm_class_name,
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
    SetWindowRole(xwindow_, std::string(wm_role_name));

  if (config.remove_standard_frame) {
    // Setting _GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED tells gnome-shell to not force
    // fullscreen on the window when it matches the desktop size.
    SetHideTitlebarWhenMaximizedProperty(xwindow_,
                                         HIDE_TITLEBAR_WHEN_MAXIMIZED);
  }

  if (config.prefer_dark_theme) {
    SetStringProperty(xwindow_, x11::GetAtom("_GTK_THEME_VARIANT"),
                      x11::GetAtom("UTF8_STRING"), kDarkGtkThemeVariant);
  }

  if (IsSyncExtensionAvailable()) {
    x11::Sync::Int64 value{};
    update_counter_ = connection_->GenerateId<x11::Sync::Counter>();
    connection_->sync().CreateCounter({update_counter_, value});
    extended_update_counter_ = connection_->GenerateId<x11::Sync::Counter>();
    connection_->sync().CreateCounter({extended_update_counter_, value});

    std::vector<x11::Sync::Counter> counters{update_counter_,
                                             extended_update_counter_};

    // Set XSyncCounter as window property _NET_WM_SYNC_REQUEST_COUNTER. the
    // compositor will listen on them during resizing.
    SetArrayProperty(xwindow_, x11::GetAtom("_NET_WM_SYNC_REQUEST_COUNTER"),
                     x11::Atom::CARDINAL, counters);
  }

  // Always composite Chromium windows if a compositing WM is used.  Sometimes,
  // WMs will not composite fullscreen windows as an optimization, but this can
  // lead to tearing of fullscreen videos.
  x11::SetProperty<uint32_t>(xwindow_,
                             x11::GetAtom("_NET_WM_BYPASS_COMPOSITOR"),
                             x11::Atom::CARDINAL, 2);

  if (config.icon)
    SetXWindowIcons(gfx::ImageSkia(), *config.icon);
}

void XWindow::Map(bool inactive) {
  // Before we map the window, set size hints. Otherwise, some window managers
  // will ignore toplevel XMoveWindow commands.
  SizeHints size_hints;
  memset(&size_hints, 0, sizeof(size_hints));
  GetWmNormalHints(xwindow_, &size_hints);
  size_hints.flags |= SIZE_HINT_P_POSITION;
  size_hints.x = bounds_in_pixels_.x();
  size_hints.y = bounds_in_pixels_.y();
  SetWmNormalHints(xwindow_, size_hints);

  ignore_keyboard_input_ = inactive;
  auto wm_user_time_ms = ignore_keyboard_input_
                             ? x11::Time::CurrentTime
                             : X11EventSource::GetInstance()->GetTimestamp();
  if (inactive || wm_user_time_ms != x11::Time::CurrentTime) {
    SetProperty(xwindow_, x11::GetAtom("_NET_WM_USER_TIME"),
                x11::Atom::CARDINAL, wm_user_time_ms);
  }

  UpdateMinAndMaxSize();

  if (window_properties_.empty()) {
    x11::DeleteProperty(xwindow_, x11::GetAtom("_NET_WM_STATE"));
  } else {
    SetArrayProperty(xwindow_, x11::GetAtom("_NET_WM_STATE"), x11::Atom::ATOM,
                     std::vector<x11::Atom>(std::begin(window_properties_),
                                            std::end(window_properties_)));
  }

  connection_->MapWindow({xwindow_});
  window_mapped_in_client_ = true;

  // TODO(thomasanderson): Find out why this flush is necessary.
  connection_->Flush();
}

void XWindow::Close() {
  if (xwindow_ == x11::Window::None)
    return;

  CancelResize();
  UnconfineCursor();

  connection_->DestroyWindow({xwindow_});
  xwindow_ = x11::Window::None;

  if (update_counter_ != x11::Sync::Counter{}) {
    connection_->sync().DestroyCounter({update_counter_});
    connection_->sync().DestroyCounter({extended_update_counter_});
    update_counter_ = {};
    extended_update_counter_ = {};
  }
}

void XWindow::Maximize() {
  // Some WMs do not respect maximization hints on unmapped windows, so we
  // save this one for later too.
  should_maximize_after_map_ = !window_mapped_in_client_;

  SetWMSpecState(true, x11::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 x11::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

void XWindow::Minimize() {
  if (window_mapped_in_client_) {
    SendClientMessage(xwindow_, x_root_window_, x11::GetAtom("WM_CHANGE_STATE"),
                      {WM_STATE_ICONIC, 0, 0, 0, 0});
  } else {
    SetWMSpecState(true, x11::GetAtom("_NET_WM_STATE_HIDDEN"), x11::Atom::None);
  }
}

void XWindow::Unmaximize() {
  should_maximize_after_map_ = false;
  SetWMSpecState(false, x11::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 x11::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

bool XWindow::Hide() {
  if (!window_mapped_in_client_)
    return false;

  // Make sure no resize task will run after the window is unmapped.
  CancelResize();

  WithdrawWindow(xwindow_);
  window_mapped_in_client_ = false;
  return true;
}

void XWindow::Unhide() {
  SetWMSpecState(false, x11::GetAtom("_NET_WM_STATE_HIDDEN"), x11::Atom::None);
}

void XWindow::SetFullscreen(bool fullscreen) {
  SetWMSpecState(fullscreen, x11::GetAtom("_NET_WM_STATE_FULLSCREEN"),
                 x11::Atom::None);
}

void XWindow::Activate() {
  if (!IsXWindowVisible() || !activatable_)
    return;

  BeforeActivationStateChanged();

  ignore_keyboard_input_ = false;

  // wmii says that it supports _NET_ACTIVE_WINDOW but does not.
  // https://code.google.com/p/wmii/issues/detail?id=266
  static bool wm_supports_active_window =
      GuessWindowManager() != WM_WMII &&
      WmSupportsHint(x11::GetAtom("_NET_ACTIVE_WINDOW"));

  x11::Time timestamp = X11EventSource::GetInstance()->GetTimestamp();

  // override_redirect windows ignore _NET_ACTIVE_WINDOW.
  // https://crbug.com/940924
  if (wm_supports_active_window && !override_redirect_) {
    std::array<uint32_t, 5> data = {
        // We're an app.
        1,
        static_cast<uint32_t>(timestamp),
        // TODO(thomasanderson): if another chrome window is active, specify
        // that here.  The EWMH spec claims this may make the WM more likely to
        // service our _NET_ACTIVE_WINDOW request.
        0,
        0,
        0,
    };
    SendClientMessage(xwindow_, x_root_window_,
                      x11::GetAtom("_NET_ACTIVE_WINDOW"), data);
  } else {
    RaiseWindow(xwindow_);
    // Directly ask the X server to give focus to the window. Note that the call
    // would have raised an X error if the window is not mapped.
    connection_
        ->SetInputFocus({x11::InputFocus::Parent, xwindow_,
                         static_cast<x11::Time>(timestamp)})
        .IgnoreError();
    // At this point, we know we will receive focus, and some webdriver tests
    // depend on a window being IsActive() immediately after an Activate(), so
    // just set this state now.
    has_pointer_focus_ = false;
    has_window_focus_ = true;
    window_mapped_in_server_ = true;
  }

  AfterActivationStateChanged();
}

void XWindow::Deactivate() {
  BeforeActivationStateChanged();

  // Ignore future input events.
  ignore_keyboard_input_ = true;

  ui::LowerWindow(xwindow_);

  AfterActivationStateChanged();
}

bool XWindow::IsActive() const {
  // Focus and stacking order are independent in X11.  Since we cannot guarantee
  // a window is topmost iff it has focus, just use the focus state to determine
  // if a window is active.  Note that Activate() and Deactivate() change the
  // stacking order in addition to changing the focus state.
  return (has_window_focus_ || has_pointer_focus_) && !ignore_keyboard_input_;
}

void XWindow::SetSize(const gfx::Size& size_in_pixels) {
  connection_->ConfigureWindow({.window = xwindow_,
                                .width = size_in_pixels.width(),
                                .height = size_in_pixels.height()});
  bounds_in_pixels_.set_size(size_in_pixels);
}

void XWindow::SetBounds(const gfx::Rect& requested_bounds_in_pixels) {
  gfx::Rect bounds_in_pixels(requested_bounds_in_pixels);
  bool origin_changed = bounds_in_pixels_.origin() != bounds_in_pixels.origin();
  bool size_changed = bounds_in_pixels_.size() != bounds_in_pixels.size();

  x11::ConfigureWindowRequest req{.window = xwindow_};

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

    req.width = bounds_in_pixels.width();
    req.height = bounds_in_pixels.height();
  }

  if (origin_changed) {
    req.x = bounds_in_pixels.x();
    req.y = bounds_in_pixels.y();
  }

  if (origin_changed || size_changed)
    connection_->ConfigureWindow(req);

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_in_pixels_| later.
  bounds_in_pixels_ = bounds_in_pixels;
  ResetWindowRegion();

  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  OnXWindowBoundsChanged(bounds_in_pixels);
}

bool XWindow::IsXWindowVisible() const {
  // On Windows, IsVisible() returns true for minimized windows.  On X11, a
  // minimized window is not mapped, so an explicit IsMinimized() check is
  // necessary.
  return window_mapped_in_client_ || IsMinimized();
}

bool XWindow::IsMinimized() const {
  return HasWMSpecProperty(window_properties_,
                           x11::GetAtom("_NET_WM_STATE_HIDDEN"));
}

bool XWindow::IsMaximized() const {
  return (HasWMSpecProperty(window_properties_,
                            x11::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT")) &&
          HasWMSpecProperty(window_properties_,
                            x11::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ")));
}

bool XWindow::IsFullscreen() const {
  return HasWMSpecProperty(window_properties_,
                           x11::GetAtom("_NET_WM_STATE_FULLSCREEN"));
}

gfx::Rect XWindow::GetOuterBounds() const {
  gfx::Rect outer_bounds(bounds_in_pixels_);
  outer_bounds.Inset(-native_window_frame_borders_in_pixels_);
  return outer_bounds;
}

void XWindow::GrabPointer() {
  // If the pointer is already in |xwindow_|, we will not get a crossing event
  // with a mode of NotifyGrab, so we must record the grab state manually.
  has_pointer_grab_ |=
      (ui::GrabPointer(xwindow_, true, nullptr) == x11::GrabStatus::Success);
}

void XWindow::ReleasePointerGrab() {
  UngrabPointer();
  has_pointer_grab_ = false;
}

void XWindow::StackXWindowAbove(x11::Window window) {
  DCHECK(window != x11::Window::None);

  // Find all parent windows up to the root.
  std::vector<x11::Window> window_below_parents =
      GetParentsList(connection_, window);
  std::vector<x11::Window> window_above_parents =
      GetParentsList(connection_, xwindow_);

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
    connection_->ConfigureWindow(x11::ConfigureWindowRequest{
        .window = *it_above_window,
        .sibling = *it_below_window,
        .stack_mode = x11::StackMode::Above,
    });
  }
}

void XWindow::StackXWindowAtTop() {
  RaiseWindow(xwindow_);
}

void XWindow::SetCursor(scoped_refptr<X11Cursor> cursor) {
  last_cursor_ = cursor;
  on_cursor_loaded_.Reset(base::BindOnce(DefineCursor, xwindow_));
  if (cursor)
    cursor->OnCursorLoaded(on_cursor_loaded_.callback());
}

bool XWindow::SetTitle(base::string16 title) {
  if (window_title_ == title)
    return false;

  window_title_ = title;
  std::string utf8str = base::UTF16ToUTF8(title);
  SetStringProperty(xwindow_, x11::GetAtom("_NET_WM_NAME"),
                    x11::GetAtom("UTF8_STRING"), utf8str);
  SetStringProperty(xwindow_, x11::Atom::WM_NAME, x11::GetAtom("UTF8_STRING"),
                    utf8str);
  return true;
}

void XWindow::SetXWindowOpacity(float opacity) {
  // X server opacity is in terms of 32 bit unsigned int space, and counts from
  // the opposite direction.
  // XChangeProperty() expects "cardinality" to be long.

  // Scale opacity to [0 .. 255] range.
  uint32_t opacity_8bit = static_cast<uint32_t>(opacity * 255.0f) & 0xFF;
  // Use opacity value for all channels.
  uint32_t channel_multiplier = 0x1010101;
  uint32_t cardinality = opacity_8bit * channel_multiplier;

  if (cardinality == 0xffffffff) {
    x11::DeleteProperty(xwindow_, x11::GetAtom("_NET_WM_WINDOW_OPACITY"));
  } else {
    SetProperty(xwindow_, x11::GetAtom("_NET_WM_WINDOW_OPACITY"),
                x11::Atom::CARDINAL, cardinality);
  }
}

void XWindow::SetXWindowAspectRatio(const gfx::SizeF& aspect_ratio) {
  SizeHints size_hints;
  memset(&size_hints, 0, sizeof(size_hints));

  GetWmNormalHints(xwindow_, &size_hints);
  // Unforce aspect ratio is parameter length is 0, otherwise set normally.
  if (aspect_ratio.IsEmpty()) {
    size_hints.flags &= ~SIZE_HINT_P_ASPECT;
  } else {
    size_hints.flags |= SIZE_HINT_P_ASPECT;
    size_hints.min_aspect_num = size_hints.max_aspect_num =
        aspect_ratio.width();
    size_hints.min_aspect_den = size_hints.max_aspect_den =
        aspect_ratio.height();
  }
  SetWmNormalHints(xwindow_, size_hints);
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
  std::vector<uint32_t> data;

  if (!window_icon.isNull())
    SerializeImageRepresentation(window_icon.GetRepresentation(1.0f), &data);

  if (!app_icon.isNull())
    SerializeImageRepresentation(app_icon.GetRepresentation(1.0f), &data);

  if (!data.empty()) {
    SetArrayProperty(xwindow_, x11::GetAtom("_NET_WM_ICON"),
                     x11::Atom::CARDINAL, data);
  }
}

void XWindow::SetXWindowVisibleOnAllWorkspaces(bool visible) {
  SetWMSpecState(visible, x11::GetAtom("_NET_WM_STATE_STICKY"),
                 x11::Atom::None);

  int new_desktop = 0;
  if (visible) {
    new_desktop = kAllWorkspaces;
  } else {
    if (!GetCurrentDesktop(&new_desktop))
      return;
  }

  workspace_ = kAllWorkspaces;
  SendClientMessage(xwindow_, x_root_window_, x11::GetAtom("_NET_WM_DESKTOP"),
                    {new_desktop, 0, 0, 0, 0});
}

bool XWindow::IsXWindowVisibleOnAllWorkspaces() const {
  // We don't need a check for _NET_WM_STATE_STICKY because that would specify
  // that the window remain in a fixed position even if the viewport scrolls.
  // This is different from the type of workspace that's associated with
  // _NET_WM_DESKTOP.
  return workspace_ == kAllWorkspaces;
}

void XWindow::MoveCursorTo(const gfx::Point& location_in_pixels) {
  connection_->WarpPointer(x11::WarpPointerRequest{
      .dst_window = x_root_window_,
      .dst_x = bounds_in_pixels_.x() + location_in_pixels.x(),
      .dst_y = bounds_in_pixels_.y() + location_in_pixels.y(),
  });
}

void XWindow::ResetWindowRegion() {
  std::unique_ptr<std::vector<x11::Rectangle>> xregion;
  if (!use_custom_shape() && !IsMaximized() && !IsFullscreen()) {
    SkPath window_mask = GetWindowMaskForXWindow();
    // Some frame views define a custom (non-rectangular) window mask. If
    // so, use it to define the window shape. If not, fall through.
    if (window_mask.countPoints() > 0)
      xregion = x11::CreateRegionFromSkPath(window_mask);
  }
  UpdateWindowRegion(std::move(xregion));
}

void XWindow::OnWorkspaceUpdated() {
  auto old_workspace = workspace_;
  int workspace;
  if (GetWindowDesktop(xwindow_, &workspace))
    workspace_ = workspace;
  else
    workspace_ = base::nullopt;

  if (workspace_ != old_workspace)
    OnXWindowWorkspaceChanged();
}

void XWindow::SetAlwaysOnTop(bool always_on_top) {
  is_always_on_top_ = always_on_top;
  SetWMSpecState(always_on_top, x11::GetAtom("_NET_WM_STATE_ABOVE"),
                 x11::Atom::None);
}

void XWindow::SetFlashFrameHint(bool flash_frame) {
  if (urgency_hint_set_ == flash_frame)
    return;

  WmHints hints;
  memset(&hints, 0, sizeof(hints));
  GetWmHints(xwindow_, &hints);

  if (flash_frame)
    hints.flags |= WM_HINT_X_URGENCY;
  else
    hints.flags &= ~WM_HINT_X_URGENCY;

  SetWmHints(xwindow_, hints);

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

  SizeHints hints;
  memset(&hints, 0, sizeof(hints));
  GetWmNormalHints(xwindow_, &hints);

  if (min_size_in_pixels_.IsEmpty()) {
    hints.flags &= ~SIZE_HINT_P_MIN_SIZE;
  } else {
    hints.flags |= SIZE_HINT_P_MIN_SIZE;
    hints.min_width = min_size_in_pixels_.width();
    hints.min_height = min_size_in_pixels_.height();
  }

  if (max_size_in_pixels_.IsEmpty()) {
    hints.flags &= ~SIZE_HINT_P_MAX_SIZE;
  } else {
    hints.flags |= SIZE_HINT_P_MAX_SIZE;
    hints.max_width = max_size_in_pixels_.width();
    hints.max_height = max_size_in_pixels_.height();
  }

  SetWmNormalHints(xwindow_, hints);
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
  SetUseOSWindowFrame(xwindow_, use_native_frame);
  ResetWindowRegion();
}

void XWindow::OnCrossingEvent(bool enter,
                              bool focus_in_window_or_ancestor,
                              x11::NotifyMode mode,
                              x11::NotifyDetail detail) {
  // NotifyInferior on a crossing event means the pointer moved into or out of a
  // child window, but the pointer is still within |xwindow_|.
  if (detail == x11::NotifyDetail::Inferior)
    return;

  BeforeActivationStateChanged();

  if (mode == x11::NotifyMode::Grab)
    has_pointer_grab_ = enter;
  else if (mode == x11::NotifyMode::Ungrab)
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

void XWindow::OnFocusEvent(bool focus_in,
                           x11::NotifyMode mode,
                           x11::NotifyDetail detail) {
  // NotifyInferior on a focus event means the focus moved into or out of a
  // child window, but the focus is still within |xwindow_|.
  if (detail == x11::NotifyDetail::Inferior)
    return;

  bool notify_grab =
      mode == x11::NotifyMode::Grab || mode == x11::NotifyMode::Ungrab;

  BeforeActivationStateChanged();

  // For every focus change, the X server sends normal focus events which are
  // useful for tracking |has_window_focus_|, but supplements these events with
  // NotifyPointer events which are only useful for tracking pointer focus.

  // For |has_pointer_focus_| and |has_window_focus_|, we continue tracking
  // state during a grab, but ignore grab/ungrab events themselves.
  if (!notify_grab && detail != x11::NotifyDetail::Pointer)
    has_window_focus_ = focus_in;

  if (!notify_grab && has_pointer_) {
    switch (detail) {
      case x11::NotifyDetail::Ancestor:
      case x11::NotifyDetail::Virtual:
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
      case x11::NotifyDetail::Pointer:
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
      case x11::NotifyDetail::Nonlinear:
      case x11::NotifyDetail::NonlinearVirtual:
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

bool XWindow::IsTargetedBy(const x11::Event& x11_event) const {
  return x11_event.window() == xwindow_;
}

bool XWindow::IsTransientWindowTargetedBy(const x11::Event& x11_event) const {
  return x11_event.window() == transient_window_;
}

void XWindow::SetTransientWindow(x11::Window window) {
  transient_window_ = window;
}

void XWindow::WmMoveResize(int hittest, const gfx::Point& location) const {
  int direction = HitTestToWmMoveResizeDirection(hittest);
  if (direction == -1)
    return;

  DoWMMoveResize(connection_, x_root_window_, xwindow_, location, direction);
}

void XWindow::OnEvent(const x11::Event& xev) {
  // We can lose track of the window's position when the window is reparented.
  // When the parent window is moved, we won't get an event, so the window's
  // position relative to the root window will get out-of-sync.  We can re-sync
  // when getting pointer events (EnterNotify, LeaveNotify, ButtonPress,
  // ButtonRelease, MotionNotify) which include the pointer location both
  // relative to this window and relative to the root window, so we can
  // calculate this window's position from that information.
  gfx::Point window_point = EventLocationFromXEvent(xev);
  gfx::Point root_point = EventSystemLocationFromXEvent(xev);
  if (!window_point.IsOrigin() && !root_point.IsOrigin()) {
    gfx::Point window_origin = gfx::Point() + (root_point - window_point);
    if (bounds_in_pixels_.origin() != window_origin) {
      bounds_in_pixels_.set_origin(window_origin);
      NotifyBoundsChanged(bounds_in_pixels_);
    }
  }

  // May want to factor CheckXEventForConsistency(xev); into a common location
  // since it is called here.
  if (auto* crossing = xev.As<x11::CrossingEvent>()) {
    bool focus = crossing->same_screen_focus & CROSSING_FLAG_FOCUS;
    OnCrossingEvent(crossing->opcode == x11::CrossingEvent::EnterNotify, focus,
                    crossing->mode, crossing->detail);
  } else if (auto* expose = xev.As<x11::ExposeEvent>()) {
    gfx::Rect damage_rect_in_pixels(expose->x, expose->y, expose->width,
                                    expose->height);
    OnXWindowDamageEvent(damage_rect_in_pixels);
  } else if (auto* focus = xev.As<x11::FocusEvent>()) {
    OnFocusEvent(focus->opcode == x11::FocusEvent::In, focus->mode,
                 focus->detail);
  } else if (auto* configure = xev.As<x11::ConfigureNotifyEvent>()) {
    OnConfigureEvent(*configure);
  } else if (auto* crossing = xev.As<x11::Input::CrossingEvent>()) {
    TouchFactory* factory = TouchFactory::GetInstance();
    if (factory->ShouldProcessCrossingEvent(*crossing)) {
      auto mode = XI2ModeToXMode(crossing->mode);
      auto detail = XI2DetailToXDetail(crossing->detail);
      switch (crossing->opcode) {
        case x11::Input::CrossingEvent::Enter:
          OnCrossingEvent(true, crossing->focus, mode, detail);
          break;
        case x11::Input::CrossingEvent::Leave:
          OnCrossingEvent(false, crossing->focus, mode, detail);
          break;
        case x11::Input::CrossingEvent::FocusIn:
          OnFocusEvent(true, mode, detail);
          break;
        case x11::Input::CrossingEvent::FocusOut:
          OnFocusEvent(false, mode, detail);
          break;
      }
    }
  } else if (xev.As<x11::MapNotifyEvent>()) {
    OnWindowMapped();
  } else if (xev.As<x11::UnmapNotifyEvent>()) {
    window_mapped_in_server_ = false;
    has_pointer_ = false;
    has_pointer_grab_ = false;
    has_pointer_focus_ = false;
    has_window_focus_ = false;
  } else if (auto* client = xev.As<x11::ClientMessageEvent>()) {
    x11::Atom message_type = client->type;
    if (message_type == x11::GetAtom("WM_PROTOCOLS")) {
      x11::Atom protocol = static_cast<x11::Atom>(client->data.data32[0]);
      if (protocol == x11::GetAtom("WM_DELETE_WINDOW")) {
        // We have received a close message from the window manager.
        OnXWindowCloseRequested();
      } else if (protocol == x11::GetAtom("_NET_WM_PING")) {
        x11::ClientMessageEvent reply_event = *client;
        reply_event.window = x_root_window_;
        x11::SendEvent(reply_event, x_root_window_,
                       x11::EventMask::SubstructureNotify |
                           x11::EventMask::SubstructureRedirect);
      } else if (protocol == x11::GetAtom("_NET_WM_SYNC_REQUEST")) {
        pending_counter_value_ =
            client->data.data32[2] +
            (static_cast<int64_t>(client->data.data32[3]) << 32);
        pending_counter_value_is_extended_ = client->data.data32[4] != 0;
      }
    } else {
      OnXWindowDragDropEvent(*client);
    }
  } else if (auto* property = xev.As<x11::PropertyNotifyEvent>()) {
    x11::Atom changed_atom = property->atom;
    if (changed_atom == x11::GetAtom("_NET_WM_STATE"))
      OnWMStateUpdated();
    else if (changed_atom == x11::GetAtom("_NET_FRAME_EXTENTS"))
      OnFrameExtentsUpdated();
    else if (changed_atom == x11::GetAtom("_NET_WM_DESKTOP"))
      OnWorkspaceUpdated();
  } else if (auto* selection = xev.As<x11::SelectionNotifyEvent>()) {
    OnXWindowSelectionEvent(*selection);
  }
}

void XWindow::UpdateWMUserTime(Event* event) {
  if (!IsActive())
    return;
  DCHECK(event);
  EventType type = event->type();
  if (type == ET_MOUSE_PRESSED || type == ET_KEY_PRESSED ||
      type == ET_TOUCH_PRESSED) {
    uint32_t wm_user_time_ms =
        (event->time_stamp() - base::TimeTicks()).InMilliseconds();
    SetProperty(xwindow_, x11::GetAtom("_NET_WM_USER_TIME"),
                x11::Atom::CARDINAL, wm_user_time_ms);
  }
}

void XWindow::OnWindowMapped() {
  window_mapped_in_server_ = true;
  // Some WMs only respect maximize hints after the window has been mapped.
  // Check whether we need to re-do a maximization.
  if (should_maximize_after_map_) {
    Maximize();
    should_maximize_after_map_ = false;
  }
}

void XWindow::OnConfigureEvent(const x11::ConfigureNotifyEvent& configure) {
  DCHECK_EQ(xwindow_, configure.window);
  DCHECK_EQ(xwindow_, configure.event);

  if (pending_counter_value_) {
    DCHECK(!configure_counter_value_);
    configure_counter_value_ = pending_counter_value_;
    configure_counter_value_is_extended_ = pending_counter_value_is_extended_;
    pending_counter_value_is_extended_ = false;
    pending_counter_value_ = 0;
  }

  // It's possible that the X window may be resized by some other means than
  // from within aura (e.g. the X window manager can change the size). Make
  // sure the root window size is maintained properly.
  int translated_x_in_pixels = configure.x;
  int translated_y_in_pixels = configure.y;
  if (!configure.send_event && !configure.override_redirect) {
    auto future =
        connection_->TranslateCoordinates({xwindow_, x_root_window_, 0, 0});
    if (auto coords = future.Sync()) {
      translated_x_in_pixels = coords->dst_x;
      translated_y_in_pixels = coords->dst_y;
    }
  }
  gfx::Rect bounds_in_pixels(translated_x_in_pixels, translated_y_in_pixels,
                             configure.width, configure.height);
  bool size_changed = bounds_in_pixels_.size() != bounds_in_pixels.size();
  bool origin_changed = bounds_in_pixels_.origin() != bounds_in_pixels.origin();
  previous_bounds_in_pixels_ = bounds_in_pixels_;
  bounds_in_pixels_ = bounds_in_pixels;

  if (size_changed)
    DispatchResize();
  else if (origin_changed)
    NotifyBoundsChanged(bounds_in_pixels_);
}

void XWindow::SetWMSpecState(bool enabled, x11::Atom state1, x11::Atom state2) {
  if (window_mapped_in_client_) {
    ui::SetWMSpecState(xwindow_, enabled, state1, state2);
  } else {
    // The updated state will be set when the window is (re)mapped.
    base::flat_set<x11::Atom> new_window_properties = window_properties_;
    for (x11::Atom atom : {state1, state2}) {
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
  std::vector<x11::Atom> atom_list;
  if (GetArrayProperty(xwindow_, x11::GetAtom("_NET_WM_STATE"), &atom_list) ||
      window_mapped_in_client_) {
    UpdateWindowProperties(
        base::flat_set<x11::Atom>(std::begin(atom_list), std::end(atom_list)));
  }
}

void XWindow::UpdateWindowProperties(
    const base::flat_set<x11::Atom>& new_window_properties) {
  was_minimized_ = IsMinimized();

  window_properties_ = new_window_properties;

  // Ignore requests by the window manager to enter or exit fullscreen (e.g. as
  // a result of pressing a window manager accelerator key). Chrome does not
  // handle window manager initiated fullscreen. In particular, Chrome needs to
  // do preprocessing before the x window's fullscreen state is toggled.

  is_always_on_top_ = HasWMSpecProperty(window_properties_,
                                        x11::GetAtom("_NET_WM_STATE_ABOVE"));
  OnXWindowStateChanged();
  ResetWindowRegion();
}

void XWindow::OnFrameExtentsUpdated() {
  std::vector<int32_t> insets;
  if (GetArrayProperty(xwindow_, x11::GetAtom("_NET_FRAME_EXTENTS"), &insets) &&
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
      SyncSetCounter(connection_, extended_update_counter_,
                     current_counter_value_);
    }
    return;
  }

  if (configure_counter_value_ != 0) {
    SyncSetCounter(connection_, update_counter_, configure_counter_value_);
    configure_counter_value_ = 0;
  }
}

// Removes |delayed_resize_task_| from the task queue (if it's in the queue) and
// adds it back at the end of the queue.
void XWindow::DispatchResize() {
  if (update_counter_ == x11::Sync::Counter{} ||
      configure_counter_value_ == 0) {
    // WM doesn't support _NET_WM_SYNC_REQUEST. Or we are too slow, so
    // _NET_WM_SYNC_REQUEST is disabled by the compositor.
    delayed_resize_task_.Reset(base::BindOnce(
        &XWindow::DelayedResize, base::Unretained(this), bounds_in_pixels_));
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
    SyncSetCounter(connection_, extended_update_counter_,
                   ++current_counter_value_);
  }

  CancelResize();
  NotifyBoundsChanged(bounds_in_pixels);

  // No more member accesses here: bounds change propagation may have deleted
  // |this| (e.g. when a chrome window is snapped into a tab strip. Further
  // details at crbug.com/1068755).
}

void XWindow::CancelResize() {
  delayed_resize_task_.Cancel();
}

void XWindow::ConfineCursorTo(const gfx::Rect& bounds) {
  UnconfineCursor();

  if (bounds.IsEmpty())
    return;

  gfx::Rect barrier = bounds + bounds_in_pixels_.OffsetFromOrigin();

  auto make_barrier = [&](uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                          x11::XFixes::BarrierDirections directions) {
    x11::XFixes::Barrier barrier =
        connection_->GenerateId<x11::XFixes::Barrier>();
    connection_->xfixes().CreatePointerBarrier(
        {barrier, x_root_window_, x1, y1, x2, y2, directions});
    return barrier;
  };

  // Top horizontal barrier.
  pointer_barriers_[0] =
      make_barrier(barrier.x(), barrier.y(), barrier.right(), barrier.y(),
                   x11::XFixes::BarrierDirections::PositiveY);
  // Bottom horizontal barrier.
  pointer_barriers_[1] =
      make_barrier(barrier.x(), barrier.bottom(), barrier.right(),
                   barrier.bottom(), x11::XFixes::BarrierDirections::NegativeY);
  // Left vertical barrier.
  pointer_barriers_[2] =
      make_barrier(barrier.x(), barrier.y(), barrier.x(), barrier.bottom(),
                   x11::XFixes::BarrierDirections::PositiveX);
  // Right vertical barrier.
  pointer_barriers_[3] =
      make_barrier(barrier.right(), barrier.y(), barrier.right(),
                   barrier.bottom(), x11::XFixes::BarrierDirections::NegativeX);

  has_pointer_barriers_ = true;
}

void XWindow::LowerWindow() {
  ui::LowerWindow(xwindow_);
}

void XWindow::SetOverrideRedirect(bool override_redirect) {
  bool remap = window_mapped_in_client_;
  if (remap)
    Hide();
  connection_->ChangeWindowAttributes(x11::ChangeWindowAttributesRequest{
      .window = xwindow_,
      .override_redirect = x11::Bool32(override_redirect),
  });
  if (remap) {
    Map();
    if (has_pointer_grab_)
      ChangeActivePointerGrabCursor(nullptr);
  }
}

bool XWindow::ContainsPointInRegion(const gfx::Point& point) const {
  if (!shape())
    return true;

  for (const auto& rect : *shape()) {
    if (gfx::Rect(rect.x, rect.y, rect.width, rect.height).Contains(point))
      return true;
  }
  return false;
}

void XWindow::SetXWindowShape(std::unique_ptr<NativeShapeRects> native_shape,
                              const gfx::Transform& transform) {
  std::unique_ptr<std::vector<x11::Rectangle>> xregion;
  if (native_shape) {
    SkRegion native_region;
    for (const gfx::Rect& rect : *native_shape)
      native_region.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
    if (!transform.IsIdentity() && !native_region.isEmpty()) {
      SkPath path_in_dip;
      if (native_region.getBoundaryPath(&path_in_dip)) {
        SkPath path_in_pixels;
        path_in_dip.transform(SkMatrix(transform.matrix()), &path_in_pixels);
        xregion = x11::CreateRegionFromSkPath(path_in_pixels);
      } else {
        xregion = std::make_unique<std::vector<x11::Rectangle>>();
      }
    } else {
      xregion = x11::CreateRegionFromSkRegion(native_region);
    }
  }

  custom_window_shape_ = !!xregion;
  window_shape_ = std::move(xregion);
  ResetWindowRegion();
}

void XWindow::UnconfineCursor() {
  if (!has_pointer_barriers_)
    return;

  for (auto pointer_barrier : pointer_barriers_)
    connection_->xfixes().DeletePointerBarrier({pointer_barrier});

  pointer_barriers_.fill({});

  has_pointer_barriers_ = false;
}

void XWindow::UpdateWindowRegion(
    std::unique_ptr<std::vector<x11::Rectangle>> region) {
  auto set_shape = [&](const std::vector<x11::Rectangle>& rectangles) {
    connection_->shape().Rectangles(x11::Shape::RectanglesRequest{
        .operation = x11::Shape::So::Set,
        .destination_kind = x11::Shape::Sk::Bounding,
        .ordering = x11::ClipOrdering::YXBanded,
        .destination_window = xwindow_,
        .rectangles = rectangles,
    });
  };

  // If a custom window shape was supplied then apply it.
  if (use_custom_shape()) {
    set_shape(*window_shape_);
    return;
  }

  window_shape_ = std::move(region);
  if (window_shape_) {
    set_shape(*window_shape_);
    return;
  }

  // If we didn't set the shape for any reason, reset the shaping information.
  // How this is done depends on the border style, due to quirks and bugs in
  // various window managers.
  if (use_native_frame()) {
    // If the window has system borders, the mask must be set to null (not a
    // rectangle), because several window managers (eg, KDE, XFCE, XMonad) will
    // not put borders on a window with a custom shape.
    connection_->shape().Mask(x11::Shape::MaskRequest{
        .operation = x11::Shape::So::Set,
        .destination_kind = x11::Shape::Sk::Bounding,
        .destination_window = xwindow_,
        .source_bitmap = x11::Pixmap::None,
    });
  } else {
    // Conversely, if the window does not have system borders, the mask must be
    // manually set to a rectangle that covers the whole window (not null). This
    // is due to a bug in KWin <= 4.11.5 (KDE bug #330573) where setting a null
    // shape causes the hint to disable system borders to be ignored (resulting
    // in a double border).
    x11::Rectangle r{0, 0, bounds_in_pixels_.width(),
                     bounds_in_pixels_.height()};
    set_shape({r});
  }
}

void XWindow::NotifyBoundsChanged(const gfx::Rect& new_bounds_in_px) {
  ResetWindowRegion();
  OnXWindowBoundsChanged(new_bounds_in_px);
}

bool XWindow::InitializeAsStatusIcon() {
  std::string atom_name = "_NET_SYSTEM_TRAY_S" +
                          base::NumberToString(connection_->DefaultScreenId());
  auto reply = connection_->GetSelectionOwner({x11::GetAtom(atom_name)}).Sync();
  if (!reply || reply->owner == x11::Window::None)
    return false;
  auto manager = reply->owner;

  SetArrayProperty(
      xwindow_, x11::GetAtom("_XEMBED_INFO"), x11::Atom::CARDINAL,
      std::vector<uint32_t>{kXembedInfoProtocolVersion, kXembedInfoFlags});

  x11::ChangeWindowAttributesRequest req{xwindow_};
  if (has_alpha()) {
    req.background_pixel = 0;
  } else {
    SetProperty(xwindow_, x11::GetAtom("CHROMIUM_COMPOSITE_WINDOW"),
                x11::Atom::CARDINAL, static_cast<uint32_t>(1));
    req.background_pixmap =
        static_cast<x11::Pixmap>(x11::BackPixmap::ParentRelative);
  }
  connection_->ChangeWindowAttributes(req);

  auto future = SendClientMessage(
      manager, manager, x11::GetAtom("_NET_SYSTEM_TRAY_OPCODE"),
      {static_cast<uint32_t>(X11EventSource::GetInstance()->GetTimestamp()),
       kSystemTrayRequestDock, static_cast<uint32_t>(xwindow_), 0, 0},
      x11::EventMask::NoEvent);
  return !future.Sync().error;
}

}  // namespace ui
