// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window.h"

#include "base/trace_event/trace_event.h"
#include "ui/base/buildflags.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/display/screen.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/platform_window/platform_window_delegate_linux.h"
#include "ui/platform_window/x11/x11_window_manager.h"

#if defined(USE_OZONE)
#include "ui/events/ozone/events_ozone.h"
#endif

#if BUILDFLAG(USE_ATK)
#include "ui/platform_window/x11/atk_event_conversion.h"
#endif

namespace ui {

namespace {

ui::XWindow::Configuration ConvertInitPropertiesToXWindowConfig(
    const PlatformWindowInitProperties& properties) {
  using WindowType = ui::XWindow::WindowType;
  using WindowOpacity = ui::XWindow::WindowOpacity;
  ui::XWindow::Configuration config;

  switch (properties.type) {
    case PlatformWindowType::kWindow:
      config.type = WindowType::kWindow;
      break;
    case PlatformWindowType::kMenu:
      config.type = WindowType::kMenu;
      break;
    case PlatformWindowType::kTooltip:
      config.type = WindowType::kTooltip;
      break;
    case PlatformWindowType::kPopup:
      config.type = WindowType::kPopup;
      break;
    case PlatformWindowType::kDrag:
      config.type = WindowType::kDrag;
      break;
    case PlatformWindowType::kBubble:
      config.type = WindowType::kBubble;
      break;
  }

  switch (properties.opacity) {
    case PlatformWindowOpacity::kInferOpacity:
      config.opacity = WindowOpacity::kInferOpacity;
      break;
    case PlatformWindowOpacity::kOpaqueWindow:
      config.opacity = WindowOpacity::kOpaqueWindow;
      break;
    case PlatformWindowOpacity::kTranslucentWindow:
      config.opacity = WindowOpacity::kTranslucentWindow;
      break;
  }

  config.bounds = properties.bounds;
  config.icon = properties.icon;
  config.force_show_in_taskbar = properties.force_show_in_taskbar;
  config.keep_on_top = properties.keep_on_top;
  config.visible_on_all_workspaces = properties.visible_on_all_workspaces;
  config.remove_standard_frame = properties.remove_standard_frame;
  config.workspace = properties.workspace;
  config.wm_class_name = properties.wm_class_name;
  config.wm_class_class = properties.wm_class_class;
  config.wm_role_name = properties.wm_role_name;
  config.activatable = properties.activatable;
  config.visual_id = properties.x_visual_id;
  config.prefer_dark_theme = properties.prefer_dark_theme;
  config.background_color = properties.background_color;
  return config;
}

}  // namespace

X11Window::X11Window(PlatformWindowDelegateLinux* platform_window_delegate)
    : platform_window_delegate_(platform_window_delegate) {
  // Set a class property key, which allows |this| to be used for interactive
  // events, e.g. move or resize.
  SetWmMoveResizeHandler(this, static_cast<WmMoveResizeHandler*>(this));
}

X11Window::~X11Window() {
  PrepareForShutdown();
  Close();
}

void X11Window::Initialize(PlatformWindowInitProperties properties) {
  XWindow::Configuration config =
      ConvertInitPropertiesToXWindowConfig(properties);

  gfx::Size adjusted_size_in_pixels =
      AdjustSizeForDisplay(config.bounds.size());
  config.bounds.set_size(adjusted_size_in_pixels);

  Init(config);
}

void X11Window::SetXEventDelegate(XEventDelegate* delegate) {
  DCHECK(!x_event_delegate_);
  x_event_delegate_ = delegate;
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
  if (mapped_in_client())
    return;

  XWindow::Map(inactive);
}

void X11Window::Hide() {
  XWindow::Hide();
}

void X11Window::Close() {
  if (is_shutting_down_)
    return;

  X11WindowManager::GetInstance()->RemoveWindow(this);

  is_shutting_down_ = true;
  XWindow::Close();
  platform_window_delegate_->OnClosed();
}

bool X11Window::IsVisible() const {
  return XWindow::IsXWindowVisible();
}

void X11Window::PrepareForShutdown() {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void X11Window::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect current_bounds_in_pixels = GetBounds();
  gfx::Rect bounds_in_pixels(bounds.origin(),
                             AdjustSizeForDisplay(bounds.size()));

  bool size_changed =
      current_bounds_in_pixels.size() != bounds_in_pixels.size();

  if (size_changed) {
    // Only cancel the delayed resize task if we're already about to call
    // OnHostResized in this function.
    XWindow::CancelResize();
  }

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_| later.
  XWindow::SetBounds(bounds_in_pixels);

  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  platform_window_delegate_->OnBoundsChanged(bounds_in_pixels);
}

gfx::Rect X11Window::GetBounds() {
  return XWindow::bounds();
}

void X11Window::SetTitle(const base::string16& title) {
  XWindow::SetTitle(title);
}

void X11Window::SetCapture() {
  if (HasCapture())
    return;
  X11WindowManager::GetInstance()->GrabEvents(this);
  GrabPointer();
}

void X11Window::ReleaseCapture() {
  if (!HasCapture())
    return;
  ReleasePointerGrab();
  X11WindowManager::GetInstance()->UngrabEvents(this);
}

bool X11Window::HasCapture() const {
  return X11WindowManager::GetInstance()->located_events_grabber() == this;
}

void X11Window::ToggleFullscreen() {
  // Check if we need to fullscreen the window or not.
  bool fullscreen = state_ != PlatformWindowState::kFullScreen;
  if (fullscreen)
    CancelResize();

  // Work around a bug where if we try to unfullscreen, metacity immediately
  // fullscreens us again. This is a little flickery and not necessary if
  // there's a gnome-panel, but it's not easy to detect whether there's a
  // panel or not.
  bool unmaximize_and_remaximize = !fullscreen && IsMaximized() &&
                                   ui::GuessWindowManager() == ui::WM_METACITY;

  if (unmaximize_and_remaximize)
    Restore();

  // Fullscreen state changes have to be handled manually and then checked
  // against configuration events, which come from a compositor. The reason
  // of manually changing the |state_| is that the compositor answers
  // about state changes asynchronously, which leads to a wrong return value in
  // DesktopWindowTreeHostPlatform::IsFullscreen, for example, and media
  // files can never be set to fullscreen. Wayland does the same.
  if (fullscreen)
    state_ = PlatformWindowState::kFullScreen;
  else
    state_ = PlatformWindowState::kUnknown;
  SetFullscreen(fullscreen);

  if (unmaximize_and_remaximize)
    Maximize();

  // Try to guess the size we will have after the switch to/from fullscreen:
  // - (may) avoid transient states
  // - works around Flash content which expects to have the size updated
  //   synchronously.
  // See https://crbug.com/361408
  gfx::Rect bounds_in_pixels = GetBounds();
  if (fullscreen) {
    display::Screen* screen = display::Screen::GetScreen();
    const display::Display display =
        screen->GetDisplayMatching(bounds_in_pixels);
    SetRestoredBoundsInPixels(bounds_in_pixels);
    bounds_in_pixels = display.bounds();
  } else {
    bounds_in_pixels = GetRestoredBoundsInPixels();
  }
  // Do not go through SetBounds as long as it adjusts bounds and sets them to X
  // Server. Instead, we just store the bounds and notify the client that the
  // window occupies the entire screen.
  XWindow::set_bounds(bounds_in_pixels);
  platform_window_delegate_->OnBoundsChanged(bounds_in_pixels);
}

void X11Window::Maximize() {
  if (IsFullscreen()) {
    // Unfullscreen the window if it is fullscreen.
    ToggleFullscreen();

    // Resize the window so that it does not have the same size as a monitor.
    // (Otherwise, some window managers immediately put the window back in
    // fullscreen mode).
    gfx::Rect bounds_in_pixels = GetBounds();
    gfx::Rect adjusted_bounds_in_pixels(
        bounds_in_pixels.origin(),
        AdjustSizeForDisplay(bounds_in_pixels.size()));
    if (adjusted_bounds_in_pixels != bounds_in_pixels)
      SetBounds(adjusted_bounds_in_pixels);
  }

  // When we are in the process of requesting to maximize a window, we can
  // accurately keep track of our restored bounds instead of relying on the
  // heuristics that are in the PropertyNotify and ConfigureNotify handlers.
  SetRestoredBoundsInPixels(GetBounds());

  XWindow::Maximize();
}

void X11Window::Minimize() {
  XWindow::Minimize();
}

void X11Window::Restore() {
  if (XWindow::IsFullscreen())
    ToggleFullscreen();
  if (XWindow::IsMaximized())
    XWindow::Unmaximize();
  XWindow::Unhide();
}

PlatformWindowState X11Window::GetPlatformWindowState() const {
  return state_;
}

void X11Window::Activate() {
  XWindow::Activate();
}

void X11Window::Deactivate() {
  XWindow::Deactivate();
}

bool X11Window::IsSyncExtensionAvailable() const {
  return ui::IsSyncExtensionAvailable();
}

void X11Window::OnCompleteSwapAfterResize() {
  XWindow::NotifySwapAfterResize();
}

void X11Window::SetUseNativeFrame(bool use_native_frame) {
  XWindow::SetUseNativeFrame(use_native_frame);
}

bool X11Window::ShouldUseNativeFrame() const {
  return XWindow::use_native_frame();
}

void X11Window::SetCursor(PlatformCursor cursor) {
  // X11PlatformWindowOzone has different type of PlatformCursor. Thus, use this
  // only for X11 and Ozone will manage this by itself.
#if defined(USE_X11)
  XWindow::SetCursor(cursor);
#endif
}

void X11Window::MoveCursorTo(const gfx::Point& location) {
  XWindow::MoveCursorTo(location);
}

void X11Window::ConfineCursorToBounds(const gfx::Rect& bounds) {
  XWindow::ConfineCursorTo(bounds);
}

void X11Window::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  restored_bounds_in_pixels_ = bounds;
}

