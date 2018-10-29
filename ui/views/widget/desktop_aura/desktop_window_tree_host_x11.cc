// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/class_property.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/layout.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/base/x/x11_window_event_manager.h"
#include "ui/display/screen.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event_utils.h"
#include "ui/events/keyboard_hook.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/path.h"
#include "ui/gfx/path_x11.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_delegate.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_observer_x11.h"
#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"
#include "ui/views/widget/desktop_aura/x11_desktop_window_move_client.h"
#include "ui/views/widget/desktop_aura/x11_window_event_filter.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/window_util.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(views::DesktopWindowTreeHostX11*);

namespace views {

DesktopWindowTreeHostX11* DesktopWindowTreeHostX11::g_current_capture =
    NULL;
std::list<XID>* DesktopWindowTreeHostX11::open_windows_ = NULL;

DEFINE_UI_CLASS_PROPERTY_KEY(
    aura::Window*, kViewsWindowForRootWindow, NULL);

DEFINE_UI_CLASS_PROPERTY_KEY(
    DesktopWindowTreeHostX11*, kHostForRootWindow, NULL);

namespace {

// Special value of the _NET_WM_DESKTOP property which indicates that the window
// should appear on all desktops.
const int kAllDesktops = 0xFFFFFFFF;

const char kX11WindowRolePopup[] = "popup";
const char kX11WindowRoleBubble[] = "bubble";

// Returns the whole path from |window| to the root.
std::vector<::Window> GetParentsList(XDisplay* xdisplay, ::Window window) {
  ::Window parent_win, root_win;
  Window* child_windows;
  unsigned int num_child_windows;
  std::vector<::Window> result;

  while (window) {
    result.push_back(window);
    if (!XQueryTree(xdisplay, window,
                    &root_win, &parent_win, &child_windows, &num_child_windows))
      break;
    if (child_windows)
      XFree(child_windows);
    window = parent_win;
  }
  return result;
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

int IgnoreX11Errors(XDisplay* display, XErrorEvent* error) {
  return 0;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, public:

DesktopWindowTreeHostX11::DesktopWindowTreeHostX11(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : xdisplay_(gfx::GetXDisplay()),
      xwindow_(0),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      window_mapped_in_server_(false),
      window_mapped_in_client_(false),
      is_fullscreen_(false),
      is_always_on_top_(false),
      use_native_frame_(false),
      should_maximize_after_map_(false),
      use_argb_visual_(false),
      drag_drop_client_(NULL),
      native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      window_parent_(NULL),
      custom_window_shape_(false),
      urgency_hint_set_(false),
      has_pointer_grab_(false),
      activatable_(true),
      has_pointer_(false),
      has_window_focus_(false),
      has_pointer_focus_(false),
      modal_dialog_counter_(0),
      close_widget_factory_(this),
      weak_factory_(this) {}

DesktopWindowTreeHostX11::~DesktopWindowTreeHostX11() {
  window()->ClearProperty(kHostForRootWindow);
  wm::SetWindowMoveClient(window(), NULL);
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
  DestroyDispatcher();
}

// static
aura::Window* DesktopWindowTreeHostX11::GetContentWindowForXID(XID xid) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(xid);
  return host ? host->window()->GetProperty(kViewsWindowForRootWindow) : NULL;
}

// static
DesktopWindowTreeHostX11* DesktopWindowTreeHostX11::GetHostForXID(XID xid) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(xid);
  return host ? host->window()->GetProperty(kHostForRootWindow) : NULL;
}

// static
std::vector<aura::Window*> DesktopWindowTreeHostX11::GetAllOpenWindows() {
  std::vector<aura::Window*> windows(open_windows().size());
  std::transform(open_windows().begin(),
                 open_windows().end(),
                 windows.begin(),
                 GetContentWindowForXID);
  return windows;
}

gfx::Rect DesktopWindowTreeHostX11::GetX11RootWindowBounds() const {
  return bounds_in_pixels_;
}

gfx::Rect DesktopWindowTreeHostX11::GetX11RootWindowOuterBounds() const {
  gfx::Rect outer_bounds(bounds_in_pixels_);
  outer_bounds.Inset(-native_window_frame_borders_in_pixels_);
  return outer_bounds;
}

::Region DesktopWindowTreeHostX11::GetWindowShape() const {
  return window_shape_.get();
}

void DesktopWindowTreeHostX11::BeforeActivationStateChanged() {
  was_active_ = IsActive();
  had_pointer_ = has_pointer_;
  had_pointer_grab_ = has_pointer_grab_;
  had_window_focus_ = has_window_focus_;
}

void DesktopWindowTreeHostX11::AfterActivationStateChanged() {
  if (had_pointer_grab_ && !has_pointer_grab_)
    dispatcher()->OnHostLostMouseGrab();

  bool had_pointer_capture = had_pointer_ || had_pointer_grab_;
  bool has_pointer_capture = has_pointer_ || has_pointer_grab_;
  if (had_pointer_capture && !has_pointer_capture)
    OnHostLostWindowCapture();

  if (!was_active_ && IsActive()) {
    FlashFrame(false);
    // TODO(thomasanderson): Remove this window shuffling and use XWindowCache
    // instead.
    open_windows().remove(xwindow_);
    open_windows().insert(open_windows().begin(), xwindow_);
  }

  if (was_active_ != IsActive()) {
    desktop_native_widget_aura_->HandleActivationChanged(IsActive());
    native_widget_delegate_->AsWidget()->GetRootView()->SchedulePaint();
  }
}

void DesktopWindowTreeHostX11::OnCrossingEvent(bool enter,
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

void DesktopWindowTreeHostX11::OnFocusEvent(bool focus_in,
                                            int mode,
                                            int detail) {
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
        // 2. Focus moves from a decendant of |xwindow_| to an ancestor
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
        // 4. Focus moves from a decendant of |xwindow_| to the PointerRoot
        // 5. Focus moves from None to the PointerRoot
        // 6. Focus moves from Other to the PointerRoot
        // 7. Focus moves from None to an ancestor of |xwindow_|
        // 8. Focus moves from Other to an ancestor fo |xwindow_|
        // In each case, we will get a FocusIn with a detail of NotifyPointer.
        // The remaining cases for |has_pointer_focus_| becoming false are:
        // 3. Focus moves from the PointerRoot to |xwindow_|
        // 4. Focus moves from the PointerRoot to a decendant of |xwindow|
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
        // 2. Focus moves from Other to a decendant of |xwindow_|
        //    (FocusIn with NotifyNonlinearVirtual)
        // 3. Focus moves from |xwindow_| to Other
        //    (FocusOut with NotifyNonlinear)
        // 4. Focus moves from a decendant of |xwindow_| to Other
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

void DesktopWindowTreeHostX11::AddObserver(
    DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.AddObserver(observer);
}

void DesktopWindowTreeHostX11::RemoveObserver(
    DesktopWindowTreeHostObserverX11* observer) {
  observer_list_.RemoveObserver(observer);
}

void DesktopWindowTreeHostX11::SwapNonClientEventHandler(
    std::unique_ptr<ui::EventHandler> handler) {
  wm::CompoundEventFilter* compound_event_filter =
      desktop_native_widget_aura_->root_window_event_filter();
  if (x11_non_client_event_filter_)
    compound_event_filter->RemoveHandler(x11_non_client_event_filter_.get());
  compound_event_filter->AddHandler(handler.get());
  x11_non_client_event_filter_ = std::move(handler);
}

void DesktopWindowTreeHostX11::CleanUpWindowList(
    void (*func)(aura::Window* window)) {
  if (!open_windows_)
    return;
  while (!open_windows_->empty()) {
    XID xid = open_windows_->front();
    func(GetContentWindowForXID(xid));
    if (!open_windows_->empty() && open_windows_->front() == xid)
      open_windows_->erase(open_windows_->begin());
  }

  delete open_windows_;
  open_windows_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, DesktopWindowTreeHost implementation:

void DesktopWindowTreeHostX11::Init(const Widget::InitParams& params) {
  activatable_ = (params.activatable == Widget::InitParams::ACTIVATABLE_YES);

  if (params.type == Widget::InitParams::TYPE_WINDOW)
    content_window()->SetProperty(aura::client::kAnimationsDisabledKey, true);

  // TODO(erg): Check whether we *should* be building a WindowTreeHost here, or
  // whether we should be proxying requests to another DRWHL.

  // In some situations, views tries to make a zero sized window, and that
  // makes us crash. Make sure we have valid sizes.
  Widget::InitParams sanitized_params = params;
  if (sanitized_params.bounds.width() == 0)
    sanitized_params.bounds.set_width(100);
  if (sanitized_params.bounds.height() == 0)
    sanitized_params.bounds.set_height(100);

  InitX11Window(sanitized_params);
  InitHost();
  window()->Show();
}

void DesktopWindowTreeHostX11::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  window()->SetProperty(kViewsWindowForRootWindow, content_window());
  window()->SetProperty(kHostForRootWindow, this);

  // Ensure that the X11DesktopHandler exists so that it tracks create/destroy
  // notify events.
  X11DesktopHandler::get();

  // TODO(erg): Unify this code once the other consumer goes away.
  SwapNonClientEventHandler(
      std::unique_ptr<ui::EventHandler>(new X11WindowEventFilter(this)));
  SetUseNativeFrame(params.type == Widget::InitParams::TYPE_WINDOW &&
                    !params.remove_standard_frame);

  x11_window_move_client_.reset(new X11DesktopWindowMoveClient);
  wm::SetWindowMoveClient(window(), x11_window_move_client_.get());

  SetWindowTransparency();

  native_widget_delegate_->OnNativeWidgetCreated(true);
}

void DesktopWindowTreeHostX11::OnWidgetInitDone() {}

void DesktopWindowTreeHostX11::OnActiveWindowChanged(bool active) {}

std::unique_ptr<corewm::Tooltip> DesktopWindowTreeHostX11::CreateTooltip() {
  return base::WrapUnique(new corewm::TooltipAura);
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostX11::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
  drag_drop_client_ = new DesktopDragDropClientAuraX11(
      window(), cursor_manager, xdisplay_, xwindow_);
  drag_drop_client_->Init();
  return base::WrapUnique(drag_drop_client_);
}

void DesktopWindowTreeHostX11::Close() {
  content_window()->Hide();

  // TODO(erg): Might need to do additional hiding tasks here.
  delayed_resize_task_.Cancel();

  if (!close_widget_factory_.HasWeakPtrs()) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&DesktopWindowTreeHostX11::CloseNow,
                              close_widget_factory_.GetWeakPtr()));
  }
}

void DesktopWindowTreeHostX11::CloseNow() {
  if (xwindow_ == x11::None)
    return;

  ReleaseCapture();
  native_widget_delegate_->OnNativeWidgetDestroying();

  // If we have children, close them. Use a copy for iteration because they'll
  // remove themselves.
  std::set<DesktopWindowTreeHostX11*> window_children_copy = window_children_;
  for (auto it = window_children_copy.begin(); it != window_children_copy.end();
       ++it) {
    (*it)->CloseNow();
  }
  DCHECK(window_children_.empty());

  // If we have a parent, remove ourselves from its children list.
  if (window_parent_) {
    window_parent_->window_children_.erase(this);
    window_parent_ = NULL;
  }

  // Remove the event listeners we've installed. We need to remove these
  // because otherwise we get assert during ~WindowEventDispatcher().
  desktop_native_widget_aura_->root_window_event_filter()->RemoveHandler(
      x11_non_client_event_filter_.get());
  x11_non_client_event_filter_.reset();

  // Destroy the compositor before destroying the |xwindow_| since shutdown
  // may try to swap, and the swap without a window causes an X error, which
  // causes a crash with in-process renderer.
  DestroyCompositor();

  open_windows().remove(xwindow_);
  // Actually free our native resources.
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  XDestroyWindow(xdisplay_, xwindow_);
  xwindow_ = x11::None;

  desktop_native_widget_aura_->OnHostClosed();
}

aura::WindowTreeHost* DesktopWindowTreeHostX11::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostX11::Show(ui::WindowShowState show_state,
                                    const gfx::Rect& restore_bounds) {
  if (compositor())
    SetVisible(true);

  if (!IsVisible())
    MapWindow(show_state);

  switch (show_state) {
    case ui::SHOW_STATE_MAXIMIZED:
      Maximize();
      if (!restore_bounds.IsEmpty()) {
        // Enforce |restored_bounds_in_pixels_| since calling Maximize() could
        // have reset it.
        restored_bounds_in_pixels_ = ToPixelRect(restore_bounds);
      }

      break;
    case ui::SHOW_STATE_MINIMIZED:
      Minimize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      SetFullscreen(true);
      break;
    default:
      break;
  }

  native_widget_delegate_->AsWidget()->SetInitialFocus(show_state);

  content_window()->Show();
}

bool DesktopWindowTreeHostX11::IsVisible() const {
  return window_mapped_in_client_ && !IsMinimized();
}

void DesktopWindowTreeHostX11::SetSize(const gfx::Size& requested_size) {
  gfx::Size size_in_pixels = ToPixelRect(gfx::Rect(requested_size)).size();
  size_in_pixels = AdjustSize(size_in_pixels);
  bool size_changed = bounds_in_pixels_.size() != size_in_pixels;
  XResizeWindow(xdisplay_, xwindow_, size_in_pixels.width(),
                size_in_pixels.height());
  bounds_in_pixels_.set_size(size_in_pixels);
  if (size_changed) {
    OnHostResizedInPixels(size_in_pixels);
    ResetWindowRegion();
  }
}

void DesktopWindowTreeHostX11::StackAbove(aura::Window* window) {
  if (window && window->GetRootWindow()) {
    ::Window window_below = window->GetHost()->GetAcceleratedWidget();
    // Find all parent windows up to the root.
    std::vector<::Window> window_below_parents =
        GetParentsList(xdisplay_, window_below);
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
      // First stack |xwindow_| below so Z-order of |window| stays the same.
      ::Window windows[] = {*it_below_window, *it_above_window};
      if (XRestackWindows(xdisplay_, windows, 2) == 0) {
        // Now stack them properly.
        std::swap(windows[0], windows[1]);
        XRestackWindows(xdisplay_, windows, 2);
      }
    }
  }
}

void DesktopWindowTreeHostX11::StackAtTop() {
  XRaiseWindow(xdisplay_, xwindow_);
}

void DesktopWindowTreeHostX11::CenterWindow(const gfx::Size& size) {
  gfx::Size size_in_pixels = ToPixelRect(gfx::Rect(size)).size();
  gfx::Rect parent_bounds_in_pixels = ToPixelRect(GetWorkAreaBoundsInScreen());

  // If |window_|'s transient parent bounds are big enough to contain |size|,
  // use them instead.
  if (wm::GetTransientParent(content_window())) {
    gfx::Rect transient_parent_rect =
        wm::GetTransientParent(content_window())->GetBoundsInScreen();
    if (transient_parent_rect.height() >= size.height() &&
        transient_parent_rect.width() >= size.width()) {
      parent_bounds_in_pixels = ToPixelRect(transient_parent_rect);
    }
  }

  gfx::Rect window_bounds_in_pixels(
      parent_bounds_in_pixels.x() +
          (parent_bounds_in_pixels.width() - size_in_pixels.width()) / 2,
      parent_bounds_in_pixels.y() +
          (parent_bounds_in_pixels.height() - size_in_pixels.height()) / 2,
      size_in_pixels.width(), size_in_pixels.height());
  // Don't size the window bigger than the parent, otherwise the user may not be
  // able to close or move it.
  window_bounds_in_pixels.AdjustToFit(parent_bounds_in_pixels);

  SetBoundsInPixels(window_bounds_in_pixels);
}

void DesktopWindowTreeHostX11::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = GetRestoredBounds();

