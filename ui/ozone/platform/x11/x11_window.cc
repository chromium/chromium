// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/x11/x11_window.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "net/base/network_interfaces.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/buildflags.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/wm_role_names_linux.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/screen.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/visual_manager.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/x11_path.h"
#include "ui/gfx/x/xproto.h"
#include "ui/ozone/platform/x11/hit_test_x11.h"
#include "ui/ozone/platform/x11/x11_window_manager.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/platform_window/extensions/workspace_extension_delegate.h"
#include "ui/platform_window/extensions/x11_extension_delegate.h"
#include "ui/platform_window/wm/wm_drop_handler.h"

#if BUILDFLAG(USE_ATK)
#include "ui/ozone/platform/x11/atk_event_conversion.h"
#endif

namespace ui {
namespace {

using mojom::DragOperation;

// Opacity for drag widget windows.
constexpr float kDragWidgetOpacity = .75f;

// Coalesce touch/mouse events if needed
bool CoalesceEventsIfNeeded(const x11::Event& xev,
                            EventType type,
                            x11::Event* out) {
  if (xev.As<x11::MotionNotifyEvent>() ||
      (xev.As<x11::Input::DeviceEvent>() &&
       (type == ui::EventType::kTouchMoved ||
        type == ui::EventType::kMouseMoved ||
        type == ui::EventType::kMouseDragged))) {
    return ui::CoalescePendingMotionEvents(xev, out) > 0;
  }
  return false;
}

int GetKeyModifiers(const XDragDropClient* client) {
  if (!client) {
    return ui::XGetMaskAsEventFlags();
  }
  return client->current_modifier_state();
}

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

  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      data->push_back(bitmap.getColor(x, y));
    }
  }
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
      NOTREACHED_IN_MIGRATION();
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
  x11::Sync::Int64 sync_value{.hi = static_cast<int32_t>(value >> 32),
                              .lo = static_cast<uint32_t>(value)};
  connection->sync().SetCounter({counter, sync_value});
}

// Returns the whole path from |window| to the root.
std::vector<x11::Window> GetParentsList(x11::Connection* connection,
                                        x11::Window window) {
  std::vector<x11::Window> result;
  while (window != x11::Window::None) {
    result.push_back(window);
    if (auto reply = connection->QueryTree({window}).Sync()) {
      window = reply->parent;
    } else {
      break;
    }
  }
  return result;
}

std::vector<x11::Window>& GetSecuritySurfaces() {
  static base::NoDestructor<std::vector<x11::Window>> security_surfaces;
  return *security_surfaces;
}

}  // namespace

X11Window::X11Window(PlatformWindowDelegate* platform_window_delegate)
    : platform_window_delegate_(platform_window_delegate),
      connection_(*x11::Connection::Get()),
      x_root_window_(GetX11RootWindow()) {
  DCHECK_NE(x_root_window_, x11::Window::None);
  DCHECK(platform_window_delegate_);

  // Set a class property key, which allows |this| to be used for interactive
  // events, e.g. move or resize.
  SetWmMoveResizeHandler(this, static_cast<WmMoveResizeHandler*>(this));

  // Set extensions property key that extends the interface of this platform
  // implementation.
  SetWorkspaceExtension(this, static_cast<WorkspaceExtension*>(this));
  SetX11Extension(this, static_cast<X11Extension*>(this));
}

X11Window::~X11Window() {
  PrepareForShutdown();
  Close();
}

void X11Window::Initialize(PlatformWindowInitProperties properties) {
  CreateXWindow(properties);

  // It can be a status icon window.  If it fails to initialize, don't provide
  // it with a native window handle, close ourselves and let the client destroy
  // ourselves.
  if (properties.wm_role_name == kStatusIconWmRoleName &&
      !InitializeAsStatusIcon()) {
    CloseXWindow();
    return;
  }

  // At this point, the X window is created.  Register it and notify the
  // platform window delegate.
  X11WindowManager::GetInstance()->AddWindow(this);

  connection_->AddEventObserver(this);
  DCHECK(X11EventSource::HasInstance());
  X11EventSource::GetInstance()->AddPlatformEventDispatcher(this);

  x11_window_move_client_ =
      std::make_unique<ui::X11DesktopWindowMoveClient>(this);

  // Mark the window as eligible for the move loop, which allows tab dragging.
  SetWmMoveLoopHandler(this, static_cast<WmMoveLoopHandler*>(this));

  platform_window_delegate_->OnAcceleratedWidgetAvailable(GetWidget());

  // TODO(erg): Maybe need to set a ViewProp here like in RWHL::RWHL().

  auto event_mask =
      x11::EventMask::ButtonPress | x11::EventMask::ButtonRelease |
      x11::EventMask::FocusChange | x11::EventMask::KeyPress |
      x11::EventMask::KeyRelease | x11::EventMask::EnterWindow |
      x11::EventMask::LeaveWindow | x11::EventMask::Exposure |
      x11::EventMask::VisibilityChange | x11::EventMask::StructureNotify |
      x11::EventMask::PropertyChange | x11::EventMask::PointerMotion;
  xwindow_events_ = connection_->ScopedSelectEvent(xwindow_, event_mask);
  connection_->Flush();

  if (IsXInput2Available()) {
    TouchFactory::GetInstance()->SetupXI2ForXWindow(xwindow_);
  }

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
  connection_->SetArrayProperty(xwindow_, x11::GetAtom("WM_PROTOCOLS"),
                                x11::Atom::ATOM, protocols);

  // We need a WM_CLIENT_MACHINE value so we integrate with the desktop
  // environment.
  connection_->SetStringProperty(xwindow_, x11::Atom::WM_CLIENT_MACHINE,
                                 x11::Atom::STRING, net::GetHostName());

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(uint32_t) >= sizeof(pid_t),
                "pid_t should not be larger than uint32_t");
  uint32_t pid = getpid();
  connection_->SetProperty(xwindow_, x11::GetAtom("_NET_WM_PID"),
                           x11::Atom::CARDINAL, pid);

  x11::Atom window_type;
  switch (properties.type) {
    case PlatformWindowType::kMenu:
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_MENU");
      break;
    case PlatformWindowType::kTooltip:
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_TOOLTIP");
      break;
    case PlatformWindowType::kBubble:
    case PlatformWindowType::kPopup:
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_NOTIFICATION");
      break;
    case PlatformWindowType::kDrag:
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_DND");
      break;
    default:
      window_type = x11::GetAtom("_NET_WM_WINDOW_TYPE_NORMAL");
      break;
  }
  connection_->SetProperty(xwindow_, x11::GetAtom("_NET_WM_WINDOW_TYPE"),
                           x11::Atom::ATOM, window_type);

  // The changes to |window_properties_| here will be sent to the X server just
  // before the window is mapped.

  // Remove popup windows from taskbar unless overridden.
  if ((properties.type == PlatformWindowType::kPopup ||
       properties.type == PlatformWindowType::kBubble) &&
      !properties.force_show_in_taskbar) {
    window_properties_.insert(x11::GetAtom("_NET_WM_STATE_SKIP_TASKBAR"));
  }

  // If the window should stay on top of other windows, add the
  // _NET_WM_STATE_ABOVE property.
  is_always_on_top_ = properties.keep_on_top;
  if (is_always_on_top_) {
    window_properties_.insert(x11::GetAtom("_NET_WM_STATE_ABOVE"));
  }

  is_security_surface_ = properties.is_security_surface;
  if (is_security_surface_) {
    GetSecuritySurfaces().push_back(xwindow_);
  } else {
    // Newly created windows appear at the top of the stacking order, so raise
    // any security surfaces since the WM will not do it if the window is
    // override-redirect.
    for (x11::Window window : GetSecuritySurfaces()) {
      connection_->RaiseWindow(window);
    }
  }

  workspace_ = std::nullopt;
  if (properties.visible_on_all_workspaces) {
    window_properties_.insert(x11::GetAtom("_NET_WM_STATE_STICKY"));
    connection_->SetProperty(xwindow_, x11::GetAtom("_NET_WM_DESKTOP"),
                             x11::Atom::CARDINAL, kAllWorkspaces);
  } else if (!properties.workspace.empty()) {
    int32_t workspace;
    if (base::StringToInt(properties.workspace, &workspace)) {
      connection_->SetProperty(xwindow_, x11::GetAtom("_NET_WM_DESKTOP"),
                               x11::Atom::CARDINAL, workspace);
    }
  }

  if (!properties.wm_class_name.empty() || !properties.wm_class_class.empty()) {
    SetWindowClassHint(&connection_.get(), xwindow_, properties.wm_class_name,
                       properties.wm_class_class);
  }

  const char* wm_role_name = nullptr;
  // If the widget isn't overriding the role, provide a default value for popup
  // and bubble types.
  if (!properties.wm_role_name.empty()) {
    wm_role_name = properties.wm_role_name.c_str();
  } else {
    switch (properties.type) {
      case PlatformWindowType::kPopup:
        wm_role_name = kX11WindowRolePopup;
        break;
      case PlatformWindowType::kBubble:
        wm_role_name = kX11WindowRoleBubble;
        break;
      default:
        break;
    }
  }
  if (wm_role_name) {
    SetWindowRole(xwindow_, std::string(wm_role_name));
  }

  SetTitle(u"");

  if (properties.remove_standard_frame) {
    // Setting _GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED tells gnome-shell to not force
    // fullscreen on the window when it matches the desktop size.
    SetHideTitlebarWhenMaximizedProperty(xwindow_,
                                         HIDE_TITLEBAR_WHEN_MAXIMIZED);
  }

  if (properties.prefer_dark_theme) {
    connection_->SetStringProperty(xwindow_, x11::GetAtom("_GTK_THEME_VARIANT"),
                                   x11::GetAtom("UTF8_STRING"),
                                   kDarkGtkThemeVariant);
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
    connection_->SetArrayProperty(xwindow_,
                                  x11::GetAtom("_NET_WM_SYNC_REQUEST_COUNTER"),
                                  x11::Atom::CARDINAL, counters);
  }

  // Always composite Chromium windows if a compositing WM is used.  Sometimes,
  // WMs will not composite fullscreen windows as an optimization, but this can
  // lead to tearing of fullscreen videos.
  connection_->SetProperty<uint32_t>(xwindow_,
                                     x11::GetAtom("_NET_WM_BYPASS_COMPOSITOR"),
                                     x11::Atom::CARDINAL, 2);

  if (properties.icon) {
    SetWindowIcons(gfx::ImageSkia(), *properties.icon);
  }

  if (properties.type == PlatformWindowType::kDrag) {
    SetOpacity(kDragWidgetOpacity);
  }

  SetWmDragHandler(this, this);

  drag_drop_client_ = std::make_unique<XDragDropClient>(this, window());
}