gfx::Rect X11Window::GetRestoredBoundsInPixels() const {
  return restored_bounds_in_pixels_;
}

bool X11Window::ShouldWindowContentsBeTransparent() const {
  return XWindow::has_alpha();
}

void X11Window::SetZOrderLevel(ZOrderLevel order) {
  z_order_ = order;

  // Emulate the multiple window levels provided by other platforms by
  // collapsing the z-order enum into kNormal = normal, everything else = always
  // on top.
  XWindow::SetAlwaysOnTop(order != ui::ZOrderLevel::kNormal);
}

ZOrderLevel X11Window::GetZOrderLevel() const {
  bool window_always_on_top = is_always_on_top();
  bool level_always_on_top = z_order_ != ui::ZOrderLevel::kNormal;

  if (window_always_on_top == level_always_on_top)
    return z_order_;

  // If something external has forced a window to be always-on-top, map it to
  // kFloatingWindow as a reasonable equivalent.
  return window_always_on_top ? ui::ZOrderLevel::kFloatingWindow
                              : ui::ZOrderLevel::kNormal;
}

void X11Window::StackAbove(gfx::AcceleratedWidget widget) {
  // Check comment in the GetWidget method about this this cast.
  XWindow::StackXWindowAbove(static_cast<::Window>(widget));
}