  if (IsFullscreen()) {
    *show_state = ui::SHOW_STATE_FULLSCREEN;
  } else if (IsMinimized()) {
    *show_state = ui::SHOW_STATE_MINIMIZED;
  } else if (IsMaximized()) {
    *show_state = ui::SHOW_STATE_MAXIMIZED;
  } else if (!IsActive()) {
    *show_state = ui::SHOW_STATE_INACTIVE;
  } else {
    *show_state = ui::SHOW_STATE_NORMAL;
  }
}

gfx::Rect DesktopWindowTreeHostX11::GetWindowBoundsInScreen() const {
  return ToDIPRect(bounds_in_pixels_);
}

gfx::Rect DesktopWindowTreeHostX11::GetClientAreaBoundsInScreen() const {
  // TODO(erg): The NativeWidgetAura version returns |bounds_in_pixels_|,
  // claiming it's needed for View::ConvertPointToScreen() to work correctly.
  // DesktopWindowTreeHostWin::GetClientAreaBoundsInScreen() just asks windows
  // what it thinks the client rect is.
  //
  // Attempts to calculate the rect by asking the NonClientFrameView what it
  // thought its GetBoundsForClientView() were broke combobox drop down
  // placement.
  return GetWindowBoundsInScreen();
}

gfx::Rect DesktopWindowTreeHostX11::GetRestoredBounds() const {
  // We can't reliably track the restored bounds of a window, but we can get
  // the 90% case down. When *chrome* is the process that requests maximizing
  // or restoring bounds, we can record the current bounds before we request
  // maximization, and clear it when we detect a state change.
  if (!restored_bounds_in_pixels_.IsEmpty())
    return ToDIPRect(restored_bounds_in_pixels_);

  return GetWindowBoundsInScreen();
}

