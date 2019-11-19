// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

#include "ui/aura/null_window_targeter.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/platform_window_linux.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/window_event_filter_linux.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(USE_ATK)
#include "ui/accessibility/platform/atk_util_auralinux.h"
#endif

DEFINE_UI_CLASS_PROPERTY_TYPE(views::DesktopWindowTreeHostLinux*)

namespace views {

std::list<gfx::AcceleratedWidget>* DesktopWindowTreeHostLinux::open_windows_ =
    nullptr;

DEFINE_UI_CLASS_PROPERTY_KEY(DesktopWindowTreeHostLinux*,
                             kHostForRootWindow,
                             nullptr)

namespace {

class SwapWithNewSizeObserverHelper : public ui::CompositorObserver {
 public:
  using HelperCallback = base::RepeatingCallback<void(const gfx::Size&)>;
  SwapWithNewSizeObserverHelper(ui::Compositor* compositor,
                                const HelperCallback& callback)
      : compositor_(compositor), callback_(callback) {
    compositor_->AddObserver(this);
  }
  ~SwapWithNewSizeObserverHelper() override {
    if (compositor_)
      compositor_->RemoveObserver(this);
  }

 private:
  // ui::CompositorObserver:
  void OnCompositingCompleteSwapWithNewSize(ui::Compositor* compositor,
                                            const gfx::Size& size) override {
    DCHECK_EQ(compositor, compositor_);
    callback_.Run(size);
  }
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    DCHECK_EQ(compositor, compositor_);
    compositor_->RemoveObserver(this);
    compositor_ = nullptr;
  }

  ui::Compositor* compositor_;
  const HelperCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SwapWithNewSizeObserverHelper);
};

}  // namespace

DesktopWindowTreeHostLinux::DesktopWindowTreeHostLinux(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostPlatform(native_widget_delegate,
                                    desktop_native_widget_aura) {}

DesktopWindowTreeHostLinux::~DesktopWindowTreeHostLinux() {
  window()->ClearProperty(kHostForRootWindow);
}

// static
aura::Window* DesktopWindowTreeHostLinux::GetContentWindowForWidget(
    gfx::AcceleratedWidget widget) {
  auto* host = DesktopWindowTreeHostLinux::GetHostForWidget(widget);
  return host ? host->GetContentWindow() : nullptr;
}

// static
DesktopWindowTreeHostLinux* DesktopWindowTreeHostLinux::GetHostForWidget(
    gfx::AcceleratedWidget widget) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(widget);
  return host ? host->window()->GetProperty(kHostForRootWindow) : nullptr;
}

// static
std::vector<aura::Window*> DesktopWindowTreeHostLinux::GetAllOpenWindows() {
  std::vector<aura::Window*> windows(open_windows().size());
  std::transform(open_windows().begin(), open_windows().end(), windows.begin(),
                 GetContentWindowForWidget);
  return windows;
}

// static
void DesktopWindowTreeHostLinux::CleanUpWindowList(
    void (*func)(aura::Window* window)) {
  if (!open_windows_)
    return;
  while (!open_windows_->empty()) {
    gfx::AcceleratedWidget widget = open_windows_->front();
    func(GetContentWindowForWidget(widget));
    if (!open_windows_->empty() && open_windows_->front() == widget)
      open_windows_->erase(open_windows_->begin());
  }

  delete open_windows_;
  open_windows_ = nullptr;
}

void DesktopWindowTreeHostLinux::SetPendingXVisualId(int x_visual_id) {
  pending_x_visual_id_ = x_visual_id;
}

gfx::Rect DesktopWindowTreeHostLinux::GetXRootWindowOuterBounds() const {
  return GetPlatformWindowLinux()->GetXRootWindowOuterBounds();
}

bool DesktopWindowTreeHostLinux::ContainsPointInXRegion(
    const gfx::Point& point) const {
  return GetPlatformWindowLinux()->ContainsPointInXRegion(point);
}

void DesktopWindowTreeHostLinux::LowerXWindow() {
  GetPlatformWindowLinux()->LowerXWindow();
}

base::OnceClosure DesktopWindowTreeHostLinux::DisableEventListening() {
  // Allows to open multiple file-pickers. See https://crbug.com/678982
  modal_dialog_counter_++;
  if (modal_dialog_counter_ == 1) {
    // ScopedWindowTargeter is used to temporarily replace the event-targeter
    // with NullWindowEventTargeter to make |dialog| modal.
    targeter_for_modal_ = std::make_unique<aura::ScopedWindowTargeter>(
        window(), std::make_unique<aura::NullWindowTargeter>());
  }

  return base::BindOnce(&DesktopWindowTreeHostLinux::EnableEventListening,
                        weak_factory_.GetWeakPtr());
}