void X11Window::OnXWindowLostCapture() {
  platform_window_delegate_->OnLostCapture();
}

void X11Window::OnMouseEnter() {
  platform_window_delegate_->OnMouseEnter();
}

gfx::AcceleratedWidget X11Window::GetWidget() const {
  // In spite of being defined in Xlib as `unsigned long`, XID (|window()|'s
  // type) is fixed at 32-bits (CARD32) in X11 Protocol, therefore can't be
  // larger than 32 bits values on the wire (see https://crbug.com/607014 for
  // more details). So, It's safe to use static_cast here.
  return static_cast<gfx::AcceleratedWidget>(window());
}

void X11Window::Show(bool inactive) {
  if (window_mapped_in_client_) {
    return;
  }

  Map(inactive);
}

void X11Window::Hide() {
  if (!window_mapped_in_client_) {
    return;
  }

  // Make sure no resize task will run after the window is unmapped.
  CancelResize();

  connection_->WithdrawWindow(xwindow_);
  window_mapped_in_client_ = false;
}

void X11Window::Close() {
  if (is_shutting_down_) {
    return;
  }

  X11WindowManager::GetInstance()->RemoveWindow(this);

  is_shutting_down_ = true;

  CloseXWindow();

  platform_window_delegate_->OnClosed();
}

bool X11Window::IsVisible() const {
  // On Windows, IsVisible() returns true for minimized windows.  On X11, a
  // minimized window is not mapped, so an explicit IsMinimized() check is
  // necessary.
  return window_mapped_in_client_ || IsMinimized();
}