std::string DesktopWindowTreeHostX11::GetWorkspace() const {
  return workspace_ ? base::IntToString(workspace_.value()) : std::string();
}

void DesktopWindowTreeHostX11::UpdateWorkspace() {
  int workspace;
  if (ui::GetWindowDesktop(xwindow_, &workspace))
    workspace_ = workspace;
  else
    workspace_ = base::nullopt;
}

gfx::Rect DesktopWindowTreeHostX11::GetWorkAreaBoundsInScreen() const {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(const_cast<aura::Window*>(window()))
      .work_area();
}

void DesktopWindowTreeHostX11::SetShape(
    std::unique_ptr<Widget::ShapeRects> native_shape) {
  custom_window_shape_ = false;
  window_shape_.reset();

  if (native_shape) {
    SkRegion native_region;
    for (const gfx::Rect& rect : *native_shape)
      native_region.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
    gfx::Transform transform = GetRootTransform();
    if (!transform.IsIdentity() && !native_region.isEmpty()) {
      SkPath path_in_dip;
      if (native_region.getBoundaryPath(&path_in_dip)) {
        SkPath path_in_pixels;
        path_in_dip.transform(transform.matrix(), &path_in_pixels);
        window_shape_.reset(gfx::CreateRegionFromSkPath(path_in_pixels));
      } else {
        window_shape_.reset(XCreateRegion());
      }
    } else {
      window_shape_.reset(gfx::CreateRegionFromSkRegion(native_region));
    }

    custom_window_shape_ = true;
  }
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::Activate() {
  if (!IsVisible() || !activatable_)
    return;

  BeforeActivationStateChanged();

  ignore_keyboard_input_ = false;

  // wmii says that it supports _NET_ACTIVE_WINDOW but does not.
  // https://code.google.com/p/wmii/issues/detail?id=266
  static bool wm_supports_active_window =
      ui::GuessWindowManager() != ui::WM_WMII &&
      ui::WmSupportsHint(gfx::GetAtom("_NET_ACTIVE_WINDOW"));

  Time timestamp = ui::X11EventSource::GetInstance()->GetTimestamp();

  if (wm_supports_active_window) {
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
    auto old_error_handler = XSetErrorHandler(IgnoreX11Errors);
    XSetInputFocus(xdisplay_, xwindow_, RevertToParent, timestamp);
    // At this point, we know we will receive focus, and some
    // webdriver tests depend on a window being IsActive() immediately
    // after an Activate(), so just set this state now.
    has_pointer_focus_ = false;
    has_window_focus_ = true;
    // window_mapped_in_client_ == true based on the IsVisible() check above.
    window_mapped_in_server_ = true;
    XSetErrorHandler(old_error_handler);
  }
  AfterActivationStateChanged();
}

void DesktopWindowTreeHostX11::Deactivate() {
  BeforeActivationStateChanged();

  // Ignore future input events.
  ignore_keyboard_input_ = true;

  ReleaseCapture();
  XLowerWindow(xdisplay_, xwindow_);

  AfterActivationStateChanged();
}

bool DesktopWindowTreeHostX11::IsActive() const {
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

void DesktopWindowTreeHostX11::Maximize() {
  if (ui::HasWMSpecProperty(window_properties_,
                            gfx::GetAtom("_NET_WM_STATE_FULLSCREEN"))) {
    // Unfullscreen the window if it is fullscreen.
    SetWMSpecState(false, gfx::GetAtom("_NET_WM_STATE_FULLSCREEN"), x11::None);

    // Resize the window so that it does not have the same size as a monitor.
    // (Otherwise, some window managers immediately put the window back in
    // fullscreen mode).
    gfx::Rect adjusted_bounds_in_pixels(bounds_in_pixels_.origin(),
                                        AdjustSize(bounds_in_pixels_.size()));
    if (adjusted_bounds_in_pixels != bounds_in_pixels_)
      SetBoundsInPixels(adjusted_bounds_in_pixels);
  }

  // Some WMs do not respect maximization hints on unmapped windows, so we
  // save this one for later too.
  should_maximize_after_map_ = !IsVisible();

  // When we are in the process of requesting to maximize a window, we can
  // accurately keep track of our restored bounds instead of relying on the
  // heuristics that are in the PropertyNotify and ConfigureNotify handlers.
  restored_bounds_in_pixels_ = bounds_in_pixels_;

  SetWMSpecState(true, gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
  if (IsMinimized())
    Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
}

void DesktopWindowTreeHostX11::Minimize() {
  ReleaseCapture();
  if (window_mapped_in_client_)
    XIconifyWindow(xdisplay_, xwindow_, 0);
  else
    SetWMSpecState(true, gfx::GetAtom("_NET_WM_STATE_HIDDEN"), x11::None);
}

void DesktopWindowTreeHostX11::Restore() {
  should_maximize_after_map_ = false;
  SetWMSpecState(false, gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
  Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
  SetWMSpecState(false, gfx::GetAtom("_NET_WM_STATE_HIDDEN"), x11::None);
}

bool DesktopWindowTreeHostX11::IsMaximized() const {
  return (ui::HasWMSpecProperty(window_properties_,
                                gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT")) &&
          ui::HasWMSpecProperty(window_properties_,
                                gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ")));
}

bool DesktopWindowTreeHostX11::IsMinimized() const {
  return ui::HasWMSpecProperty(window_properties_,
                               gfx::GetAtom("_NET_WM_STATE_HIDDEN"));
}

bool DesktopWindowTreeHostX11::HasCapture() const {
  return g_current_capture == this;
}

void DesktopWindowTreeHostX11::SetAlwaysOnTop(bool always_on_top) {
  is_always_on_top_ = always_on_top;
  SetWMSpecState(always_on_top, gfx::GetAtom("_NET_WM_STATE_ABOVE"), x11::None);
}

bool DesktopWindowTreeHostX11::IsAlwaysOnTop() const {
  return is_always_on_top_;
}

void DesktopWindowTreeHostX11::SetVisible(bool visible) {
  if (compositor())
    compositor()->SetVisible(visible);
  if (IsVisible() != visible)
    native_widget_delegate_->OnNativeWidgetVisibilityChanged(visible);
}

void DesktopWindowTreeHostX11::SetVisibleOnAllWorkspaces(bool always_visible) {
  SetWMSpecState(always_visible, gfx::GetAtom("_NET_WM_STATE_STICKY"),
                 x11::None);

  int new_desktop = 0;
  if (always_visible) {
    new_desktop = kAllDesktops;
  } else {
    if (!ui::GetCurrentDesktop(&new_desktop))
      return;
  }

  workspace_ = kAllDesktops;
  XEvent xevent;
  memset (&xevent, 0, sizeof (xevent));
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

bool DesktopWindowTreeHostX11::IsVisibleOnAllWorkspaces() const {
  // We don't need a check for _NET_WM_STATE_STICKY because that would specify
  // that the window remain in a fixed position even if the viewport scrolls.
  // This is different from the type of workspace that's associated with
  // _NET_WM_DESKTOP.
  return GetWorkspace() == base::IntToString(kAllDesktops);
}

bool DesktopWindowTreeHostX11::SetWindowTitle(const base::string16& title) {
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

void DesktopWindowTreeHostX11::ClearNativeFocus() {
  // This method is weird and misnamed. Instead of clearing the native focus,
  // it sets the focus to our content_window(), which will trigger a cascade
  // of focus changes into views.
  if (content_window() && aura::client::GetFocusClient(content_window()) &&
      content_window()->Contains(
          aura::client::GetFocusClient(content_window())->GetFocusedWindow())) {
    aura::client::GetFocusClient(content_window())
        ->FocusWindow(content_window());
  }
}

Widget::MoveLoopResult DesktopWindowTreeHostX11::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  wm::WindowMoveSource window_move_source =
      source == Widget::MOVE_LOOP_SOURCE_MOUSE ? wm::WINDOW_MOVE_SOURCE_MOUSE
                                               : wm::WINDOW_MOVE_SOURCE_TOUCH;
  if (x11_window_move_client_->RunMoveLoop(content_window(), drag_offset,
                                           window_move_source) ==
      wm::MOVE_SUCCESSFUL)
    return Widget::MOVE_LOOP_SUCCESSFUL;

  return Widget::MOVE_LOOP_CANCELED;
}

void DesktopWindowTreeHostX11::EndMoveLoop() {
  x11_window_move_client_->EndMoveLoop();
}

void DesktopWindowTreeHostX11::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  // Much like the previous NativeWidgetGtk, we don't have anything to do here.
}

NonClientFrameView* DesktopWindowTreeHostX11::CreateNonClientFrameView() {
  return ShouldUseNativeFrame()
             ? new NativeFrameView(native_widget_delegate_->AsWidget())
             : nullptr;
}

bool DesktopWindowTreeHostX11::ShouldUseNativeFrame() const {
  return use_native_frame_;
}

bool DesktopWindowTreeHostX11::ShouldWindowContentsBeTransparent() const {
  return use_argb_visual_;
}

void DesktopWindowTreeHostX11::FrameTypeChanged() {
  Widget::FrameType new_type =
      native_widget_delegate_->AsWidget()->frame_type();
  if (new_type == Widget::FRAME_TYPE_DEFAULT) {
    // The default is determined by Widget::InitParams::remove_standard_frame
    // and does not change.
    return;
  }
  // Avoid mutating |View::children_| while possibly iterating over them.
  // See View::PropagateNativeThemeChanged().
  // TODO(varkha, sadrul): Investigate removing this (and instead expecting the
  // NonClientView::UpdateFrame() to update the frame-view when theme changes,
  // like all other views).
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DesktopWindowTreeHostX11::DelayedChangeFrameType,
                            weak_factory_.GetWeakPtr(),
                            new_type));
}

void DesktopWindowTreeHostX11::SetFullscreen(bool fullscreen) {
  if (is_fullscreen_ == fullscreen)
    return;
  is_fullscreen_ = fullscreen;
  if (is_fullscreen_)
    delayed_resize_task_.Cancel();

  // Work around a bug where if we try to unfullscreen, metacity immediately
  // fullscreens us again. This is a little flickery and not necessary if
  // there's a gnome-panel, but it's not easy to detect whether there's a
  // panel or not.
  bool unmaximize_and_remaximize = !fullscreen && IsMaximized() &&
                                   ui::GuessWindowManager() == ui::WM_METACITY;

  if (unmaximize_and_remaximize)
    Restore();
  SetWMSpecState(fullscreen, gfx::GetAtom("_NET_WM_STATE_FULLSCREEN"),
                 x11::None);
  if (unmaximize_and_remaximize)
    Maximize();

  // Try to guess the size we will have after the switch to/from fullscreen:
  // - (may) avoid transient states
  // - works around Flash content which expects to have the size updated
  //   synchronously.
  // See https://crbug.com/361408
  if (fullscreen) {
    restored_bounds_in_pixels_ = bounds_in_pixels_;
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window());
    bounds_in_pixels_ = ToPixelRect(display.bounds());
  } else {
    bounds_in_pixels_ = restored_bounds_in_pixels_;
  }
  OnHostMovedInPixels(bounds_in_pixels_.origin());
  OnHostResizedInPixels(bounds_in_pixels_.size());

  if (ui::HasWMSpecProperty(window_properties_,
                            gfx::GetAtom("_NET_WM_STATE_FULLSCREEN")) ==
      fullscreen) {
    Relayout();
    ResetWindowRegion();
  }
  // Else: the widget will be relaid out either when the window bounds change or
  // when |xwindow_|'s fullscreen state changes.
}

bool DesktopWindowTreeHostX11::IsFullscreen() const {
  return is_fullscreen_;
}

void DesktopWindowTreeHostX11::SetOpacity(float opacity) {
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

void DesktopWindowTreeHostX11::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  XSizeHints size_hints;
  size_hints.flags = 0;
  long supplied_return;

  XGetWMNormalHints(xdisplay_, xwindow_, &size_hints, &supplied_return);
  size_hints.flags |= PAspect;
  size_hints.min_aspect.x = size_hints.max_aspect.x = aspect_ratio.width();
  size_hints.min_aspect.y = size_hints.max_aspect.y = aspect_ratio.height();
  XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);
}

void DesktopWindowTreeHostX11::SetWindowIcons(
    const gfx::ImageSkia& window_icon, const gfx::ImageSkia& app_icon) {
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

void DesktopWindowTreeHostX11::InitModalType(ui::ModalType modal_type) {
  switch (modal_type) {
    case ui::MODAL_TYPE_NONE:
      break;
    default:
      // TODO(erg): Figure out under what situations |modal_type| isn't
      // none. The comment in desktop_native_widget_aura.cc suggests that this
      // is rare.
      NOTIMPLEMENTED();
  }
}

void DesktopWindowTreeHostX11::FlashFrame(bool flash_frame) {
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

bool DesktopWindowTreeHostX11::IsAnimatingClosed() const {
  return false;
}

bool DesktopWindowTreeHostX11::IsTranslucentWindowOpacitySupported() const {
  // This function may be called before InitX11Window() (which
  // initializes |use_argb_visual_|), so we cannot simply return
  // |use_argb_visual_|.
  return ui::XVisualManager::GetInstance()->ArgbVisualAvailable();
}

void DesktopWindowTreeHostX11::SizeConstraintsChanged() {
  UpdateMinAndMaxSize();
}

bool DesktopWindowTreeHostX11::ShouldUpdateWindowTransparency() const {
  return true;
}

bool DesktopWindowTreeHostX11::ShouldUseDesktopNativeCursorManager() const {
  return true;
}

bool DesktopWindowTreeHostX11::ShouldCreateVisibilityController() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, aura::WindowTreeHost implementation:

gfx::Transform DesktopWindowTreeHostX11::GetRootTransform() const {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  if (IsVisible()) {
    aura::Window* win = const_cast<aura::Window*>(window());
    display = display::Screen::GetScreen()->GetDisplayNearestWindow(win);
  }

  float scale = display.device_scale_factor();
  gfx::Transform transform;
  transform.Scale(scale, scale);
  return transform;
}

ui::EventSource* DesktopWindowTreeHostX11::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget DesktopWindowTreeHostX11::GetAcceleratedWidget() {
  return xwindow_;
}

void DesktopWindowTreeHostX11::ShowImpl() {
  Show(ui::SHOW_STATE_NORMAL, gfx::Rect());
}

void DesktopWindowTreeHostX11::HideImpl() {
  if (window_mapped_in_client_) {
    XWithdrawWindow(xdisplay_, xwindow_, 0);
    window_mapped_in_client_ = false;
    native_widget_delegate_->OnNativeWidgetVisibilityChanged(false);
  }
}

gfx::Rect DesktopWindowTreeHostX11::GetBoundsInPixels() const {
  return bounds_in_pixels_;
}

void DesktopWindowTreeHostX11::SetBoundsInPixels(
    const gfx::Rect& requested_bounds_in_pixel,
    const viz::LocalSurfaceId& local_surface_id,
    base::TimeTicks local_surface_id_allocation_time) {
  // On desktop-x11, the callers of SetBoundsInPixels() shouldn't need to (or be
  // able to) allocate LocalSurfaceId for the compositor. Aura itself should
  // allocate the new ids as needed, instead.
  DCHECK(!local_surface_id.is_valid());

  gfx::Rect bounds_in_pixels(requested_bounds_in_pixel.origin(),
                             AdjustSize(requested_bounds_in_pixel.size()));
  bool origin_changed = bounds_in_pixels_.origin() != bounds_in_pixels.origin();
  bool size_changed = bounds_in_pixels_.size() != bounds_in_pixels.size();
  XWindowChanges changes = {0};
  unsigned value_mask = 0;


  if (size_changed) {
    // Only cancel the delayed resize task if we're already about to call
    // OnHostResized in this function.
    delayed_resize_task_.Cancel();

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

  if (origin_changed)
    native_widget_delegate_->AsWidget()->OnNativeWidgetMove();
  if (size_changed) {
    OnHostResizedInPixels(bounds_in_pixels.size(), local_surface_id,
                          local_surface_id_allocation_time);
    ResetWindowRegion();
  }
}

gfx::Point DesktopWindowTreeHostX11::GetLocationOnScreenInPixels() const {
  return bounds_in_pixels_.origin();
}

void DesktopWindowTreeHostX11::SetCapture() {
  if (HasCapture())
    return;

  // Grabbing the mouse is asynchronous. However, we synchronously start
  // forwarding all mouse events received by Chrome to the
  // aura::WindowEventDispatcher which has capture. This makes capture
  // synchronous for all intents and purposes if either:
  // - |g_current_capture|'s X window has capture.
  // OR
  // - The topmost window underneath the mouse is managed by Chrome.
  DesktopWindowTreeHostX11* old_capturer = g_current_capture;

  // Update |g_current_capture| prior to calling OnHostLostWindowCapture() to
  // avoid releasing pointer grab.
  g_current_capture = this;
  if (old_capturer)
    old_capturer->OnHostLostWindowCapture();

  // If the pointer is already in |xwindow_|, we will not get a crossing event
  // with a mode of NotifyGrab, so we must record the grab state manually.
  has_pointer_grab_ |= !ui::GrabPointer(xwindow_, true, x11::None);
}

void DesktopWindowTreeHostX11::ReleaseCapture() {
  if (g_current_capture == this) {
    // Release mouse grab asynchronously. A window managed by Chrome is likely
    // the topmost window underneath the mouse so the capture release being
    // asynchronous is likely inconsequential.
    g_current_capture = NULL;
    ui::UngrabPointer();
    has_pointer_grab_ = false;

    OnHostLostWindowCapture();
  }
}

bool DesktopWindowTreeHostX11::CaptureSystemKeyEventsImpl(
    base::Optional<base::flat_set<ui::DomCode>> dom_codes) {
  // Only one KeyboardHook should be active at a time, otherwise there will be
  // problems with event routing (i.e. which Hook takes precedence) and
  // destruction ordering.
  DCHECK(!keyboard_hook_);
  keyboard_hook_ = ui::KeyboardHook::Create(
      std::move(dom_codes), GetAcceleratedWidget(),
      base::BindRepeating(&DesktopWindowTreeHostX11::DispatchKeyEvent,
                          base::Unretained(this)));

  return keyboard_hook_ != nullptr;
}

void DesktopWindowTreeHostX11::ReleaseSystemKeyEventCapture() {
  keyboard_hook_.reset();
}

bool DesktopWindowTreeHostX11::IsKeyLocked(ui::DomCode dom_code) {
  return keyboard_hook_ && keyboard_hook_->IsKeyLocked(dom_code);
}

void DesktopWindowTreeHostX11::SetCursorNative(gfx::NativeCursor cursor) {
  XDefineCursor(xdisplay_, xwindow_, cursor.platform());
}

void DesktopWindowTreeHostX11::MoveCursorToScreenLocationInPixels(
    const gfx::Point& location_in_pixels) {
  XWarpPointer(xdisplay_, x11::None, x_root_window_, 0, 0, 0, 0,
               bounds_in_pixels_.x() + location_in_pixels.x(),
               bounds_in_pixels_.y() + location_in_pixels.y());
}

void DesktopWindowTreeHostX11::OnCursorVisibilityChangedNative(bool show) {
  // TODO(erg): Conditional on us enabling touch on desktop linux builds, do
  // the same tap-to-click disabling here that chromeos does.
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, display::DisplayObserver implementation:

void DesktopWindowTreeHostX11::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  aura::WindowTreeHost::OnDisplayMetricsChanged(display, changed_metrics);

  if ((changed_metrics & DISPLAY_METRIC_DEVICE_SCALE_FACTOR) &&
      display::Screen::GetScreen()->GetDisplayNearestWindow(window()).id() ==
          display.id()) {
    // When the scale factor changes, also pretend that a resize
    // occured so that the window layout will be refreshed and a
    // compositor redraw will be scheduled.  This is weird, but works.
    // TODO(thomasanderson): Figure out a more direct way of doing
    // this.
    RestartDelayedResizeTask();
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, private:

void DesktopWindowTreeHostX11::InitX11Window(
    const Widget::InitParams& params) {
  unsigned long attribute_mask = CWBackPixel | CWBitGravity;
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = x11::None;
  swa.bit_gravity = NorthWestGravity;

  // Set the background color on startup to make the initial flickering
  // happening between the XWindow is mapped and the first expose event
  // is completely handled less annoying. If possible, we use the content
  // window's background color, otherwise we fallback to white.
  int background_color;

  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui && content_window()) {
    ui::NativeTheme::ColorId target_color;
    switch (params.type) {
      case Widget::InitParams::TYPE_BUBBLE:
        target_color = ui::NativeTheme::kColorId_BubbleBackground;
        break;
      case Widget::InitParams::TYPE_TOOLTIP:
        target_color = ui::NativeTheme::kColorId_TooltipBackground;
        break;
      default:
        target_color = ui::NativeTheme::kColorId_WindowBackground;
        break;
    }

    ui::NativeTheme* theme = linux_ui->GetNativeTheme(content_window());
    background_color = theme->GetSystemColor(target_color);
  } else {
    background_color = WhitePixel(xdisplay_, DefaultScreen(xdisplay_));
  }
  swa.background_pixel = background_color;

  XAtom window_type;
  switch (params.type) {
    case Widget::InitParams::TYPE_MENU:
      swa.override_redirect = x11::True;
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_MENU");
      break;
    case Widget::InitParams::TYPE_TOOLTIP:
      swa.override_redirect = x11::True;
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_TOOLTIP");
      break;
    case Widget::InitParams::TYPE_POPUP:
      swa.override_redirect = x11::True;
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_NOTIFICATION");
      break;
    case Widget::InitParams::TYPE_DRAG:
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

  if (swa.override_redirect)
    attribute_mask |= CWOverrideRedirect;

  bool enable_transparent_visuals;
  switch (params.opacity) {
    case Widget::InitParams::OPAQUE_WINDOW:
      enable_transparent_visuals = false;
      break;
    case Widget::InitParams::TRANSLUCENT_WINDOW:
      enable_transparent_visuals = true;
      break;
    case Widget::InitParams::INFER_OPACITY:
    default:
      enable_transparent_visuals = params.type == Widget::InitParams::TYPE_DRAG;
  }

  Visual* visual = CopyFromParent;
  int depth = CopyFromParent;
  Colormap colormap = CopyFromParent;
  ui::XVisualManager::GetInstance()->ChooseVisualForWindow(
      enable_transparent_visuals, &visual, &depth, &colormap,
      &use_argb_visual_);

  if (colormap != CopyFromParent) {
    attribute_mask |= CWColormap;
    swa.colormap = colormap;
  }

  // x.org will BadMatch if we don't set a border when the depth isn't the
  // same as the parent depth.
  attribute_mask |= CWBorderPixel;
  swa.border_pixel = 0;

  bounds_in_pixels_ = ToPixelRect(params.bounds);
  bounds_in_pixels_.set_size(AdjustSize(bounds_in_pixels_.size()));
  xwindow_ = XCreateWindow(xdisplay_, x_root_window_, bounds_in_pixels_.x(),
                           bounds_in_pixels_.y(), bounds_in_pixels_.width(),
                           bounds_in_pixels_.height(),
                           0,  // border width
                           depth, InputOutput, visual, attribute_mask, &swa);
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  open_windows().push_front(xwindow_);

  // TODO(erg): Maybe need to set a ViewProp here like in RWHL::RWHL().

  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask |
                    EnterWindowMask | LeaveWindowMask |
                    ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  xwindow_events_.reset(new ui::XScopedEventSelector(xwindow_, event_mask));
  XFlush(xdisplay_);

  if (ui::IsXInput2Available())
    ui::TouchFactory::GetInstance()->SetupXI2ForXWindow(xwindow_);

  // TODO(erg): We currently only request window deletion events. We also
  // should listen for activation events and anything else that GTK+ listens
  // for, and do something useful.
  XAtom protocols[2];
  protocols[0] = gfx::GetAtom("WM_DELETE_WINDOW");
  protocols[1] = gfx::GetAtom("_NET_WM_PING");
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

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
  if ((params.type == Widget::InitParams::TYPE_POPUP ||
       params.type == Widget::InitParams::TYPE_BUBBLE) &&
      !params.force_show_in_taskbar) {
    window_properties_.insert(gfx::GetAtom("_NET_WM_STATE_SKIP_TASKBAR"));
  }

  // If the window should stay on top of other windows, add the
  // _NET_WM_STATE_ABOVE property.
  is_always_on_top_ = params.keep_on_top;
  if (is_always_on_top_)
    window_properties_.insert(gfx::GetAtom("_NET_WM_STATE_ABOVE"));

  workspace_ = base::nullopt;
  if (params.visible_on_all_workspaces) {
    window_properties_.insert(gfx::GetAtom("_NET_WM_STATE_STICKY"));
    ui::SetIntProperty(xwindow_, "_NET_WM_DESKTOP", "CARDINAL", kAllDesktops);
  } else if (!params.workspace.empty()) {
    int workspace;
    if (base::StringToInt(params.workspace, &workspace))
      ui::SetIntProperty(xwindow_, "_NET_WM_DESKTOP", "CARDINAL", workspace);
  }

  if (!params.wm_class_name.empty() || !params.wm_class_class.empty()) {
    ui::SetWindowClassHint(
        xdisplay_, xwindow_, params.wm_class_name, params.wm_class_class);
  }

  const char* wm_role_name = NULL;
  // If the widget isn't overriding the role, provide a default value for popup
  // and bubble types.
  if (!params.wm_role_name.empty()) {
    wm_role_name = params.wm_role_name.c_str();
  } else {
    switch (params.type) {
      case Widget::InitParams::TYPE_POPUP:
        wm_role_name = kX11WindowRolePopup;
        break;
      case Widget::InitParams::TYPE_BUBBLE:
        wm_role_name = kX11WindowRoleBubble;
        break;
      default:
        break;
    }
  }
  if (wm_role_name)
    ui::SetWindowRole(xdisplay_, xwindow_, std::string(wm_role_name));

  if (params.remove_standard_frame) {
    // Setting _GTK_HIDE_TITLEBAR_WHEN_MAXIMIZED tells gnome-shell to not force
    // fullscreen on the window when it matches the desktop size.
    ui::SetHideTitlebarWhenMaximizedProperty(xwindow_,
                                             ui::HIDE_TITLEBAR_WHEN_MAXIMIZED);
  }

  if (views::LinuxUI::instance() &&
      views::LinuxUI::instance()->PreferDarkTheme()) {
    const unsigned char kDarkGtkThemeVariant[] = "dark";
    XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_GTK_THEME_VARIANT"),
                    gfx::GetAtom("UTF8_STRING"), 8, PropModeReplace,
                    kDarkGtkThemeVariant, arraysize(kDarkGtkThemeVariant) - 1);
  }

  // Always composite Chromium windows if a compositing WM is used.  Sometimes,
  // WMs will not composite fullscreen windows as an optimization, but this can
  // lead to tearing of fullscreen videos.
  ui::SetIntProperty(xwindow_, "_NET_WM_BYPASS_COMPOSITOR", "CARDINAL", 2);

  // If we have a parent, record the parent/child relationship. We use this
  // data during destruction to make sure that when we try to close a parent
  // window, we also destroy all child windows.
  if (params.parent && params.parent->GetHost()) {
    XID parent_xid =
        params.parent->GetHost()->GetAcceleratedWidget();
    window_parent_ = GetHostForXID(parent_xid);
    DCHECK(window_parent_);
    window_parent_->window_children_.insert(this);
  }

  // If we have a delegate which is providing a default window icon, use that
  // icon.
  gfx::ImageSkia* window_icon =
      ViewsDelegate::GetInstance()
          ? ViewsDelegate::GetInstance()->GetDefaultWindowIcon()
          : NULL;
  if (window_icon) {
    SetWindowIcons(gfx::ImageSkia(), *window_icon);
  }
  // Disable compositing on tooltips as a workaround for
  // https://crbug.com/442111.
  CreateCompositor(viz::FrameSinkId(),
                   params.type == Widget::InitParams::TYPE_TOOLTIP);
  OnAcceleratedWidgetAvailable();
}

gfx::Size DesktopWindowTreeHostX11::AdjustSize(
    const gfx::Size& requested_size_in_pixels) {
  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  // Compare against all monitor sizes. The window manager can move the window
  // to whichever monitor it wants.
  for (size_t i = 0; i < displays.size(); ++i) {
    if (requested_size_in_pixels == displays[i].GetSizeInPixel()) {
      return gfx::Size(requested_size_in_pixels.width() - 1,
                       requested_size_in_pixels.height() - 1);
    }
  }

  // Do not request a 0x0 window size. It causes an XError.
  gfx::Size size_in_pixels = requested_size_in_pixels;
  size_in_pixels.SetToMax(gfx::Size(1, 1));
  return size_in_pixels;
}

void DesktopWindowTreeHostX11::SetWMSpecState(bool enabled,
                                              XAtom state1,
                                              XAtom state2) {
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

void DesktopWindowTreeHostX11::OnWMStateUpdated() {
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

void DesktopWindowTreeHostX11::UpdateWindowProperties(
    const base::flat_set<XAtom>& new_window_properties) {
  bool was_minimized = IsMinimized();

  window_properties_ = new_window_properties;

  bool is_minimized = IsMinimized();

  // Propagate the window minimization information to the content window, so
  // the render side can update its visibility properly. OnWMStateUpdated() is
  // called by PropertyNofify event from DispatchEvent() when the browser is
  // minimized or shown from minimized state. On Windows, this is realized by
  // calling OnHostResizedInPixels() with an empty size. In particular,
  // HWNDMessageHandler::GetClientAreaBounds() returns an empty size when the
  // window is minimized. On Linux, returning empty size in GetBounds() or
  // SetBoundsInPixels() does not work.
  // We also propagate the minimization to the compositor, to makes sure that we
  // don't draw any 'blank' frames that could be noticed in applications such as
  // window manager previews, which show content even when a window is
  // minimized.
  if (is_minimized != was_minimized) {
    if (is_minimized) {
      SetVisible(false);
      content_window()->Hide();
    } else {
      content_window()->Show();
      SetVisible(true);
    }
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

  // Ignore requests by the window manager to enter or exit fullscreen (e.g. as
  // a result of pressing a window manager accelerator key). Chrome does not
  // handle window manager initiated fullscreen. In particular, Chrome needs to
  // do preprocessing before the x window's fullscreen state is toggled.

  is_always_on_top_ = ui::HasWMSpecProperty(
      window_properties_, gfx::GetAtom("_NET_WM_STATE_ABOVE"));

  // Now that we have different window properties, we may need to relayout the
  // window. (The windows code doesn't need this because their window change is
  // synchronous.)
  Relayout();
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::OnFrameExtentsUpdated() {
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

void DesktopWindowTreeHostX11::UpdateMinAndMaxSize() {
  gfx::Size minimum_in_pixels =
      ToPixelRect(gfx::Rect(native_widget_delegate_->GetMinimumSize())).size();
  gfx::Size maximum_in_pixels =
      ToPixelRect(gfx::Rect(native_widget_delegate_->GetMaximumSize())).size();
  if (min_size_in_pixels_ == minimum_in_pixels &&
      max_size_in_pixels_ == maximum_in_pixels)
    return;

  min_size_in_pixels_ = minimum_in_pixels;
  max_size_in_pixels_ = maximum_in_pixels;

  XSizeHints hints;
  hints.flags = 0;
  long supplied_return;
  XGetWMNormalHints(xdisplay_, xwindow_, &hints, &supplied_return);

  if (minimum_in_pixels.IsEmpty()) {
    hints.flags &= ~PMinSize;
  } else {
    hints.flags |= PMinSize;
    hints.min_width = min_size_in_pixels_.width();
    hints.min_height = min_size_in_pixels_.height();
  }

  if (maximum_in_pixels.IsEmpty()) {
    hints.flags &= ~PMaxSize;
  } else {
    hints.flags |= PMaxSize;
    hints.max_width = max_size_in_pixels_.width();
    hints.max_height = max_size_in_pixels_.height();
  }

  XSetWMNormalHints(xdisplay_, xwindow_, &hints);
}

void DesktopWindowTreeHostX11::UpdateWMUserTime(
    const ui::PlatformEvent& event) {
  if (!IsActive())
    return;

  ui::EventType type = ui::EventTypeFromNative(event);
  if (type == ui::ET_MOUSE_PRESSED ||
      type == ui::ET_KEY_PRESSED ||
      type == ui::ET_TOUCH_PRESSED) {
    unsigned long wm_user_time_ms = static_cast<unsigned long>(
        (ui::EventTimeFromNative(event) - base::TimeTicks()).InMilliseconds());
    XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_USER_TIME"),
                    XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&wm_user_time_ms),
                    1);
  }
}

void DesktopWindowTreeHostX11::SetUseNativeFrame(bool use_native_frame) {
  use_native_frame_ = use_native_frame;
  ui::SetUseOSWindowFrame(xwindow_, use_native_frame);
  ResetWindowRegion();
}

void DesktopWindowTreeHostX11::DispatchMouseEvent(ui::MouseEvent* event) {
  // In Windows, the native events sent to chrome are separated into client
  // and non-client versions of events, which we record on our LocatedEvent
  // structures. On X11, we emulate the concept of non-client. Before we pass
  // this event to the cross platform event handling framework, we need to
  // make sure it is appropriately marked as non-client if it's in the non
  // client area, or otherwise, we can get into a state where the a window is
  // set as the |mouse_pressed_handler_| in window_event_dispatcher.cc
  // despite the mouse button being released.
  //
  // We can't do this later in the dispatch process because we share that
  // with ash, and ash gets confused about event IS_NON_CLIENT-ness on
  // events, since ash doesn't expect this bit to be set, because it's never
  // been set before. (This works on ash on Windows because none of the mouse
  // events on the ash desktop are clicking in what Windows considers to be a
  // non client area.) Likewise, we won't want to do the following in any
  // WindowTreeHost that hosts ash.
  if (content_window() && content_window()->delegate()) {
    int flags = event->flags();
    int hit_test_code =
        content_window()->delegate()->GetNonClientComponent(event->location());
    if (hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE)
      flags |= ui::EF_IS_NON_CLIENT;
    event->set_flags(flags);
  }

  // While we unset the urgency hint when we gain focus, we also must remove it
  // on mouse clicks because we can call FlashFrame() on an active window.
  if (event->IsAnyButton() || event->IsMouseWheelEvent())
    FlashFrame(false);

  if (!g_current_capture || g_current_capture == this) {
    SendEventToSink(event);
  } else {
    // Another DesktopWindowTreeHostX11 has installed itself as
    // capture. Translate the event's location and dispatch to the other.
    DCHECK_EQ(ui::GetScaleFactorForNativeView(window()),
              ui::GetScaleFactorForNativeView(g_current_capture->window()));
    ConvertEventLocationToTargetWindowLocation(
        g_current_capture->GetLocationOnScreenInPixels(),
        GetLocationOnScreenInPixels(), event->AsLocatedEvent());
    g_current_capture->SendEventToSink(event);
  }
}

void DesktopWindowTreeHostX11::DispatchTouchEvent(ui::TouchEvent* event) {
  if (g_current_capture && g_current_capture != this &&
      event->type() == ui::ET_TOUCH_PRESSED) {
    DCHECK_EQ(ui::GetScaleFactorForNativeView(window()),
              ui::GetScaleFactorForNativeView(g_current_capture->window()));
    ConvertEventLocationToTargetWindowLocation(
        g_current_capture->GetLocationOnScreenInPixels(),
        GetLocationOnScreenInPixels(), event->AsLocatedEvent());
    g_current_capture->SendEventToSink(event);
  } else {
    SendEventToSink(event);
  }
}

void DesktopWindowTreeHostX11::DispatchKeyEvent(ui::KeyEvent* event) {
  if (native_widget_delegate_->AsWidget()->IsActive())
    SendEventToSink(event);
}

void DesktopWindowTreeHostX11::ResetWindowRegion() {
  // If a custom window shape was supplied then apply it.
  if (custom_window_shape_) {
    XShapeCombineRegion(xdisplay_, xwindow_, ShapeBounding, 0, 0,
                        window_shape_.get(), false);
    return;
  }

  window_shape_.reset();

  if (!IsMaximized() && !IsFullscreen()) {
    gfx::Path window_mask;
    Widget* widget = native_widget_delegate_->AsWidget();
    if (widget->non_client_view()) {
      // Some frame views define a custom (non-rectangular) window mask. If
      // so, use it to define the window shape. If not, fall through.
      widget->non_client_view()->GetWindowMask(bounds_in_pixels_.size(),
                                               &window_mask);
      if (window_mask.countPoints() > 0) {
        window_shape_.reset(gfx::CreateRegionFromSkPath(window_mask));
        XShapeCombineRegion(xdisplay_, xwindow_, ShapeBounding, 0, 0,
                            window_shape_.get(), false);
        return;
      }
    }
  }

  // If we didn't set the shape for any reason, reset the shaping information.
  // How this is done depends on the border style, due to quirks and bugs in
  // various window managers.
  if (ShouldUseNativeFrame()) {
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
    XRectangle r = {0,
                    0,
                    static_cast<unsigned short>(bounds_in_pixels_.width()),
                    static_cast<unsigned short>(bounds_in_pixels_.height())};
    XShapeCombineRectangles(
        xdisplay_, xwindow_, ShapeBounding, 0, 0, &r, 1, ShapeSet, YXBanded);
  }
}

void DesktopWindowTreeHostX11::SerializeImageRepresentation(
    const gfx::ImageSkiaRep& rep,
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

std::list<XID>& DesktopWindowTreeHostX11::open_windows() {
  if (!open_windows_)
    open_windows_ = new std::list<XID>();
  return *open_windows_;
}

void DesktopWindowTreeHostX11::MapWindow(ui::WindowShowState show_state) {
  if (show_state != ui::SHOW_STATE_DEFAULT &&
      show_state != ui::SHOW_STATE_NORMAL &&
      show_state != ui::SHOW_STATE_INACTIVE &&
      show_state != ui::SHOW_STATE_MAXIMIZED) {
    // It will behave like SHOW_STATE_NORMAL.
    NOTIMPLEMENTED_LOG_ONCE();
  }

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

  // If SHOW_STATE_INACTIVE, tell the window manager not to focus the window
  // when mapping. This is done by setting the _NET_WM_USER_TIME to 0. See e.g.
  // http://standards.freedesktop.org/wm-spec/latest/ar01s05.html
  ignore_keyboard_input_ = show_state == ui::SHOW_STATE_INACTIVE;
  unsigned long wm_user_time_ms =
      ignore_keyboard_input_
          ? 0
          : ui::X11EventSource::GetInstance()->GetTimestamp();
  if (show_state == ui::SHOW_STATE_INACTIVE || wm_user_time_ms != 0) {
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
}

void DesktopWindowTreeHostX11::SetWindowTransparency() {
  compositor()->SetBackgroundColor(use_argb_visual_ ? SK_ColorTRANSPARENT
                                                    : SK_ColorWHITE);
  window()->SetTransparent(use_argb_visual_);
  content_window()->SetTransparent(use_argb_visual_);
}

void DesktopWindowTreeHostX11::Relayout() {
  Widget* widget = native_widget_delegate_->AsWidget();
  NonClientView* non_client_view = widget->non_client_view();
  // non_client_view may be NULL, especially during creation.
  if (non_client_view) {
    non_client_view->client_view()->InvalidateLayout();
    non_client_view->InvalidateLayout();
  }
  widget->GetRootView()->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostX11, ui::PlatformEventDispatcher implementation:

bool DesktopWindowTreeHostX11::CanDispatchEvent(
    const ui::PlatformEvent& event) {
  return event->xany.window == xwindow_ ||
         (event->type == GenericEvent &&
          static_cast<XIDeviceEvent*>(event->xcookie.data)->event == xwindow_);
}

uint32_t DesktopWindowTreeHostX11::DispatchEvent(
    const ui::PlatformEvent& event) {
  XEvent* xev = event;

  TRACE_EVENT1("views", "DesktopWindowTreeHostX11::Dispatch",
               "event->type", event->type);

  UpdateWMUserTime(event);

  // May want to factor CheckXEventForConsistency(xev); into a common location
  // since it is called here.
  switch (xev->type) {
    case EnterNotify:
    case LeaveNotify: {
      OnCrossingEvent(xev->type == EnterNotify, xev->xcrossing.focus,
                      xev->xcrossing.mode, xev->xcrossing.detail);

      // Ignore EventNotify and LeaveNotify events from children of |xwindow_|.
      // NativeViewGLSurfaceGLX adds a child to |xwindow_|.
      if (xev->xcrossing.detail != NotifyInferior) {
        ui::MouseEvent mouse_event(xev);
        DispatchMouseEvent(&mouse_event);
      }
      break;
    }
    case Expose: {
      gfx::Rect damage_rect_in_pixels(xev->xexpose.x, xev->xexpose.y,
                                      xev->xexpose.width, xev->xexpose.height);
      compositor()->ScheduleRedrawRect(damage_rect_in_pixels);
      break;
    }
    case KeyPress: {
      ui::KeyEvent keydown_event(xev);
      DispatchKeyEvent(&keydown_event);
      break;
    }
    case KeyRelease: {
      // There is no way to deactivate a window in X11 so ignore input if
      // window is supposed to be 'inactive'.
      if (!IsActive() && !HasCapture())
        break;

      ui::KeyEvent key_event(xev);
      DispatchKeyEvent(&key_event);
      break;
    }
    case ButtonPress:
    case ButtonRelease: {
      ui::EventType event_type = ui::EventTypeFromNative(xev);
      switch (event_type) {
        case ui::ET_MOUSEWHEEL: {
          ui::MouseWheelEvent mouseev(xev);
          DispatchMouseEvent(&mouseev);
          break;
        }
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSE_RELEASED: {
          ui::MouseEvent mouseev(xev);
          DispatchMouseEvent(&mouseev);
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
    case x11::FocusIn:
    case x11::FocusOut:
      OnFocusEvent(xev->type == x11::FocusIn, event->xfocus.mode,
                   event->xfocus.detail);
      break;
    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
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
      bool origin_changed =
          bounds_in_pixels_.origin() != bounds_in_pixels.origin();
      previous_bounds_in_pixels_ = bounds_in_pixels_;
      bounds_in_pixels_ = bounds_in_pixels;

      if (origin_changed)
        OnHostMovedInPixels(bounds_in_pixels_.origin());

      if (size_changed)
        RestartDelayedResizeTask();
      break;
    }
    case GenericEvent: {
      ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
      if (!factory->ShouldProcessXI2Event(xev))
        break;

      XIEnterEvent* enter_event = static_cast<XIEnterEvent*>(xev->xcookie.data);
      switch (static_cast<XIEvent*>(xev->xcookie.data)->evtype) {
        case XI_Enter:
        case XI_Leave:
          OnCrossingEvent(enter_event->evtype == XI_Enter, enter_event->focus,
                          XI2ModeToXMode(enter_event->mode),
                          enter_event->detail);
          return ui::POST_DISPATCH_STOP_PROPAGATION;
        case XI_FocusIn:
        case XI_FocusOut:
          OnFocusEvent(enter_event->evtype == XI_FocusIn,
                       XI2ModeToXMode(enter_event->mode), enter_event->detail);
          return ui::POST_DISPATCH_STOP_PROPAGATION;
        default:
          break;
      }

      ui::EventType type = ui::EventTypeFromNative(xev);
      XEvent last_event;
      int num_coalesced = 0;

      switch (type) {
        case ui::ET_TOUCH_MOVED:
          num_coalesced = ui::CoalescePendingMotionEvents(xev, &last_event);
          if (num_coalesced > 0)
            xev = &last_event;
          FALLTHROUGH;
        case ui::ET_TOUCH_PRESSED:
        case ui::ET_TOUCH_RELEASED: {
          ui::TouchEvent touchev(xev);
          DispatchTouchEvent(&touchev);
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
            DispatchMouseEvent(&mouseev);
          break;
        }
        case ui::ET_MOUSEWHEEL: {
          ui::MouseWheelEvent mouseev(xev);
          DispatchMouseEvent(&mouseev);
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
            SendEventToSink(&scrollev);
          break;
        }
        case ui::ET_KEY_PRESSED:
        case ui::ET_KEY_RELEASED: {
          ui::KeyEvent key_event(xev);
          DispatchKeyEvent(&key_event);
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
      break;
    }
    case MapNotify: {
      window_mapped_in_server_ = true;

      for (DesktopWindowTreeHostObserverX11& observer : observer_list_)
        observer.OnWindowMapped(xwindow_);

      // Some WMs only respect maximize hints after the window has been mapped.
      // Check whether we need to re-do a maximization.
      if (should_maximize_after_map_) {
        Maximize();
        should_maximize_after_map_ = false;
      }

      break;
    }
    case UnmapNotify: {
      window_mapped_in_server_ = false;
      has_pointer_ = false;
      has_pointer_grab_ = false;
      has_pointer_focus_ = false;
      has_window_focus_ = false;
      for (DesktopWindowTreeHostObserverX11& observer : observer_list_)
        observer.OnWindowUnmapped(xwindow_);
      break;
    }
    case ClientMessage: {
      Atom message_type = xev->xclient.message_type;
      if (message_type == gfx::GetAtom("WM_PROTOCOLS")) {
        Atom protocol = static_cast<Atom>(xev->xclient.data.l[0]);
        if (protocol == gfx::GetAtom("WM_DELETE_WINDOW")) {
          // We have received a close message from the window manager.
          OnHostCloseRequested();
        } else if (protocol == gfx::GetAtom("_NET_WM_PING")) {
          XEvent reply_event = *xev;
          reply_event.xclient.window = x_root_window_;

          XSendEvent(xdisplay_, reply_event.xclient.window, x11::False,
                     SubstructureRedirectMask | SubstructureNotifyMask,
                     &reply_event);
        }
      } else if (message_type == gfx::GetAtom("XdndEnter")) {
        drag_drop_client_->OnXdndEnter(xev->xclient);
      } else if (message_type == gfx::GetAtom("XdndLeave")) {
        drag_drop_client_->OnXdndLeave(xev->xclient);
      } else if (message_type == gfx::GetAtom("XdndPosition")) {
        drag_drop_client_->OnXdndPosition(xev->xclient);
      } else if (message_type == gfx::GetAtom("XdndStatus")) {
        drag_drop_client_->OnXdndStatus(xev->xclient);
      } else if (message_type == gfx::GetAtom("XdndFinished")) {
        drag_drop_client_->OnXdndFinished(xev->xclient);
      } else if (message_type == gfx::GetAtom("XdndDrop")) {
        drag_drop_client_->OnXdndDrop(xev->xclient);
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
      DispatchMouseEvent(&mouseev);
      break;
    }
    case PropertyNotify: {
      XAtom changed_atom = xev->xproperty.atom;
      if (changed_atom == gfx::GetAtom("_NET_WM_STATE")) {
        OnWMStateUpdated();
      } else if (changed_atom == gfx::GetAtom("_NET_FRAME_EXTENTS")) {
        OnFrameExtentsUpdated();
      } else if (changed_atom == gfx::GetAtom("_NET_WM_DESKTOP")) {
        base::Optional<int> old_workspace = workspace_;
        UpdateWorkspace();
        if (workspace_ != old_workspace)
          OnHostWorkspaceChanged();
      }
      break;
    }
    case SelectionNotify: {
      drag_drop_client_->OnSelectionNotify(xev->xselection);
      break;
    }
  }
  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

void DesktopWindowTreeHostX11::DelayedResize(const gfx::Size& size_in_pixels) {
  OnHostResizedInPixels(size_in_pixels);
  ResetWindowRegion();
  delayed_resize_task_.Cancel();
}

void DesktopWindowTreeHostX11::DelayedChangeFrameType(Widget::FrameType type) {
  SetUseNativeFrame(type == Widget::FRAME_TYPE_FORCE_NATIVE);
  // Replace the frame and layout the contents. Even though we don't have a
  // swappable glass frame like on Windows, we still replace the frame because
  // the button assets don't update otherwise.
  native_widget_delegate_->AsWidget()->non_client_view()->UpdateFrame();
}

gfx::Rect DesktopWindowTreeHostX11::ToDIPRect(
    const gfx::Rect& rect_in_pixels) const {
  gfx::RectF rect_in_dip = gfx::RectF(rect_in_pixels);
  GetRootTransform().TransformRectReverse(&rect_in_dip);
  return gfx::ToEnclosingRect(rect_in_dip);
}

gfx::Rect DesktopWindowTreeHostX11::ToPixelRect(
    const gfx::Rect& rect_in_dip) const {
  gfx::RectF rect_in_pixels = gfx::RectF(rect_in_dip);
  GetRootTransform().TransformRect(&rect_in_pixels);
  return gfx::ToEnclosingRect(rect_in_pixels);
}

std::unique_ptr<base::OnceClosure>
DesktopWindowTreeHostX11::DisableEventListening() {
  // Allows to open multiple file-pickers. See https://crbug.com/678982
  modal_dialog_counter_++;
  if (modal_dialog_counter_ == 1) {
    // ScopedWindowTargeter is used to temporarily replace the event-targeter
    // with NullWindowEventTargeter to make |dialog| modal.
    targeter_for_modal_ = std::make_unique<aura::ScopedWindowTargeter>(
        window(), std::make_unique<aura::NullWindowTargeter>());
  }

  return std::make_unique<base::OnceClosure>(
      base::BindOnce(&DesktopWindowTreeHostX11::EnableEventListening,
                     weak_factory_.GetWeakPtr()));
}

void DesktopWindowTreeHostX11::EnableEventListening() {
  DCHECK_GT(modal_dialog_counter_, 0UL);
  if (!--modal_dialog_counter_)
    targeter_for_modal_.reset();
}

void DesktopWindowTreeHostX11::RestartDelayedResizeTask() {
  delayed_resize_task_.Reset(
      base::Bind(&DesktopWindowTreeHostX11::DelayedResize,
                 close_widget_factory_.GetWeakPtr(), bounds_in_pixels_.size()));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, delayed_resize_task_.callback());
}

aura::Window* DesktopWindowTreeHostX11::content_window() {
  return desktop_native_widget_aura_->content_window();
}

base::flat_map<std::string, std::string>
DesktopWindowTreeHostX11::GetKeyboardLayoutMap() {
  if (views::LinuxUI::instance())
    return views::LinuxUI::instance()->GetKeyboardLayoutMap();
  return {};
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHost, public:

// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostX11(native_widget_delegate,
                                      desktop_native_widget_aura);
}

}  // namespace views