void X11Window::StackAtTop() {
  XWindow::StackXWindowAtTop();
}

base::Optional<int> X11Window::GetWorkspace() const {
  return XWindow::workspace();
}

void X11Window::SetVisibleOnAllWorkspaces(bool always_visible) {
  XWindow::SetXWindowVisibleOnAllWorkspaces(always_visible);
}

bool X11Window::IsVisibleOnAllWorkspaces() const {
  return XWindow::IsXWindowVisibleOnAllWorkspaces();
}

void X11Window::FlashFrame(bool flash_frame) {
  XWindow::SetFlashFrameHint(flash_frame);
}

gfx::Rect X11Window::GetXRootWindowOuterBounds() const {
  return XWindow::GetOutterBounds();
}

bool X11Window::ContainsPointInXRegion(const gfx::Point& point) const {
  return XWindow::ContainsPointInRegion(point);
}

void X11Window::SetShape(std::unique_ptr<ShapeRects> native_shape,
                         const gfx::Transform& transform) {
  return XWindow::SetXWindowShape(std::move(native_shape), transform);
}

void X11Window::SetOpacityForXWindow(float opacity) {
  XWindow::SetXWindowOpacity(opacity);
}

void X11Window::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  XWindow::SetXWindowAspectRatio(aspect_ratio);
}