void X11Window::PrepareForShutdown() {
  if (HasCapture()) {
    X11WindowManager::GetInstance()->UngrabEvents(this);
  }
  connection_->RemoveEventObserver(this);
  DCHECK(X11EventSource::HasInstance());
  X11EventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void X11Window::SetBoundsInPixels(const gfx::Rect& bounds) {
  gfx::Rect new_bounds_in_pixels(bounds.origin(),
                                 AdjustSizeForDisplay(bounds.size()));

  const bool size_changed =
      bounds_in_pixels_.size() != new_bounds_in_pixels.size();
  const bool origin_changed =
      bounds_in_pixels_.origin() != new_bounds_in_pixels.origin();

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_| later.

  x11::ConfigureWindowRequest req{.window = xwindow_};

  if (size_changed) {
    // Only cancel the delayed resize task if we're already about to call
    // OnHostResized in this function.
    CancelResize();

    // Update the minimum and maximum sizes in case they have changed.
    UpdateMinAndMaxSize();

    if (new_bounds_in_pixels.width() < min_size_in_pixels_.width() ||
        new_bounds_in_pixels.height() < min_size_in_pixels_.height() ||
        (!max_size_in_pixels_.IsEmpty() &&
         (new_bounds_in_pixels.width() > max_size_in_pixels_.width() ||
          new_bounds_in_pixels.height() > max_size_in_pixels_.height()))) {
      gfx::Size size_in_pixels = new_bounds_in_pixels.size();
      if (!max_size_in_pixels_.IsEmpty()) {
        size_in_pixels.SetToMin(max_size_in_pixels_);
      }
      size_in_pixels.SetToMax(min_size_in_pixels_);
      new_bounds_in_pixels.set_size(size_in_pixels);
    }

    req.width = new_bounds_in_pixels.width();
    req.height = new_bounds_in_pixels.height();
  }

  if (origin_changed) {
    req.x = new_bounds_in_pixels.x();
    req.y = new_bounds_in_pixels.y();
  }

  if (origin_changed || size_changed) {
    bounds_change_in_flight_ = true;
    connection_->ConfigureWindow(req);
  }

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_in_pixels_| later.
  bounds_in_pixels_ = new_bounds_in_pixels;
  ResetWindowRegion();

  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  platform_window_delegate_->OnBoundsChanged({origin_changed});
}

gfx::Rect X11Window::GetBoundsInPixels() const {
  return bounds_in_pixels_;
}

void X11Window::SetBoundsInDIP(const gfx::Rect& bounds_in_dip) {
  SetBoundsInPixels(
      platform_window_delegate_->ConvertRectToPixels(bounds_in_dip));
}

gfx::Rect X11Window::GetBoundsInDIP() const {
  return platform_window_delegate_->ConvertRectToDIP(bounds_in_pixels_);
}

void X11Window::SetTitle(const std::u16string& title) {
  if (window_title_ == title) {
    return;
  }

  window_title_ = title;
  std::string utf8str = base::UTF16ToUTF8(title);
  connection_->SetStringProperty(xwindow_, x11::GetAtom("_NET_WM_NAME"),
                                 x11::GetAtom("UTF8_STRING"), utf8str);
  connection_->SetStringProperty(xwindow_, x11::Atom::WM_NAME,
                                 x11::GetAtom("UTF8_STRING"), utf8str);
}

void X11Window::SetCapture() {
  if (HasCapture()) {
    return;
  }
  X11WindowManager::GetInstance()->GrabEvents(this);

  // If the pointer is already in |xwindow_|, we will not get a crossing event
  // with a mode of NotifyGrab, so we must record the grab state manually.
  has_pointer_grab_ |=
      (ui::GrabPointer(xwindow_, true, nullptr) == x11::GrabStatus::Success);
}

void X11Window::ReleaseCapture() {
  if (!HasCapture()) {
    return;
  }

  UngrabPointer();
  has_pointer_grab_ = false;

  X11WindowManager::GetInstance()->UngrabEvents(this);
}

bool X11Window::HasCapture() const {
  return X11WindowManager::GetInstance()->located_events_grabber() == this;
}

void X11Window::SetFullscreen(bool fullscreen, int64_t target_display_id) {
  // TODO(crbug.com/40111909) Support `target_display_id` on this platform.
  DCHECK_EQ(target_display_id, display::kInvalidDisplayId);
  if (fullscreen) {
    CancelResize();
  }

  // Work around a bug where if we try to unfullscreen, metacity immediately
  // fullscreens us again. This is a little flickery and not necessary if
  // there's a gnome-panel, but it's not easy to detect whether there's a
  // panel or not.
  bool unmaximize_and_remaximize = !fullscreen && IsMaximized() &&
                                   ui::GuessWindowManager() == ui::WM_METACITY;

  if (unmaximize_and_remaximize) {
    Restore();
  }

  // Fullscreen state changes have to be handled manually and then checked
  // against configuration events, which come from a compositor. The reason
  // of manually changing the |state_| is that the compositor answers
  // about state changes asynchronously, which leads to a wrong return value in
  // DesktopWindowTreeHostPlatform::IsFullscreen, for example, and media
  // files can never be set to fullscreen. Wayland does the same.
  auto new_state = PlatformWindowState::kNormal;
  if (fullscreen) {
    new_state = PlatformWindowState::kFullScreen;
  } else if (IsMaximized()) {
    new_state = PlatformWindowState::kMaximized;
  }

  bool was_fullscreen = IsFullscreen();
  state_ = new_state;
  SetFullscreen(fullscreen);

  if (unmaximize_and_remaximize) {
    Maximize();
  }

  // Try to guess the size we will have after the switch to/from fullscreen:
  // - (may) avoid transient states
  // - works around Flash content which expects to have the size updated
  //   synchronously.
  // See https://crbug.com/361408
  gfx::Rect new_bounds_px = GetBoundsInPixels();
  if (fullscreen) {
    restored_bounds_in_pixels_ = new_bounds_px;
    if (x11_extension_delegate_) {
      new_bounds_px = x11_extension_delegate_->GetGuessedFullScreenSizeInPx();
    }
  } else {
    // Exiting "browser fullscreen mode", but the X11 window is not necessarily
    // in fullscreen state (e.g: a WM keybinding might have been used to toggle
    // fullscreen state). So check whether the window is in fullscreen state
    // before trying to restore its bounds (saved before entering in browser
    // fullscreen mode).
    if (was_fullscreen) {
      new_bounds_px = restored_bounds_in_pixels_;
    } else {
      restored_bounds_in_pixels_ = gfx::Rect();
    }
  }

  UpdateDecorationInsets();

  // Do not go through SetBounds as long as it adjusts bounds and sets them to X
  // Server. Instead, we just store the bounds and notify the client that the
  // window occupies the entire screen.
  bool origin_changed = bounds_in_pixels_.origin() != new_bounds_px.origin();
  bounds_in_pixels_ = new_bounds_px;

  // If there is a restore and/or bounds change in flight, then set a flag to
  // ignore the next one or two configure events (hopefully) coming from those
  // requests. This prevents any in-flight restore requests from changing the
  // bounds in a way that conflicts with the `bounds_in_pixels_` setting above.
  // This is not perfect, and if there is some other in-flight bounds change for
  // some reason, or if the ordering of events from the WM behaves differently,
  // this will not prevent the issue.  See: http://crbug.com/1227451
  ignore_next_configures_ = restore_in_flight_ ? 1 : 0;
  if (bounds_change_in_flight_) {
    ignore_next_configures_++;
  }
  // This must be the final call in this function, as `this` may be deleted
  // during the observation of this event.
  platform_window_delegate_->OnBoundsChanged({origin_changed});
}

void X11Window::Maximize() {
  if (IsFullscreen()) {
    // Unfullscreen the window if it is fullscreen.
    SetFullscreen(false);

    // Resize the window so that it does not have the same size as a monitor.
    // (Otherwise, some window managers immediately put the window back in
    // fullscreen mode).
    gfx::Rect bounds_in_pixels = GetBoundsInPixels();
    gfx::Rect adjusted_bounds_in_pixels(
        bounds_in_pixels.origin(),
        AdjustSizeForDisplay(bounds_in_pixels.size()));
    if (adjusted_bounds_in_pixels != bounds_in_pixels) {
      SetBoundsInPixels(adjusted_bounds_in_pixels);
    }
  }

  // When we are in the process of requesting to maximize a window, we can
  // accurately keep track of our restored bounds instead of relying on the
  // heuristics that are in the PropertyNotify and ConfigureNotify handlers.
  restored_bounds_in_pixels_ = GetBoundsInPixels();

  // Some WMs do not respect maximization hints on unmapped windows, so we
  // save this one for later too.
  should_maximize_after_map_ = !window_mapped_in_client_;

  SetWMSpecState(true, x11::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 x11::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

void X11Window::Minimize() {
  if (window_mapped_in_client_) {
    SendClientMessage(xwindow_, x_root_window_, x11::GetAtom("WM_CHANGE_STATE"),
                      {x11::WM_STATE_ICONIC, 0, 0, 0, 0});
  } else {
    SetWMSpecState(true, x11::GetAtom("_NET_WM_STATE_HIDDEN"), x11::Atom::None);
  }
}

void X11Window::Restore() {
  if (IsMinimized()) {
    restore_in_flight_ = true;
    SetWMSpecState(false, x11::GetAtom("_NET_WM_STATE_HIDDEN"),
                   x11::Atom::None);
  } else if (IsMaximized()) {
    restore_in_flight_ = true;
    should_maximize_after_map_ = false;
    SetWMSpecState(false, x11::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                   x11::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
  }
}

PlatformWindowState X11Window::GetPlatformWindowState() const {
  return state_;
}

void X11Window::Activate() {
  if (!IsVisible() || !activatable_) {
    return;
  }

  BeforeActivationStateChanged();

  ignore_keyboard_input_ = false;

  // wmii says that it supports _NET_ACTIVE_WINDOW but does not.
  // https://code.google.com/p/wmii/issues/detail?id=266
  static bool wm_supports_active_window =
      GuessWindowManager() != WM_WMII &&
      connection_->WmSupportsHint(x11::GetAtom("_NET_ACTIVE_WINDOW"));

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
    connection_->RaiseWindow(xwindow_);
    // Directly ask the X server to give focus to the window. Note that the call
    // would have raised an X error if the window is not mapped.
    connection_->SetInputFocus({x11::InputFocus::Parent, xwindow_, timestamp})
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

void X11Window::Deactivate() {
  BeforeActivationStateChanged();

  // Ignore future input events.
  ignore_keyboard_input_ = true;

  connection_->LowerWindow(xwindow_);

  AfterActivationStateChanged();
}

void X11Window::SetUseNativeFrame(bool use_native_frame) {
  use_native_frame_ = use_native_frame;
  SetUseOSWindowFrame(xwindow_, use_native_frame);
  ResetWindowRegion();
}

bool X11Window::ShouldUseNativeFrame() const {
  return use_native_frame_;
}

void X11Window::SetCursor(scoped_refptr<PlatformCursor> cursor) {
  DCHECK(cursor);

  // When a DnD loop is running, DesktopDragDropClientOzone may change the
  // cursor type based on the current dnd operation. Setting cursor type with
  // the current window that is involved in the DnD is no-op as
  // X11WholeScreenMoveLoop grabs the pointer and is responsible for changing
  // current pointer's bitmap. Thus, pass the changed cursor to the drag loop so
  // that it handles the change.
  if (drag_loop_) {
    drag_loop_->UpdateCursor(X11Cursor::FromPlatformCursor(cursor));
    return;
  }

  last_cursor_ = X11Cursor::FromPlatformCursor(cursor);
  on_cursor_loaded_.Reset(base::BindOnce(
      &x11::Connection::DefineCursor, base::Unretained(connection_), xwindow_));
  last_cursor_->OnCursorLoaded(on_cursor_loaded_.callback());
}

void X11Window::MoveCursorTo(const gfx::Point& location_px) {
  connection_->WarpPointer(x11::WarpPointerRequest{
      .dst_window = x_root_window_,
      .dst_x = static_cast<int16_t>(bounds_in_pixels_.x() + location_px.x()),
      .dst_y = static_cast<int16_t>(bounds_in_pixels_.y() + location_px.y()),
  });
  // The cached cursor location is no longer valid.
  X11EventSource::GetInstance()->ClearLastCursorLocation();
}

void X11Window::ConfineCursorToBounds(const gfx::Rect& bounds) {
  UnconfineCursor();

  if (bounds.IsEmpty()) {
    return;
  }

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

void X11Window::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {
  restored_bounds_in_pixels_ =
      platform_window_delegate_->ConvertRectToPixels(bounds);
}

gfx::Rect X11Window::GetRestoredBoundsInDIP() const {
  return platform_window_delegate_->ConvertRectToDIP(
      restored_bounds_in_pixels_);
}

bool X11Window::ShouldWindowContentsBeTransparent() const {
  return visual_has_alpha_;
}

void X11Window::SetZOrderLevel(ZOrderLevel order) {
  z_order_ = order;

  // Emulate the multiple window levels provided by other platforms by
  // collapsing the z-order enum into kNormal = normal, everything else = always
  // on top.
  is_always_on_top_ = (z_order_ != ui::ZOrderLevel::kNormal);
  SetWMSpecState(is_always_on_top_, x11::GetAtom("_NET_WM_STATE_ABOVE"),
                 x11::Atom::None);
}

ZOrderLevel X11Window::GetZOrderLevel() const {
  bool level_always_on_top = z_order_ != ui::ZOrderLevel::kNormal;

  if (is_always_on_top_ == level_always_on_top) {
    return z_order_;
  }

  // If something external has forced a window to be always-on-top, map it to
  // kFloatingWindow as a reasonable equivalent.
  return is_always_on_top_ ? ui::ZOrderLevel::kFloatingWindow
                           : ui::ZOrderLevel::kNormal;
}

void X11Window::StackAbove(gfx::AcceleratedWidget widget) {
  // Check comment in the GetWidget method about this cast.
  auto window = static_cast<x11::Window>(widget);
  DCHECK(window != x11::Window::None);

  // Find all parent windows up to the root.
  std::vector<x11::Window> window_below_parents =
      GetParentsList(&connection_.get(), window);
  std::vector<x11::Window> window_above_parents =
      GetParentsList(&connection_.get(), xwindow_);

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

void X11Window::StackAtTop() {
  connection_->RaiseWindow(xwindow_);
}

void X11Window::FlashFrame(bool flash_frame) {
  SetFlashFrameHint(flash_frame);
}

void X11Window::SetShape(std::unique_ptr<ShapeRects> native_shape,
                         const gfx::Transform& transform) {
  std::unique_ptr<std::vector<x11::Rectangle>> xregion;
  if (native_shape) {
    SkRegion native_region;
    for (const gfx::Rect& rect : *native_shape) {
      native_region.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
    }
    if (!transform.IsIdentity() && !native_region.isEmpty()) {
      SkPath path_in_dip;
      if (native_region.getBoundaryPath(&path_in_dip)) {
        SkPath path_in_pixels;
        path_in_dip.transform(gfx::TransformToFlattenedSkMatrix(transform),
                              &path_in_pixels);
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

void X11Window::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  x11::SizeHints size_hints;
  memset(&size_hints, 0, sizeof(size_hints));

  connection_->GetWmNormalHints(xwindow_, &size_hints);
  // Unforce aspect ratio is parameter length is 0, otherwise set normally.
  if (aspect_ratio.IsEmpty()) {
    size_hints.flags &= ~x11::SIZE_HINT_P_ASPECT;
  } else {
    size_hints.flags |= x11::SIZE_HINT_P_ASPECT;
    size_hints.min_aspect_num = size_hints.max_aspect_num =
        aspect_ratio.width();
    size_hints.min_aspect_den = size_hints.max_aspect_den =
        aspect_ratio.height();
  }
  connection_->SetWmNormalHints(xwindow_, size_hints);
}

void X11Window::SetWindowIcons(const gfx::ImageSkia& window_icon,
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

  if (!window_icon.isNull()) {
    SerializeImageRepresentation(window_icon.GetRepresentation(1.0f), &data);
  }

  if (!app_icon.isNull()) {
    SerializeImageRepresentation(app_icon.GetRepresentation(1.0f), &data);
  }

  if (!data.empty()) {
    connection_->SetArrayProperty(xwindow_, x11::GetAtom("_NET_WM_ICON"),
                                  x11::Atom::CARDINAL, data);
  }
}

void X11Window::SizeConstraintsChanged() {
  X11Window::UpdateMinAndMaxSize();
}

void X11Window::SetOpacity(float opacity) {
  // X server opacity is in terms of 32 bit unsigned int space, and counts from
  // the opposite direction.
  // XChangeProperty() expects "cardinality" to be long.

  // Scale opacity to [0 .. 255] range.
  uint32_t opacity_8bit = static_cast<uint32_t>(opacity * 255.0f) & 0xFF;
  // Use opacity value for all channels.
  uint32_t channel_multiplier = 0x1010101;
  uint32_t cardinality = opacity_8bit * channel_multiplier;

  if (cardinality == 0xffffffff) {
    connection_->DeleteProperty(xwindow_,
                                x11::GetAtom("_NET_WM_WINDOW_OPACITY"));
  } else {
    connection_->SetProperty(xwindow_, x11::GetAtom("_NET_WM_WINDOW_OPACITY"),
                             x11::Atom::CARDINAL, cardinality);
  }
}

bool X11Window::CanSetDecorationInsets() const {
  return connection_->WmSupportsHint(x11::GetAtom("_GTK_FRAME_EXTENTS"));
}

void X11Window::SetOpaqueRegion(
    std::optional<std::vector<gfx::Rect>> region_px) {
  auto atom = x11::GetAtom("_NET_WM_OPAQUE_REGION");
  if (!region_px) {
    connection_->DeleteProperty(xwindow_, atom);
    return;
  }
  std::vector<uint32_t> value;
  for (const auto& rect : *region_px) {
    value.push_back(rect.x());
    value.push_back(rect.y());
    value.push_back(rect.width());
    value.push_back(rect.height());
  }
  connection_->SetArrayProperty(xwindow_, atom, x11::Atom::CARDINAL, value);
}

void X11Window::SetInputRegion(
    std::optional<std::vector<gfx::Rect>> region_px) {
  if (!region_px.has_value() || region_px->empty()) {
    // Reset the input region.
    connection_->shape().Mask({
        .operation = x11::Shape::So::Set,
        .destination_kind = x11::Shape::Sk::Input,
        .destination_window = xwindow_,
    });
    return;
  }
  DCHECK_EQ(1u, region_px->size());
  connection_->shape().Rectangles(x11::Shape::RectanglesRequest{
      .operation = x11::Shape::So::Set,
      .destination_kind = x11::Shape::Sk::Input,
      .ordering = x11::ClipOrdering::YXBanded,
      .destination_window = xwindow_,
      .rectangles = {{static_cast<int16_t>((*region_px)[0].x()),
                      static_cast<int16_t>((*region_px)[0].y()),
                      static_cast<uint16_t>((*region_px)[0].width()),
                      static_cast<uint16_t>((*region_px)[0].height())}},
  });
}

void X11Window::NotifyStartupComplete(const std::string& startup_id) {
  std::string message = "remove: ID=\"";
  for (char c : startup_id) {
    if (c == ' ' || c == '"' || c == '\\') {
      message.push_back('\\');
    }
    message.push_back(c);
  }
  message.push_back('"');

  auto window = connection_->CreateDummyWindow();
  x11::ClientMessageEvent event{
      .format = 8,
      .window = window,
      .type = x11::GetAtom("_NET_STARTUP_INFO_BEGIN"),
  };
  constexpr size_t kChunkSize = event.data.data8.size();
  const x11::Atom net_startup_info = x11::GetAtom("_NET_STARTUP_INFO");

  // X11 ClientMessageEvents are fixed size, but we need to send a variable
  // sized message.  Send the message `kChunkSize` bytes at a time with the
  // first message having type _NET_STARTUP_INFO_BEGIN and subsequent messages
  // having type _NET_STARTUP_INFO.
  const char* data = message.c_str();
  const size_t data_size = message.size() + 1;
  for (size_t offset = 0; offset < data_size; offset += kChunkSize) {
    size_t copy_size = std::min<size_t>(kChunkSize, data_size - offset);
    uint8_t* dst = &event.data.data8[0];
    memcpy(dst, data + offset, copy_size);
    memset(dst + copy_size, 0, kChunkSize - copy_size);
    connection_->SendEvent(event, x_root_window_,
                           x11::EventMask::PropertyChange);
    event.type = net_startup_info;
  }

  connection_->DestroyWindow(window);
  connection_->Flush();
}

std::string X11Window::GetWorkspace() const {
  std::optional<int> workspace_id = workspace_;
  return workspace_id.has_value() ? base::NumberToString(workspace_id.value())
                                  : std::string();
}

void X11Window::SetVisibleOnAllWorkspaces(bool always_visible) {
  SetWMSpecState(always_visible, x11::GetAtom("_NET_WM_STATE_STICKY"),
                 x11::Atom::None);

  int new_desktop = 0;
  if (always_visible) {
    new_desktop = kAllWorkspaces;
  } else {
    if (!GetCurrentDesktop(&new_desktop)) {
      return;
    }
  }

  workspace_ = kAllWorkspaces;
  SendClientMessage(xwindow_, x_root_window_, x11::GetAtom("_NET_WM_DESKTOP"),
                    {static_cast<uint32_t>(new_desktop), 0, 0, 0, 0});
}

bool X11Window::IsVisibleOnAllWorkspaces() const {
  // We don't need a check for _NET_WM_STATE_STICKY because that would specify
  // that the window remain in a fixed position even if the viewport scrolls.
  // This is different from the type of workspace that's associated with
  // _NET_WM_DESKTOP.
  return workspace_ == kAllWorkspaces;
}

void X11Window::SetWorkspaceExtensionDelegate(
    WorkspaceExtensionDelegate* delegate) {
  workspace_extension_delegate_ = delegate;
}

bool X11Window::IsSyncExtensionAvailable() const {
  return connection_->sync_version() > std::pair<uint32_t, uint32_t>{0, 0};
}

bool X11Window::IsWmTiling() const {
  return ui::IsWmTiling(ui::GuessWindowManager());
}

void X11Window::OnCompleteSwapAfterResize() {
  if (configure_counter_value_is_extended_) {
    if ((current_counter_value_ % 2) == 1) {
      // An increase 3 means that the frame was not drawn as fast as possible.
      // This can trigger different handling from the compositor.
      // Setting an even number to |extended_update_counter_| will trigger a
      // new resize.
      current_counter_value_ += 3;
      SyncSetCounter(&connection_.get(), extended_update_counter_,
                     current_counter_value_);
    }
    return;
  }

  if (configure_counter_value_ != 0) {
    SyncSetCounter(&connection_.get(), update_counter_,
                   configure_counter_value_);
    configure_counter_value_ = 0;
  }
}

gfx::Rect X11Window::GetXRootWindowOuterBounds() const {
  return GetOuterBounds();
}

void X11Window::LowerXWindow() {
  connection_->LowerWindow(xwindow_);
}

void X11Window::SetOverrideRedirect(bool override_redirect) {
  bool remap = window_mapped_in_client_;
  if (remap) {
    Hide();
  }
  connection_->ChangeWindowAttributes(x11::ChangeWindowAttributesRequest{
      .window = xwindow_,
      .override_redirect = x11::Bool32(override_redirect),
  });
  if (remap) {
    Map();
    // We cannot regrab the pointer now since unmapping/mapping
    // happens asynchronously.  We must wait until the window is
    // mapped to issue a grab request.
    if (has_pointer_grab_) {
      should_grab_pointer_after_map_ = true;
    }
  }
}

bool X11Window::CanResetOverrideRedirect() const {
  // Ratpoision sometimes hangs when setting the override-redirect state to a
  // new value (https://crbug.com/1216221).
  return ui::GuessWindowManager() != ui::WindowManagerName::WM_RATPOISON;
}

void X11Window::SetX11ExtensionDelegate(X11ExtensionDelegate* delegate) {
  x11_extension_delegate_ = delegate;
}

bool X11Window::HandleAsAtkEvent(const x11::KeyEvent& key_event,
                                 bool send_event,
                                 bool transient) {
#if !BUILDFLAG(USE_ATK)
  // TODO(crbug.com/40653448): Support ATK in Ozone/X11.
  NOTREACHED_IN_MIGRATION();
  return false;
#else
  if (!x11_extension_delegate_) {
    return false;
  }
  auto atk_key_event = AtkKeyEventFromXEvent(key_event, send_event);
  return x11_extension_delegate_->OnAtkKeyEvent(atk_key_event.get(), transient);
#endif
}

void X11Window::OnEvent(const x11::Event& xev) {
  auto event_type = ui::EventTypeFromXEvent(xev);
  if (event_type != EventType::kUnknown) {
    // If this event can be translated, it will be handled in ::DispatchEvent.
    // Otherwise, we end up processing XEvents twice that could lead to unwanted
    // behaviour like loosing activation during tab drag and etc.
    return;
  }

  auto* prop = xev.As<x11::PropertyNotifyEvent>();
  auto* target_current_context = drag_drop_client_->target_current_context();
  if (prop && target_current_context &&
      prop->window == target_current_context->source_window()) {
    target_current_context->DispatchPropertyNotifyEvent(*prop);
  }

  HandleEvent(xev);
}

bool X11Window::CanDispatchEvent(const PlatformEvent& xev) {
  if (is_shutting_down_) {
    return false;
  }
  DCHECK_NE(window(), x11::Window::None);
  auto* dispatching_event = connection_->dispatching_event();
  return dispatching_event && IsTargetedBy(*dispatching_event);
}

uint32_t X11Window::DispatchEvent(const PlatformEvent& event) {
  TRACE_EVENT1("views", "X11PlatformWindow::Dispatch", "event->type()",
               event->type());

  DCHECK_NE(window(), x11::Window::None);
  DCHECK(event);

  auto& current_xevent = *connection_->dispatching_event();

  if (event->IsMouseEvent()) {
    X11WindowManager::GetInstance()->MouseOnWindow(this);
  }
#if BUILDFLAG(USE_ATK)
  if (auto* key = current_xevent.As<x11::KeyEvent>()) {
    if (HandleAsAtkEvent(*key, current_xevent.send_event(),
                         key->event == transient_window_)) {
      return POST_DISPATCH_STOP_PROPAGATION;
    }
  }
#endif

  DispatchUiEvent(event, current_xevent);
  return POST_DISPATCH_STOP_PROPAGATION;
}

void X11Window::DispatchUiEvent(ui::Event* event, const x11::Event& xev) {
  auto* window_manager = X11WindowManager::GetInstance();
  DCHECK(window_manager);

  // Process X11-specific bits
  HandleEvent(xev);

  x11::Event last_xev;
  std::unique_ptr<ui::Event> last_motion;
  if (CoalesceEventsIfNeeded(xev, event->type(), &last_xev)) {
    last_motion = ui::BuildEventFromXEvent(last_xev);
    event = last_motion.get();
  }
  if (!event) {
    return;
  }

  // If |event| is a located event (mouse, touch, etc) and another X11 window
  // is set as the current located events grabber, the |event| must be
  // re-routed to that grabber. Otherwise, just send the event.
  // Note: We want to coalesce events before doing this, since this modifies our
  // ui::Event's coordinates, and coalescing would simply undo the coordinate
  // change.
  auto* located_events_grabber = window_manager->located_events_grabber();
  if (event->IsLocatedEvent() && located_events_grabber &&
      located_events_grabber != this) {
    if (event->IsMouseEvent() ||
        (event->IsTouchEvent() &&
         event->type() == ui::EventType::kTouchPressed)) {
      // Another X11Window has installed itself as capture. Translate the
      // event's location and dispatch to the other.
      ConvertEventLocationToTargetWindowLocation(
          located_events_grabber->GetBoundsInPixels().origin(),
          GetBoundsInPixels().origin(), event->AsLocatedEvent());
    }
    return located_events_grabber->DispatchUiEvent(event, xev);
  }

  // If after CoalescePendingMotionEvents the type of xev is resolved to
  // UNKNOWN, i.e: xevent translation returns nullptr, don't dispatch the
  // event. TODO(crbug.com/40559202): investigate why ColescePendingMotionEvents
  // can include mouse wheel events as well. Investigation showed that events on
  // Linux are checked with cmt-device path, and can include DT_CMT_SCROLL_
  // data. See more discussion in https://crrev.com/c/853953
  UpdateWMUserTime(event);
  DispatchEventFromNativeUiEvent(
      event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                            base::Unretained(platform_window_delegate())));
}

void X11Window::UpdateDecorationInsets() {
  auto atom = x11::GetAtom("_GTK_FRAME_EXTENTS");
  auto insets_dip =
      platform_window_delegate_->CalculateInsetsInDIP(GetPlatformWindowState());

  if (insets_dip.IsEmpty()) {
    connection_->DeleteProperty(xwindow_, atom);
    return;
  }

  // Insets must be zero when the window state is not normal nor unknown.
  CHECK(GetPlatformWindowState() == PlatformWindowState::kNormal ||
        GetPlatformWindowState() == PlatformWindowState::kUnknown);

  auto insets_px = platform_window_delegate_->ConvertInsetsToPixels(insets_dip);
  connection_->SetArrayProperty(
      xwindow_, atom, x11::Atom::CARDINAL,
      std::vector<uint32_t>{static_cast<uint32_t>(insets_px.left()),
                            static_cast<uint32_t>(insets_px.right()),
                            static_cast<uint32_t>(insets_px.top()),
                            static_cast<uint32_t>(insets_px.bottom())});
}

void X11Window::OnXWindowStateChanged() {
  // Determine the new window state information to be propagated to the client.
  // Note that the order of checks is important here, because window can have
  // several properties at the same time.
  auto new_state = PlatformWindowState::kNormal;
  if (IsMinimized()) {
    new_state = PlatformWindowState::kMinimized;
  } else if (IsFullscreen()) {
    new_state = PlatformWindowState::kFullScreen;
  } else if (IsMaximized()) {
    new_state = PlatformWindowState::kMaximized;
  }

  if (restore_in_flight_ && !IsMaximized()) {
    restore_in_flight_ = false;
  }

  // fullscreen state is set syschronously at ToggleFullscreen() and must be
  // kept and propagated to the client only when explicitly requested by upper
  // layers, as it means we are in "browser fullscreen mode" (where
  // decorations, omnibar, buttons, etc are hidden), which is different from
  // the case where the request comes from the window manager (or any other
  // process), handled by this method. In this case, we follow EWMH guidelines:
  // Optimize the whole application for fullscreen usage. Window decorations
  // (e.g. borders) should be hidden, but the functionalily of the application
  // should not change. Further details:
  // https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html
  bool browser_fullconnection_mode = state_ == PlatformWindowState::kFullScreen;
  bool window_fullconnection_mode =
      new_state == PlatformWindowState::kFullScreen;
  // So, we ignore fullscreen state transitions in 2 cases:
  // 1. If |new_state| is kFullScreen but |state_| is not, which means the
  // fullscreen request is coming from an external process. So the browser
  // window must occupies the entire screen but not transitioning to browser
  // fullscreen mode.
  // 2. if |state_| is kFullScreen but |new_state| is not, we have been
  // requested to exit fullscreen by other process (e.g: via WM keybinding),
  // in this case we must keep on "browser fullscreen mode" bug the platform
  // window gets back to its previous state (e.g: unmaximized, tiled in TWMs,
  // etc).
  if (window_fullconnection_mode != browser_fullconnection_mode) {
    return;
  }

  if (restored_bounds_in_pixels_.IsEmpty()) {
    if (IsMaximized()) {
      // The request that we become maximized originated from a different
      // process. |bounds_in_pixels_| already contains our maximized bounds. Do
      // a best effort attempt to get restored bounds by setting it to our
      // previously set bounds (and if we get this wrong, we aren't any worse
      // off since we'd otherwise be returning our maximized bounds).
      restored_bounds_in_pixels_ = previous_bounds_in_pixels_;
    }
  } else if (!IsMaximized() && !IsFullscreen()) {
    // If we have restored bounds, but WM_STATE no longer claims to be
    // maximized or fullscreen, we should clear our restored bounds.
    restored_bounds_in_pixels_ = gfx::Rect();
  }

  if (new_state != state_) {
    auto old_state = state_;
    state_ = new_state;
    platform_window_delegate_->OnWindowStateChanged(old_state, state_);
    if (CanSetDecorationInsets()) {
      UpdateDecorationInsets();
    }
  }

  WindowTiledEdges tiled_state = GetTiledState();
  if (tiled_state != tiled_state_) {
    tiled_state_ = tiled_state;
#if BUILDFLAG(IS_LINUX)
    platform_window_delegate_->OnWindowTiledStateChanged(tiled_state);
    UpdateDecorationInsets();
#endif
  }
}

void X11Window::OnXWindowDamageEvent(const gfx::Rect& damage_rect) {
  platform_window_delegate_->OnDamageRect(damage_rect);
}

void X11Window::OnXWindowCloseRequested() {
  platform_window_delegate_->OnCloseRequest();
}

void X11Window::OnXWindowIsActiveChanged(bool active) {
  platform_window_delegate_->OnActivationChanged(active);
}

void X11Window::OnXWindowWorkspaceChanged() {
  if (workspace_extension_delegate_) {
    workspace_extension_delegate_->OnWorkspaceChanged();
  }
}

void X11Window::OnXWindowLostPointerGrab() {
  if (x11_extension_delegate_) {
    x11_extension_delegate_->OnLostMouseGrab();
  }
}

void X11Window::OnXWindowSelectionEvent(const x11::SelectionNotifyEvent& xev) {
  DCHECK(drag_drop_client_);
  drag_drop_client_->OnSelectionNotify(xev);
}

void X11Window::OnXWindowDragDropEvent(const x11::ClientMessageEvent& xev) {
  DCHECK(drag_drop_client_);
  drag_drop_client_->HandleXdndEvent(xev);
}

std::optional<gfx::Size> X11Window::GetMinimumSizeForXWindow() {
  if (auto max_size = platform_window_delegate_->GetMinimumSizeForWindow()) {
    return platform_window_delegate_->ConvertRectToPixels(gfx::Rect(*max_size))
        .size();
  }
  return std::nullopt;
}

std::optional<gfx::Size> X11Window::GetMaximumSizeForXWindow() {
  if (auto max_size = platform_window_delegate_->GetMaximumSizeForWindow()) {
    return platform_window_delegate_->ConvertRectToPixels(gfx::Rect(*max_size))
        .size();
  }
  return std::nullopt;
}

SkPath X11Window::GetWindowMaskForXWindow() {
  return platform_window_delegate_->GetWindowMaskForWindowShapeInPixels();
}

void X11Window::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location_in_px) {
  int direction = HitTestToWmMoveResizeDirection(hittest);
  if (direction == -1) {
    return;
  }

  DoWMMoveResize(&connection_.get(), x_root_window_, xwindow_,
                 pointer_location_in_px, direction);
}

bool X11Window::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  return x11_window_move_client_->RunMoveLoop(!HasCapture(), drag_offset);
}

void X11Window::EndMoveLoop() {
  x11_window_move_client_->EndMoveLoop();
}

bool X11Window::StartDrag(
    const OSExchangeData& data,
    int operations,
    mojom::DragEventSource source,
    gfx::NativeCursor cursor,
    bool can_grab_pointer,
    base::OnceClosure drag_started_callback,
    WmDragHandler::DragFinishedCallback drag_finished_callback,
    WmDragHandler::LocationDelegate* location_delegate) {
  DCHECK(drag_drop_client_);
  DCHECK(!drag_location_delegate_);

  drag_finished_callback_ = std::move(drag_finished_callback);
  drag_location_delegate_ = location_delegate;
  drag_drop_client_->InitDrag(operations, &data);
  allowed_drag_operations_ = 0;
  notified_enter_ = false;

  drag_loop_ = std::make_unique<X11WholeScreenMoveLoop>(this);

  auto alive = weak_ptr_factory_.GetWeakPtr();
  const bool dropped =
      drag_loop_->RunMoveLoop(can_grab_pointer, last_cursor_, last_cursor_,
                              std::move(drag_started_callback));
  if (!alive) {
    return false;
  }

  drag_loop_.reset();
  drag_location_delegate_ = nullptr;
  drag_drop_client_->CleanupDrag();
  return dropped;
}

void X11Window::CancelDrag() {
  QuitDragLoop();
}

void X11Window::UpdateDragImage(const gfx::ImageSkia& image,
                                const gfx::Vector2d& offset) {
  NOTIMPLEMENTED();
}

std::optional<gfx::AcceleratedWidget> X11Window::GetDragWidget() {
  DCHECK(drag_location_delegate_);
  return drag_location_delegate_->GetDragWidget();
}

int X11Window::UpdateDrag(const gfx::Point& connection_point) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler) {
    return DragDropTypes::DRAG_NONE;
  }

  DCHECK(drag_drop_client_);
  auto* target_current_context = drag_drop_client_->target_current_context();
  DCHECK(target_current_context);

  auto data = std::make_unique<OSExchangeData>(
      std::make_unique<XOSExchangeDataProvider>(
          drag_drop_client_->xwindow(), target_current_context->source_window(),
          target_current_context->fetched_targets()));
  int suggested_operations = target_current_context->GetDragOperation();
  // KDE-based file browsers such as Dolphin change the drag operation depending
  // on whether alt/ctrl/shift was pressed. However once Chromium gets control
  // over the X11 events, the source application does no longer receive X11
  // events for key modifier changes, so the dnd operation gets stuck in an
  // incorrect state. Blink can only dnd-open files of type DRAG_COPY, so the
  // DRAG_COPY mask is added if the dnd object is a file.
  if (data->HasFile() && (suggested_operations & (DragDropTypes::DRAG_MOVE |
                                                  DragDropTypes::DRAG_LINK))) {
    suggested_operations |= DragDropTypes::DRAG_COPY;
  }

  XDragDropClient* source_client =
      XDragDropClient::GetForWindow(target_current_context->source_window());
  gfx::PointF local_point_in_dip =
      platform_window_delegate_->ConvertScreenPointToLocalDIP(connection_point);
  if (!notified_enter_) {
    drop_handler->OnDragEnter(local_point_in_dip, suggested_operations,
                              GetKeyModifiers(source_client));

    // TODO(crbug.com/40073696): Factor DataFetched out of Enter callback.
    drop_handler->OnDragDataAvailable(std::move(data));

    notified_enter_ = true;
  }
  allowed_drag_operations_ = drop_handler->OnDragMotion(
      local_point_in_dip, suggested_operations, GetKeyModifiers(source_client));
  return allowed_drag_operations_;
}

void X11Window::UpdateCursor(DragOperation negotiated_operation) {
  DCHECK(drag_location_delegate_);
  drag_location_delegate_->OnDragOperationChanged(negotiated_operation);
}

void X11Window::OnBeginForeignDrag(x11::Window window) {
  notified_enter_ = false;
  source_window_events_ =
      connection_->ScopedSelectEvent(window, x11::EventMask::PropertyChange);
}

void X11Window::OnEndForeignDrag() {
  source_window_events_.Reset();
}

void X11Window::OnBeforeDragLeave() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler) {
    return;
  }
  drop_handler->OnDragLeave();
  notified_enter_ = false;
}

DragOperation X11Window::PerformDrop() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler || !notified_enter_) {
    return DragOperation::kNone;
  }

  // The drop data has been supplied on entering the window.  The drop handler
  // should have it since then.
  auto* target_current_context = drag_drop_client_->target_current_context();
  DCHECK(target_current_context);
  drop_handler->OnDragDrop(GetKeyModifiers(
      XDragDropClient::GetForWindow(target_current_context->source_window())));
  notified_enter_ = false;
  return PreferredDragOperation(allowed_drag_operations_);
}

void X11Window::EndDragLoop() {
  DCHECK(!drag_finished_callback_.is_null());
  std::move(drag_finished_callback_)
      .Run(PreferredDragOperation(allowed_drag_operations_));
  drag_loop_->EndMoveLoop();
}

void X11Window::OnMouseMovement(const gfx::Point& connection_point,
                                int flags,
                                base::TimeTicks event_time) {
  drag_location_delegate_->OnDragLocationChanged(connection_point);
  drag_drop_client_->HandleMouseMovement(connection_point, flags, event_time);
}

void X11Window::OnMouseReleased() {
  drag_drop_client_->HandleMouseReleased();
}

void X11Window::OnMoveLoopEnded() {
  drag_drop_client_->HandleMoveLoopEnded();
}

void X11Window::SetBoundsOnMove(const gfx::Rect& requested_bounds) {
  SetBoundsInPixels(requested_bounds);
}

scoped_refptr<X11Cursor> X11Window::GetLastCursor() {
  return last_cursor_;
}

gfx::Size X11Window::GetSize() {
  return bounds_in_pixels_.size();
}

void X11Window::QuitDragLoop() {
  DCHECK(drag_loop_);
  drag_loop_->EndMoveLoop();
}

gfx::Size X11Window::AdjustSizeForDisplay(
    const gfx::Size& requested_size_in_pixels) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // We do not need to apply the workaround for the ChromeOS.
  return requested_size_in_pixels;
#else
  auto* screen = display::Screen::GetScreen();
  if (screen && !UseTestConfigForPlatformWindows()) {
    std::vector<display::Display> displays = screen->GetAllDisplays();
    // Compare against all monitor sizes. The window manager can move the window
    // to whichever monitor it wants.
    for (const auto& display : displays) {
      if (requested_size_in_pixels == display.GetSizeInPixel()) {
        return gfx::Size(requested_size_in_pixels.width() - 1,
                         requested_size_in_pixels.height() - 1);
      }
    }
  }

  // Do not request a 0x0 window size. It causes an XError.
  gfx::Size size_in_pixels = requested_size_in_pixels;
  size_in_pixels.SetToMax(gfx::Size(1, 1));
  return size_in_pixels;
#endif
}