void DesktopWindowTreeHostLinux::Init(const Widget::InitParams& params) {
  DesktopWindowTreeHostPlatform::Init(params);

  if (GetPlatformWindowLinux()->IsSyncExtensionAvailable()) {
    compositor_observer_ = std::make_unique<SwapWithNewSizeObserverHelper>(
        compositor(),
        base::BindRepeating(
            &DesktopWindowTreeHostLinux::OnCompleteSwapWithNewSize,
            base::Unretained(this)));
  }
}

void DesktopWindowTreeHostLinux::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  window()->SetProperty(kHostForRootWindow, this);

  CreateNonClientEventFilter();
  DesktopWindowTreeHostPlatform::OnNativeWidgetCreated(params);
}

std::string DesktopWindowTreeHostLinux::GetWorkspace() const {
  base::Optional<int> workspace = GetPlatformWindowLinux()->GetWorkspace();
  return workspace ? base::NumberToString(workspace.value()) : std::string();
}

void DesktopWindowTreeHostLinux::SetVisibleOnAllWorkspaces(
    bool always_visible) {
  GetPlatformWindowLinux()->SetVisibleOnAllWorkspaces(always_visible);
}

bool DesktopWindowTreeHostLinux::IsVisibleOnAllWorkspaces() const {
  return GetPlatformWindowLinux()->IsVisibleOnAllWorkspaces();
}

void DesktopWindowTreeHostLinux::SetOpacity(float opacity) {
  DesktopWindowTreeHostPlatform::SetOpacity(opacity);
  // Note that this is no-op for Wayland.
  GetPlatformWindowLinux()->SetOpacityForXWindow(opacity);
}

base::flat_map<std::string, std::string>
DesktopWindowTreeHostLinux::GetKeyboardLayoutMap() {
  if (views::LinuxUI::instance())
    return views::LinuxUI::instance()->GetKeyboardLayoutMap();
  return {};
}

void DesktopWindowTreeHostLinux::InitModalType(ui::ModalType modal_type) {
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

void DesktopWindowTreeHostLinux::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  aura::WindowTreeHost::OnDisplayMetricsChanged(display, changed_metrics);

  if ((changed_metrics & DISPLAY_METRIC_DEVICE_SCALE_FACTOR) &&
      display::Screen::GetScreen()->GetDisplayNearestWindow(window()).id() ==
          display.id()) {
    // When the scale factor changes, also pretend that a resize
    // occurred so that the window layout will be refreshed and a
    // compositor redraw will be scheduled.  This is weird, but works.
    // TODO(thomasanderson): Figure out a more direct way of doing
    // this.
    OnHostResizedInPixels(GetBoundsInPixels().size());
  }
}

void DesktopWindowTreeHostLinux::DispatchEvent(ui::Event* event) {
  // The input can be disabled and the widget marked as non-active in case of
  // opened file-dialogs.
  if (event->IsKeyEvent() && !native_widget_delegate()->AsWidget()->IsActive())
    return;

  // In Windows, the native events sent to chrome are separated into client
  // and non-client versions of events, which we record on our LocatedEvent
  // structures. On X11/Wayland, we emulate the concept of non-client. Before we
  // pass this event to the cross platform event handling framework, we need to
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
  int hit_test_code = HTNOWHERE;
  if (event->IsMouseEvent()) {
    ui::MouseEvent* mouse_event = event->AsMouseEvent();
    if (GetContentWindow() && GetContentWindow()->delegate()) {
      int flags = mouse_event->flags();
      gfx::Point location_in_dip = mouse_event->location();
      GetRootTransform().TransformPointReverse(&location_in_dip);
      hit_test_code = GetContentWindow()->delegate()->GetNonClientComponent(
          location_in_dip);
      if (hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE)
        flags |= ui::EF_IS_NON_CLIENT;
      mouse_event->set_flags(flags);
    }

    // While we unset the urgency hint when we gain focus, we also must remove
    // it on mouse clicks because we can call FlashFrame() on an active window.
    if (mouse_event->IsAnyButton() || mouse_event->IsMouseWheelEvent())
      FlashFrame(false);
  }

  // Store the location in px to restore it later.
  gfx::Point previous_mouse_location_in_px;
  if (event->IsMouseEvent())
    previous_mouse_location_in_px = event->AsMouseEvent()->location();

  WindowTreeHostPlatform::DispatchEvent(event);

  // Posthandle the event if it has not been consumed.
  if (!event->handled() && event->IsMouseEvent() &&
      non_client_window_event_filter_) {
    auto* mouse_event = event->AsMouseEvent();
    // Location is set in dip after the event is dispatched to the event sink.
    // Restore it back to be in px that WindowEventFilterLinux requires.
    mouse_event->set_location(previous_mouse_location_in_px);
    mouse_event->set_root_location(previous_mouse_location_in_px);
    non_client_window_event_filter_->HandleMouseEventWithHitTest(hit_test_code,
                                                                 mouse_event);
  }
}

void DesktopWindowTreeHostLinux::OnClosed() {
  open_windows().remove(GetAcceleratedWidget());
  DestroyNonClientEventFilter();
  DesktopWindowTreeHostPlatform::OnClosed();
}