void X11Window::SetWindowIcons(const gfx::ImageSkia& window_icon,
                               const gfx::ImageSkia& app_icon) {
  XWindow::SetXWindowIcons(window_icon, app_icon);
}

void X11Window::SizeConstraintsChanged() {
  XWindow::UpdateMinAndMaxSize();
}

bool X11Window::IsTranslucentWindowOpacitySupported() const {
  // This function may be called before InitX11Window() (which
  // initializes |visual_has_alpha_|), so we cannot simply return
  // |visual_has_alpha_|.
  return ui::XVisualManager::GetInstance()->ArgbVisualAvailable();
}

void X11Window::LowerXWindow() {
  XWindow::LowerWindow();
}

bool X11Window::HandleAsAtkEvent(XEvent* xev) {
#if !BUILDFLAG(USE_ATK)
  // TODO(crbug.com/1014934): Support ATK in Ozone/X11.
  NOTREACHED();
  return false;
#else
  DCHECK(xev);
  if (xev->type != KeyPress && xev->type != KeyRelease)
    return false;
  auto atk_key_event = AtkKeyEventFromXEvent(xev);
  return platform_window_delegate_->OnAtkKeyEvent(atk_key_event.get());
#endif
}

bool X11Window::CanDispatchEvent(const PlatformEvent& xev) {
#if defined(USE_X11)
  return XWindow::IsTargetedBy(*xev);
#else
  NOTREACHED() << "Ozone must use own dispatcher as it has different type of "
                  "PlatformEvent";
  return false;
#endif
}

uint32_t X11Window::DispatchEvent(const PlatformEvent& event) {
#if defined(USE_X11)
  TRACE_EVENT1("views", "X11PlatformWindow::Dispatch", "event->type",
               event->type);

  if (!HandleAsAtkEvent(event))
    ProcessEvent(event);
  return POST_DISPATCH_STOP_PROPAGATION;
#else
  // Notify that the mouse gets physically entered to the window regardless of
  // the grab. This is used to update the mouse cursor state.
  auto* window_manager = X11WindowManager::GetInstance();
  DCHECK(window_manager);
  if (event->IsMouseEvent())
    window_manager->MouseOnWindow(this);

  OnXWindowEvent(event);
  return POST_DISPATCH_STOP_PROPAGATION;
#endif
}

void X11Window::OnXWindowCreated() {
  X11WindowManager::GetInstance()->AddWindow(this);

  // X11WindowOzone overrides this method and manages events by itself.
  SetPlatformEventDispatcher();
  platform_window_delegate_->OnAcceleratedWidgetAvailable(GetWidget());
}

void X11Window::OnXWindowStateChanged() {
  // Propagate the window state information to the client. Note that the order
  // of checks is important here, because window can have several properties
  // at the same time.
  PlatformWindowState old_state = state_;
  if (IsMinimized()) {
    state_ = PlatformWindowState::kMinimized;
  } else if (IsFullscreen()) {
    state_ = PlatformWindowState::kFullScreen;
  } else if (IsMaximized()) {
    state_ = PlatformWindowState::kMaximized;
  } else {
    state_ = PlatformWindowState::kNormal;
  }

  if (old_state != state_)
    platform_window_delegate_->OnWindowStateChanged(state_);
}

void X11Window::OnXWindowDamageEvent(const gfx::Rect& damage_rect) {
  platform_window_delegate_->OnDamageRect(damage_rect);
}

void X11Window::OnXWindowBoundsChanged(const gfx::Rect& bounds) {
  platform_window_delegate_->OnBoundsChanged(bounds);
}