void X11Window::CreateXWindow(const PlatformWindowInitProperties& properties) {
  auto bounds =
      platform_window_delegate_->ConvertRectToPixels(properties.bounds);

  gfx::Size adjusted_size_in_pixels = AdjustSizeForDisplay(bounds.size());
  bounds.set_size(adjusted_size_in_pixels);
  const auto override_redirect =
      properties.x11_extension_delegate &&
      properties.x11_extension_delegate->IsOverrideRedirect();

  workspace_extension_delegate_ = properties.workspace_extension_delegate;
  x11_extension_delegate_ = properties.x11_extension_delegate;

  activatable_ = properties.activatable;

  x11::CreateWindowRequest req;
  req.bit_gravity = x11::Gravity::NorthWest;
  req.background_pixel = properties.background_color.has_value()
                             ? properties.background_color.value()
                             : connection_->default_screen().white_pixel;

  switch (properties.type) {
    case PlatformWindowType::kMenu:
      req.override_redirect = x11::Bool32(true);
      break;
    case PlatformWindowType::kTooltip:
      req.override_redirect = x11::Bool32(true);
      break;
    case PlatformWindowType::kPopup:
      req.override_redirect = x11::Bool32(true);
      break;
    case PlatformWindowType::kDrag:
      req.override_redirect = x11::Bool32(true);
      break;
    default:
      break;
  }
  // An in-activatable window should not interact with the system wm.
  if (!activatable_ || override_redirect) {
    req.override_redirect = x11::Bool32(true);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  req.override_redirect = x11::Bool32(UseTestConfigForPlatformWindows());
#endif

  override_redirect_ = req.override_redirect.has_value();

  bool enable_transparent_visuals;
  switch (properties.opacity) {
    case PlatformWindowOpacity::kOpaqueWindow:
      enable_transparent_visuals = false;
      break;
    case PlatformWindowOpacity::kTranslucentWindow:
      enable_transparent_visuals = true;
      break;
    case PlatformWindowOpacity::kInferOpacity:
      enable_transparent_visuals = properties.type == PlatformWindowType::kDrag;
  }

  if (properties.wm_role_name == kStatusIconWmRoleName) {
    std::string atom_name =
        "_NET_SYSTEM_TRAY_S" +
        base::NumberToString(connection_->DefaultScreenId());
    auto selection =
        connection_->GetSelectionOwner({x11::GetAtom(atom_name.c_str())});
    if (auto reply = selection.Sync()) {
      connection_->GetPropertyAs(
          reply->owner, x11::GetAtom("_NET_SYSTEM_TRAY_VISUAL"), &visual_id_);
    }
  }

  x11::VisualId visual_id = visual_id_;
  uint8_t depth = 0;
  x11::ColorMap colormap{};
  auto& visual_manager = connection_->GetOrCreateVisualManager();
  if (visual_id_ == x11::VisualId{} ||
      !visual_manager.GetVisualInfo(visual_id_, &depth, &colormap,
                                    &visual_has_alpha_)) {
    visual_manager.ChooseVisualForWindow(enable_transparent_visuals, &visual_id,
                                         &depth, &colormap, &visual_has_alpha_);
  }
  // When drawing translucent windows, ensure a translucent background pixel
  // value so that a colored border won't be shown in the time after the window
  // has been resized smaller but before Chrome has finished drawing a frame.
  if (visual_has_alpha_) {
    req.background_pixel = 0;
  }

  // x.org will BadMatch if we don't set a border when the depth isn't the
  // same as the parent depth.
  req.border_pixel = 0;

  bounds_in_pixels_ = SanitizeBounds(bounds);
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
}

void X11Window::CloseXWindow() {
  if (xwindow_ == x11::Window::None) {
    return;
  }

  CancelResize();
  UnconfineCursor();
  // Unregister from the global security surface list if necessary.
  if (is_security_surface_) {
    auto& security_surfaces = GetSecuritySurfaces();
    security_surfaces.erase(base::ranges::find(security_surfaces, xwindow_),
                            security_surfaces.end());
  }

  connection_->DestroyWindow({xwindow_});
  xwindow_ = x11::Window::None;

  if (update_counter_ != x11::Sync::Counter{}) {
    connection_->sync().DestroyCounter({update_counter_});
    connection_->sync().DestroyCounter({extended_update_counter_});
    update_counter_ = {};
    extended_update_counter_ = {};
  }
}

void X11Window::Map(bool inactive) {
  // Before we map the window, set size hints. Otherwise, some window managers
  // will ignore toplevel XMoveWindow commands.
  x11::SizeHints size_hints;
  memset(&size_hints, 0, sizeof(size_hints));
  connection_->GetWmNormalHints(xwindow_, &size_hints);
  size_hints.flags |= x11::SIZE_HINT_P_POSITION;
  size_hints.x = bounds_in_pixels_.x();
  size_hints.y = bounds_in_pixels_.y();
  // Set STATIC_GRAVITY so that the window position is not affected by the
  // frame width when running with window manager.
  size_hints.flags |= x11::SIZE_HINT_P_WIN_GRAVITY;
  size_hints.win_gravity = x11::WIN_GRAVITY_HINT_STATIC_GRAVITY;
  connection_->SetWmNormalHints(xwindow_, size_hints);

  ignore_keyboard_input_ = inactive;
  auto wm_user_time_ms = ignore_keyboard_input_
                             ? x11::Time::CurrentTime
                             : X11EventSource::GetInstance()->GetTimestamp();
  if (inactive || wm_user_time_ms != x11::Time::CurrentTime) {
    connection_->SetProperty(xwindow_, x11::GetAtom("_NET_WM_USER_TIME"),
                             x11::Atom::CARDINAL, wm_user_time_ms);
  }

  UpdateMinAndMaxSize();

  UpdateDecorationInsets();

  if (window_properties_.empty()) {
    connection_->DeleteProperty(xwindow_, x11::GetAtom("_NET_WM_STATE"));
  } else {
    connection_->SetArrayProperty(
        xwindow_, x11::GetAtom("_NET_WM_STATE"), x11::Atom::ATOM,
        std::vector<x11::Atom>(std::begin(window_properties_),
                               std::end(window_properties_)));
  }

  connection_->MapWindow({xwindow_});
  window_mapped_in_client_ = true;

  // TODO(thomasanderson): Find out why this flush is necessary.
  connection_->Flush();
}

void X11Window::SetFullscreen(bool fullscreen) {
  SetWMSpecState(fullscreen, x11::GetAtom("_NET_WM_STATE_FULLSCREEN"),
                 x11::Atom::None);
}

bool X11Window::IsActive() const {
  // Focus and stacking order are independent in X11.  Since we cannot guarantee
  // a window is topmost iff it has focus, just use the focus state to determine
  // if a window is active.  Note that Activate() and Deactivate() change the
  // stacking order in addition to changing the focus state.
  return (has_window_focus_ || has_pointer_focus_) && !ignore_keyboard_input_;
}

bool X11Window::IsMinimized() const {
  return HasWMSpecProperty(window_properties_,
                           x11::GetAtom("_NET_WM_STATE_HIDDEN"));
}

bool X11Window::IsMaximized() const {
  // In X11, if a maximized window is minimized, it will have both the "hidden"
  // and "maximized" states.
  if (IsMinimized()) {
    return false;
  }
  return (HasWMSpecProperty(window_properties_,
                            x11::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT")) &&
          HasWMSpecProperty(window_properties_,
                            x11::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ")));
}

bool X11Window::IsFullscreen() const {
  return HasWMSpecProperty(window_properties_,
                           x11::GetAtom("_NET_WM_STATE_FULLSCREEN"));
}

gfx::Rect X11Window::GetOuterBounds() const {
  gfx::Rect outer_bounds(bounds_in_pixels_);
  outer_bounds.Inset(-native_window_frame_borders_in_pixels_);
  return outer_bounds;
}

void X11Window::ResetWindowRegion() {
  std::unique_ptr<std::vector<x11::Rectangle>> xregion;
  if (!custom_window_shape_ && !IsMaximized() && !IsFullscreen()) {
    SkPath window_mask = GetWindowMaskForXWindow();
    // Some frame views define a custom (non-rectangular) window mask. If
    // so, use it to define the window shape. If not, fall through.
    if (window_mask.countPoints() > 0) {
      xregion = x11::CreateRegionFromSkPath(window_mask);
    }
  }
  UpdateWindowRegion(std::move(xregion));
}

void X11Window::OnWorkspaceUpdated() {
  auto old_workspace = workspace_;
  int workspace;
  if (GetWindowDesktop(xwindow_, &workspace)) {
    workspace_ = workspace;
  } else {
    workspace_ = std::nullopt;
  }

  if (workspace_ != old_workspace) {
    OnXWindowWorkspaceChanged();
  }
}

void X11Window::SetFlashFrameHint(bool flash_frame) {
  if (urgency_hint_set_ == flash_frame) {
    return;
  }

  x11::WmHints hints;
  memset(&hints, 0, sizeof(hints));
  connection_->GetWmHints(xwindow_, &hints);

  if (flash_frame) {
    hints.flags |= x11::WM_HINT_X_URGENCY;
  } else {
    hints.flags &= ~x11::WM_HINT_X_URGENCY;
  }

  connection_->SetWmHints(xwindow_, hints);

  urgency_hint_set_ = flash_frame;
}

void X11Window::UpdateMinAndMaxSize() {
  std::optional<gfx::Size> minimum_in_pixels = GetMinimumSizeForXWindow();
  std::optional<gfx::Size> maximum_in_pixels = GetMaximumSizeForXWindow();
  if ((!minimum_in_pixels ||
       min_size_in_pixels_ == minimum_in_pixels.value()) &&
      (!maximum_in_pixels ||
       max_size_in_pixels_ == maximum_in_pixels.value())) {
    return;
  }

  min_size_in_pixels_ = minimum_in_pixels.value();
  max_size_in_pixels_ = maximum_in_pixels.value();

  x11::SizeHints hints;
  memset(&hints, 0, sizeof(hints));
  connection_->GetWmNormalHints(xwindow_, &hints);

  if (min_size_in_pixels_.IsEmpty()) {
    hints.flags &= ~x11::SIZE_HINT_P_MIN_SIZE;
  } else {
    hints.flags |= x11::SIZE_HINT_P_MIN_SIZE;
    hints.min_width = min_size_in_pixels_.width();
    hints.min_height = min_size_in_pixels_.height();
  }

  if (max_size_in_pixels_.IsEmpty()) {
    hints.flags &= ~x11::SIZE_HINT_P_MAX_SIZE;
  } else {
    hints.flags |= x11::SIZE_HINT_P_MAX_SIZE;
    hints.max_width = max_size_in_pixels_.width();
    hints.max_height = max_size_in_pixels_.height();
  }

  connection_->SetWmNormalHints(xwindow_, hints);
}

void X11Window::BeforeActivationStateChanged() {
  was_active_ = IsActive();
  had_pointer_ = has_pointer_;
  had_pointer_grab_ = has_pointer_grab_;
  had_window_focus_ = has_window_focus_;
}

void X11Window::AfterActivationStateChanged() {
  if (had_pointer_grab_ && !has_pointer_grab_) {
    OnXWindowLostPointerGrab();
  }

  if (had_pointer_grab_ && !has_pointer_grab_) {
    OnXWindowLostCapture();
  }

  bool is_active = IsActive();
  if (!was_active_ && is_active) {
    SetFlashFrameHint(false);
  }

  if (was_active_ != is_active) {
    OnXWindowIsActiveChanged(is_active);
  }
}

void X11Window::MaybeUpdateOcclusionState() {
  PlatformWindowOcclusionState occlusion_state =
      is_occluded_ ? PlatformWindowOcclusionState::kOccluded
                   : PlatformWindowOcclusionState::kVisible;

  if (!window_mapped_in_client_ || IsMinimized()) {
    occlusion_state = PlatformWindowOcclusionState::kHidden;
  }

  if (occlusion_state != occlusion_state_) {
    occlusion_state_ = occlusion_state;
    platform_window_delegate_->OnOcclusionStateChanged(occlusion_state);
  }
}

void X11Window::OnCrossingEvent(bool enter,
                                bool focus_in_window_or_ancestor,
                                x11::NotifyMode mode,
                                x11::NotifyDetail detail) {
  // NotifyInferior on a crossing event means the pointer moved into or out of a
  // child window, but the pointer is still within |xwindow_|.
  if (detail == x11::NotifyDetail::Inferior) {
    return;
  }

  BeforeActivationStateChanged();

  if (mode == x11::NotifyMode::Grab) {
    has_pointer_grab_ = enter;
  } else if (mode == x11::NotifyMode::Ungrab) {
    has_pointer_grab_ = false;
  }

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

void X11Window::OnFocusEvent(bool focus_in,
                             x11::NotifyMode mode,
                             x11::NotifyDetail detail) {
  // NotifyInferior on a focus event means the focus moved into or out of a
  // child window, but the focus is still within |xwindow_|.
  if (detail == x11::NotifyDetail::Inferior) {
    return;
  }

  bool notify_grab =
      mode == x11::NotifyMode::Grab || mode == x11::NotifyMode::Ungrab;

  BeforeActivationStateChanged();

  // For every focus change, the X server sends normal focus events which are
  // useful for tracking |has_window_focus_|, but supplements these events with
  // NotifyPointer events which are only useful for tracking pointer focus.

  // For |has_pointer_focus_| and |has_window_focus_|, we continue tracking
  // state during a grab, but ignore grab/ungrab events themselves.
  if (!notify_grab && detail != x11::NotifyDetail::Pointer) {
    has_window_focus_ = focus_in;
  }

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

bool X11Window::IsTargetedBy(const x11::Event& xev) const {
  if (auto* button = xev.As<x11::ButtonEvent>()) {
    return button->event == xwindow_;
  }
  if (auto* key = xev.As<x11::KeyEvent>()) {
    return key->event == xwindow_;
  }
  if (auto* motion = xev.As<x11::MotionNotifyEvent>()) {
    return motion->event == xwindow_;
  }
  if (auto* xievent = xev.As<x11::Input::DeviceEvent>()) {
    return xievent->event == xwindow_;
  }
  if (auto* motion = xev.As<x11::MotionNotifyEvent>()) {
    return motion->event == xwindow_;
  }
  if (auto* crossing = xev.As<x11::CrossingEvent>()) {
    return crossing->event == xwindow_;
  }
  if (auto* expose = xev.As<x11::ExposeEvent>()) {
    return expose->window == xwindow_;
  }
  if (auto* focus = xev.As<x11::FocusEvent>()) {
    return focus->event == xwindow_;
  }
  if (auto* configure = xev.As<x11::ConfigureNotifyEvent>()) {
    return configure->window == xwindow_;
  }
  if (auto* crossing_input = xev.As<x11::Input::CrossingEvent>()) {
    return crossing_input->event == xwindow_;
  }
  if (auto* map = xev.As<x11::MapNotifyEvent>()) {
    return map->window == xwindow_;
  }
  if (auto* unmap = xev.As<x11::UnmapNotifyEvent>()) {
    return unmap->window == xwindow_;
  }
  if (auto* client = xev.As<x11::ClientMessageEvent>()) {
    return client->window == xwindow_;
  }
  if (auto* property = xev.As<x11::PropertyNotifyEvent>()) {
    return property->window == xwindow_;
  }
  if (auto* selection = xev.As<x11::SelectionNotifyEvent>()) {
    return selection->requestor == xwindow_;
  }
  if (auto* visibility = xev.As<x11::VisibilityNotifyEvent>()) {
    return visibility->window == xwindow_;
  }
  return false;
}

void X11Window::SetTransientWindow(x11::Window window) {
  transient_window_ = window;
}

void X11Window::HandleEvent(const x11::Event& xev) {
  if (!IsTargetedBy(xev)) {
    return;
  }

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
      NotifyBoundsChanged(/*origin changed=*/true);
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
    OnConfigureEvent(*configure, xev.send_event());
  } else if (auto* crossing_input = xev.As<x11::Input::CrossingEvent>()) {
    TouchFactory* factory = TouchFactory::GetInstance();
    if (factory->ShouldProcessCrossingEvent(*crossing_input)) {
      auto mode = XI2ModeToXMode(crossing_input->mode);
      auto detail = XI2DetailToXDetail(crossing_input->detail);
      switch (crossing_input->opcode) {
        case x11::Input::CrossingEvent::Enter:
          OnCrossingEvent(true, crossing_input->focus, mode, detail);
          break;
        case x11::Input::CrossingEvent::Leave:
          OnCrossingEvent(false, crossing_input->focus, mode, detail);
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
        connection_->SendEvent(reply_event, x_root_window_,
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
    if (changed_atom == x11::GetAtom("_NET_WM_STATE")) {
      OnWMStateUpdated();
    } else if (changed_atom == x11::GetAtom("_NET_FRAME_EXTENTS")) {
      OnFrameExtentsUpdated();
    } else if (changed_atom == x11::GetAtom("_NET_WM_DESKTOP")) {
      OnWorkspaceUpdated();
    }
  } else if (auto* selection = xev.As<x11::SelectionNotifyEvent>()) {
    OnXWindowSelectionEvent(*selection);
  } else if (auto* visibility = xev.As<x11::VisibilityNotifyEvent>()) {
    is_occluded_ = visibility->state == x11::Visibility::FullyObscured;
    MaybeUpdateOcclusionState();
  }
}

void X11Window::UpdateWMUserTime(Event* event) {
  if (!IsActive()) {
    return;
  }
  DCHECK(event);
  EventType type = event->type();
  if (type == EventType::kMousePressed || type == EventType::kKeyPressed ||
      type == EventType::kTouchPressed) {
    uint32_t wm_user_time_ms =
        (event->time_stamp() - base::TimeTicks()).InMilliseconds();
    connection_->SetProperty(xwindow_, x11::GetAtom("_NET_WM_USER_TIME"),
                             x11::Atom::CARDINAL, wm_user_time_ms);
  }
}

void X11Window::OnWindowMapped() {
  window_mapped_in_server_ = true;
  // Some WMs only respect maximize hints after the window has been mapped.
  // Check whether we need to re-do a maximization.
  if (should_maximize_after_map_) {
    Maximize();
    should_maximize_after_map_ = false;
  }
  if (should_grab_pointer_after_map_) {
    has_pointer_grab_ |=
        (ui::GrabPointer(xwindow_, true, nullptr) == x11::GrabStatus::Success);
    should_grab_pointer_after_map_ = false;
  }
}

void X11Window::OnConfigureEvent(const x11::ConfigureNotifyEvent& configure,
                                 bool send_event) {
  DCHECK_EQ(xwindow_, configure.window);

  if (pending_counter_value_) {
    DCHECK(!configure_counter_value_);
    configure_counter_value_ = pending_counter_value_;
    configure_counter_value_is_extended_ = pending_counter_value_is_extended_;
    pending_counter_value_is_extended_ = false;
    pending_counter_value_ = 0;
  }

  // During a Restore() -> ToggleFullscreen() or Restore() -> SetBounds() ->
  // ToggleFullscreen() sequence, ignore the configure events from the Restore
  // and SetBounds requests, if we're waiting on fullscreen.  After
  // OnXWindowStateChanged unsets this flag, there will be a configuration event
  // that will set the bounds to the final fullscreen bounds.
  if (ignore_next_configures_ > 0) {
    ignore_next_configures_--;
    return;
  }

  // Note: This OnConfigureEvent might not necessarily correspond to a previous
  // SetBounds request. Due to limitations in X11 there isn't a way to
  // match events to its original request. For now, we assume that the next
  // OnConfigureEvent event after a SetBounds (ConfigureWindow) request is from
  // that request. This would break in some scenarios (for example calling
  // SetBounds more than once quickly). See crbug.com/1227451.
  bounds_change_in_flight_ = false;

  // It's possible that the X window may be resized by some other means than
  // from within aura (e.g. the X window manager can change the size). Make
  // sure the root window size is maintained properly.
  int translated_x_in_pixels = configure.x;
  int translated_y_in_pixels = configure.y;
  if (!send_event && !configure.override_redirect) {
    auto future =
        connection_->TranslateCoordinates({xwindow_, x_root_window_, 0, 0});
    if (auto coords = future.Sync()) {
      translated_x_in_pixels = coords->dst_x;
      translated_y_in_pixels = coords->dst_y;
    }
  }
  gfx::Rect new_bounds_px(translated_x_in_pixels, translated_y_in_pixels,
                          configure.width, configure.height);
  const bool size_changed = bounds_in_pixels_.size() != new_bounds_px.size();
  const bool origin_changed =
      bounds_in_pixels_.origin() != new_bounds_px.origin();
  previous_bounds_in_pixels_ = bounds_in_pixels_;
  bounds_in_pixels_ = new_bounds_px;

  if (size_changed) {
    DispatchResize(origin_changed);
  } else if (origin_changed) {
    NotifyBoundsChanged(/*origin changed=*/true);
  }
}

void X11Window::SetWMSpecState(bool enabled,
                               x11::Atom state1,
                               x11::Atom state2) {
  if (window_mapped_in_client_) {
    ui::SetWMSpecState(xwindow_, enabled, state1, state2);
  } else {
    // The updated state will be set when the window is (re)mapped.
    base::flat_set<x11::Atom> new_window_properties = window_properties_;
    for (x11::Atom atom : {state1, state2}) {
      if (enabled) {
        new_window_properties.insert(atom);
      } else {
        new_window_properties.erase(atom);
      }
    }
    UpdateWindowProperties(new_window_properties);
  }
}

void X11Window::OnWMStateUpdated() {
  // The EWMH spec requires window managers to remove the _NET_WM_STATE property
  // when a window is unmapped.  However, Chromium code wants the state to
  // persist across a Hide() and Show().  So if the window is currently
  // unmapped, leave the state unchanged so it will be restored when the window
  // is remapped.
  std::vector<x11::Atom> atom_list;
  if (connection_->GetArrayProperty(xwindow_, x11::GetAtom("_NET_WM_STATE"),
                                    &atom_list) ||
      window_mapped_in_client_) {
    UpdateWindowProperties(
        base::flat_set<x11::Atom>(std::begin(atom_list), std::end(atom_list)));
  }
}

WindowTiledEdges X11Window::GetTiledState() const {
  const bool vert = HasWMSpecProperty(
      window_properties_, x11::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"));
  const bool horz = HasWMSpecProperty(
      window_properties_, x11::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
  return WindowTiledEdges{vert, vert, horz, horz};
}

void X11Window::UpdateWindowProperties(
    const base::flat_set<x11::Atom>& new_window_properties) {
  // If the window is hidden, ignore new properties.
  // See https://crbug.com/1260832
  if (!window_mapped_in_client_) {
    return;
  }

  window_properties_ = new_window_properties;

  // Ignore requests by the window manager to enter or exit fullscreen (e.g. as
  // a result of pressing a window manager accelerator key). Chrome does not
  // handle window manager initiated fullscreen. In particular, Chrome needs to
  // do preprocessing before the x window's fullscreen state is toggled.

  is_always_on_top_ = HasWMSpecProperty(window_properties_,
                                        x11::GetAtom("_NET_WM_STATE_ABOVE"));
  OnXWindowStateChanged();
  MaybeUpdateOcclusionState();
  ResetWindowRegion();
}

void X11Window::OnFrameExtentsUpdated() {
  std::vector<int32_t> insets;
  if (connection_->GetArrayProperty(
          xwindow_, x11::GetAtom("_NET_FRAME_EXTENTS"), &insets) &&
      insets.size() == 4) {
    // |insets| are returned in the order: [left, right, top, bottom].
    native_window_frame_borders_in_pixels_ =
        gfx::Insets::TLBR(insets[2], insets[0], insets[3], insets[1]);
  } else {
    native_window_frame_borders_in_pixels_ = gfx::Insets();
  }
}

// Removes |delayed_resize_task_| from the task queue (if it's in the queue) and
// adds it back at the end of the queue.
void X11Window::DispatchResize(bool origin_changed) {
  if (update_counter_ == x11::Sync::Counter{} ||
      configure_counter_value_ == 0) {
    // WM doesn't support _NET_WM_SYNC_REQUEST. Or we are too slow, so
    // _NET_WM_SYNC_REQUEST is disabled by the compositor.
    delayed_resize_task_.Reset(base::BindOnce(
        &X11Window::DelayedResize, base::Unretained(this), origin_changed));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, delayed_resize_task_.callback());
    return;
  }

  if (configure_counter_value_is_extended_) {
    current_counter_value_ = configure_counter_value_;
    configure_counter_value_ = 0;
    // Make sure the counter is even number.
    if ((current_counter_value_ % 2) == 1) {
      ++current_counter_value_;
    }
  }

  // If _NET_WM_SYNC_REQUEST is used to synchronize with compositor during
  // resizing, the compositor will not resize the window, until last resize is
  // handled, so we don't need accumulate resize events.
  DelayedResize(origin_changed);
}

void X11Window::DelayedResize(bool origin_changed) {
  if (configure_counter_value_is_extended_ &&
      (current_counter_value_ % 2) == 0) {
    // Increase the |extended_update_counter_|, so the compositor will know we
    // are not frozen and re-enable _NET_WM_SYNC_REQUEST, if it was disabled.
    // Increase the |extended_update_counter_| to an odd number will not trigger
    // a new resize.
    SyncSetCounter(&connection_.get(), extended_update_counter_,
                   ++current_counter_value_);
  }

  CancelResize();
  NotifyBoundsChanged(/*origin changed=*/origin_changed);

  // No more member accesses here: bounds change propagation may have deleted
  // |this| (e.g. when a chrome window is snapped into a tab strip. Further
  // details at crbug.com/1068755).
}

void X11Window::CancelResize() {
  delayed_resize_task_.Cancel();
}

void X11Window::UnconfineCursor() {
  if (!has_pointer_barriers_) {
    return;
  }

  for (auto pointer_barrier : pointer_barriers_) {
    connection_->xfixes().DeletePointerBarrier({pointer_barrier});
  }

  pointer_barriers_.fill({});

  has_pointer_barriers_ = false;
}

void X11Window::UpdateWindowRegion(
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
  if (custom_window_shape_) {
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
  if (use_native_frame_) {
    // If the window has system borders, the mask must be set to null (not a
    // rectangle), because several window managers (eg, KDE, XFCE, XMonad) will
    // not put borders on a window with a custom shape.
    connection_->shape().Mask(x11::Shape::MaskRequest{
        .operation = x11::Shape::So::Set,
        .destination_kind = x11::Shape::Sk::Bounding,
        .destination_window = xwindow_,
        .source_bitmap = x11::Pixmap::None,
    });
  }
}

void X11Window::NotifyBoundsChanged(bool origin_changed) {
  ResetWindowRegion();
  platform_window_delegate_->OnBoundsChanged({origin_changed});
}

bool X11Window::InitializeAsStatusIcon() {
  std::string atom_name = "_NET_SYSTEM_TRAY_S" +
                          base::NumberToString(connection_->DefaultScreenId());
  auto reply =
      connection_->GetSelectionOwner({x11::GetAtom(atom_name.c_str())}).Sync();
  if (!reply || reply->owner == x11::Window::None) {
    return false;
  }
  auto manager = reply->owner;

  connection_->SetArrayProperty(
      xwindow_, x11::GetAtom("_XEMBED_INFO"), x11::Atom::CARDINAL,
      std::vector<uint32_t>{kXembedInfoProtocolVersion, kXembedInfoFlags});

  x11::ChangeWindowAttributesRequest req{xwindow_};
  if (visual_has_alpha_) {
    req.background_pixel = 0;
  } else {
    connection_->SetProperty(xwindow_,
                             x11::GetAtom("CHROMIUM_COMPOSITE_WINDOW"),
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