void DesktopWindowTreeHostLinux::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  open_windows().push_front(widget);
  DesktopWindowTreeHostPlatform::OnAcceleratedWidgetAvailable(widget);
}

void DesktopWindowTreeHostLinux::OnActivationChanged(bool active) {
  if (active) {
    auto widget = GetAcceleratedWidget();
    open_windows().remove(widget);
    open_windows().insert(open_windows().begin(), widget);
  }
  DesktopWindowTreeHostPlatform::OnActivationChanged(active);
}

#if BUILDFLAG(USE_ATK)
bool DesktopWindowTreeHostLinux::OnAtkKeyEvent(AtkKeyEventStruct* atk_event) {
  if (!IsActive() && !HasCapture())
    return false;
  return ui::AtkUtilAuraLinux::HandleAtkKeyEvent(atk_event) ==
         ui::DiscardAtkKeyEvent::Discard;
}
#endif

void DesktopWindowTreeHostLinux::AddAdditionalInitProperties(
    const Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {
  // Set the background color on startup to make the initial flickering
  // happening between the XWindow is mapped and the first expose event
  // is completely handled less annoying. If possible, we use the content
  // window's background color, otherwise we fallback to white.
  base::Optional<int> background_color;
  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui && GetContentWindow()) {
    ui::NativeTheme::ColorId target_color;
    switch (properties->type) {
      case ui::PlatformWindowType::kBubble:
        target_color = ui::NativeTheme::kColorId_BubbleBackground;
        break;
      case ui::PlatformWindowType::kTooltip:
        target_color = ui::NativeTheme::kColorId_TooltipBackground;
        break;
      default:
        target_color = ui::NativeTheme::kColorId_WindowBackground;
        break;
    }
    ui::NativeTheme* theme = linux_ui->GetNativeTheme(GetContentWindow());
    background_color = theme->GetSystemColor(target_color);
  }
  properties->prefer_dark_theme = linux_ui && linux_ui->PreferDarkTheme();
  properties->background_color = background_color;
  properties->icon = ViewsDelegate::GetInstance()->GetDefaultWindowIcon();

  properties->wm_class_name = params.wm_class_name;
  properties->wm_class_class = params.wm_class_class;
  properties->wm_role_name = params.wm_role_name;

  properties->x_visual_id = pending_x_visual_id_;
}

void DesktopWindowTreeHostLinux::OnCompleteSwapWithNewSize(
    const gfx::Size& size) {
  GetPlatformWindowLinux()->OnCompleteSwapAfterResize();
}

void DesktopWindowTreeHostLinux::CreateNonClientEventFilter() {
  DCHECK(!non_client_window_event_filter_);
  non_client_window_event_filter_ = std::make_unique<WindowEventFilterLinux>(
      this, GetWmMoveResizeHandler(*GetPlatformWindowLinux()));
}

void DesktopWindowTreeHostLinux::DestroyNonClientEventFilter() {
  non_client_window_event_filter_.reset();
}

void DesktopWindowTreeHostLinux::OnWorkspaceChanged() {
  OnHostWorkspaceChanged();
}

void DesktopWindowTreeHostLinux::GetWindowMask(const gfx::Size& size,
                                               SkPath* window_mask) {
  DCHECK(window_mask);
  Widget* widget = native_widget_delegate()->AsWidget();
  if (widget->non_client_view()) {
    // Some frame views define a custom (non-rectangular) window mask. If
    // so, use it to define the window shape. If not, fall through.
    widget->non_client_view()->GetWindowMask(size, window_mask);
  }
}

void DesktopWindowTreeHostLinux::OnLostMouseGrab() {
  dispatcher()->OnHostLostMouseGrab();
}

void DesktopWindowTreeHostLinux::EnableEventListening() {
  DCHECK_GT(modal_dialog_counter_, 0UL);
  if (!--modal_dialog_counter_)
    targeter_for_modal_.reset();
}

const ui::PlatformWindowLinux*
DesktopWindowTreeHostLinux::GetPlatformWindowLinux() const {
  return static_cast<const ui::PlatformWindowLinux*>(platform_window());
}

ui::PlatformWindowLinux* DesktopWindowTreeHostLinux::GetPlatformWindowLinux() {
  return static_cast<ui::PlatformWindowLinux*>(platform_window());
}

std::list<gfx::AcceleratedWidget>& DesktopWindowTreeHostLinux::open_windows() {
  if (!open_windows_)
    open_windows_ = new std::list<gfx::AcceleratedWidget>();
  return *open_windows_;
}

// As DWTHX11 subclasses DWTHPlatform through DWTHLinux now (during transition
// period. see https://crbug.com/990756), we need to guard this factory method.
// TODO(msisov): remove this guard once DWTHX11 is finally merged into
// DWTHPlatform and .
#if !defined(USE_X11)
// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostLinux(native_widget_delegate,
                                        desktop_native_widget_aura);
}
#endif

}  // namespace views