void X11Window::OnXWindowCloseRequested() {
  platform_window_delegate_->OnCloseRequest();
}

void X11Window::OnXWindowIsActiveChanged(bool active) {
  platform_window_delegate_->OnActivationChanged(active);
}

void X11Window::OnXWindowMapped() {
  platform_window_delegate_->OnXWindowMapped();
}

void X11Window::OnXWindowUnmapped() {
  platform_window_delegate_->OnXWindowUnmapped();
}

void X11Window::OnXWindowWorkspaceChanged() {
  platform_window_delegate_->OnWorkspaceChanged();
}

void X11Window::OnXWindowLostPointerGrab() {
  platform_window_delegate_->OnLostMouseGrab();
}

void X11Window::OnXWindowEvent(ui::Event* event) {
  DCHECK_NE(window(), x11::None);
  DCHECK(event);

  auto* window_manager = X11WindowManager::GetInstance();
  DCHECK(window_manager);

  // If |event| is a located event (mouse, touch, etc) and another X11 window
  // is set as the current located events grabber, the |event| must be
  // re-routed to that grabber. Otherwise, just send the event.
  auto* located_events_grabber = window_manager->located_events_grabber();
  if (event->IsLocatedEvent() && located_events_grabber &&
      located_events_grabber != this) {
    if (event->IsMouseEvent() ||
        (event->IsTouchEvent() && event->type() == ui::ET_TOUCH_PRESSED)) {
      // Another X11Window has installed itself as capture. Translate the
      // event's location and dispatch to the other.
      ConvertEventLocationToTargetLocation(located_events_grabber->GetBounds(),
                                           GetBounds(),
                                           event->AsLocatedEvent());
    }
    located_events_grabber->OnXWindowEvent(event);
    return;
  }

#if defined(USE_OZONE)
  DispatchEventFromNativeUiEvent(
      event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                            base::Unretained(platform_window_delegate())));
#else
  platform_window_delegate_->DispatchEvent(event);
#endif
}

void X11Window::OnXWindowSelectionEvent(XEvent* xev) {
  if (x_event_delegate_)
    x_event_delegate_->OnXWindowSelectionEvent(xev);
}

void X11Window::OnXWindowDragDropEvent(XEvent* xev) {
  if (x_event_delegate_)
    x_event_delegate_->OnXWindowDragDropEvent(xev);
}

base::Optional<gfx::Size> X11Window::GetMinimumSizeForXWindow() {
  return platform_window_delegate_->GetMinimumSizeForWindow();
}

base::Optional<gfx::Size> X11Window::GetMaximumSizeForXWindow() {
  return platform_window_delegate_->GetMaximumSizeForWindow();
}

void X11Window::GetWindowMaskForXWindow(const gfx::Size& size,
                                        SkPath* window_mask) {
  platform_window_delegate_->GetWindowMask(size, window_mask);
}

void X11Window::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location_in_px) {
  XWindow::WmMoveResize(hittest, pointer_location_in_px);
}

void X11Window::SetPlatformEventDispatcher() {
  DCHECK(PlatformEventSource::GetInstance());
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

gfx::Size X11Window::AdjustSizeForDisplay(
    const gfx::Size& requested_size_in_pixels) {
#if defined(OS_CHROMEOS)
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

void X11Window::ConvertEventLocationToTargetLocation(
    const gfx::Rect& target_window_bounds,
    const gfx::Rect& current_window_bounds,
    ui::LocatedEvent* located_event) {
  // TODO(msisov): for ozone, we need to access PlatformScreen instead and get
  // the displays.
  auto* display = display::Screen::GetScreen();
  DCHECK(display);
  auto display_window_target =
      display->GetDisplayMatching(target_window_bounds);
  auto display_window_current =
      display->GetDisplayMatching(current_window_bounds);
  DCHECK_EQ(display_window_target.device_scale_factor(),
            display_window_current.device_scale_factor());

  ConvertEventLocationToTargetWindowLocation(target_window_bounds.origin(),
                                             current_window_bounds.origin(),
                                             located_event);
}

}  // namespace ui
