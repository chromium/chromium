// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "ui/aura/null_window_targeter.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/platform_window/extensions/x11_extension.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/window_event_filter_linux.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(USE_ATK)
#include "ui/accessibility/platform/atk_util_auralinux.h"
#endif

namespace views {

std::list<gfx::AcceleratedWidget>* DesktopWindowTreeHostLinux::open_windows_ =
    nullptr;

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

DesktopWindowTreeHostLinux::~DesktopWindowTreeHostLinux() = default;

// static
std::vector<aura::Window*> DesktopWindowTreeHostLinux::GetAllOpenWindows() {
  std::vector<aura::Window*> windows(open_windows().size());
  std::transform(open_windows().begin(), open_windows().end(), windows.begin(),
                 DesktopWindowTreeHostPlatform::GetContentWindowForWidget);
  return windows;
}

// static
void DesktopWindowTreeHostLinux::CleanUpWindowList(
    void (*func)(aura::Window* window)) {
  if (!open_windows_)
    return;
  while (!open_windows_->empty()) {
    gfx::AcceleratedWidget widget = open_windows_->front();
    func(DesktopWindowTreeHostPlatform::GetContentWindowForWidget(widget));
    if (!open_windows_->empty() && open_windows_->front() == widget)
      open_windows_->erase(open_windows_->begin());
  }

  delete open_windows_;
  open_windows_ = nullptr;
}

gfx::Rect DesktopWindowTreeHostLinux::GetXRootWindowOuterBounds() const {
  // TODO(msisov): must be removed as soon as all X11 low-level bits are moved
  // to Ozone.
  DCHECK(GetX11Extension());
  return GetX11Extension()->GetXRootWindowOuterBounds();
}

bool DesktopWindowTreeHostLinux::ContainsPointInXRegion(
    const gfx::Point& point) const {
  // TODO(msisov): must be removed as soon as all X11 low-level bits are moved
  // to Ozone.
  DCHECK(GetX11Extension());
  return GetX11Extension()->ContainsPointInXRegion(point);
}

void DesktopWindowTreeHostLinux::LowerXWindow() {
  // TODO(msisov): must be removed as soon as all X11 low-level bits are moved
  // to Ozone.
  DCHECK(GetX11Extension());
  GetX11Extension()->LowerXWindow();
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

  if (GetX11Extension() && GetX11Extension()->IsSyncExtensionAvailable()) {
    compositor_observer_ = std::make_unique<SwapWithNewSizeObserverHelper>(
        compositor(),
        base::BindRepeating(
            &DesktopWindowTreeHostLinux::OnCompleteSwapWithNewSize,
            base::Unretained(this)));
  }
}

void DesktopWindowTreeHostLinux::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  CreateNonClientEventFilter();
  DesktopWindowTreeHostPlatform::OnNativeWidgetCreated(params);
}

base::flat_map<std::string, std::string>
DesktopWindowTreeHostLinux::GetKeyboardLayoutMap() {
  if (features::IsUsingOzonePlatform())
    return DesktopWindowTreeHostPlatform::GetKeyboardLayoutMap();
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

Widget::MoveLoopResult DesktopWindowTreeHostLinux::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  GetContentWindow()->SetCapture();
  return DesktopWindowTreeHostPlatform::RunMoveLoop(drag_offset, source,
                                                    escape_behavior);
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

  // Prehandle the event as long as as we are not able to track if it is handled
  // or not as SendEventToSink results in copying the event and our copy of the
  // event will not set to handled unless a dispatcher or a target are
  // destroyed.
  if (event->IsMouseEvent() && non_client_window_event_filter_) {
    non_client_window_event_filter_->HandleMouseEventWithHitTest(
        hit_test_code, event->AsMouseEvent());
  }

  if (!event->handled())
    WindowTreeHostPlatform::DispatchEvent(event);
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

ui::X11Extension* DesktopWindowTreeHostLinux::GetX11Extension() {
  return ui::GetX11Extension(*(platform_window()));
}

const ui::X11Extension* DesktopWindowTreeHostLinux::GetX11Extension() const {
  return ui::GetX11Extension(*(platform_window()));
}

#if BUILDFLAG(USE_ATK)
bool DesktopWindowTreeHostLinux::OnAtkKeyEvent(AtkKeyEventStruct* atk_event,
                                               bool transient) {
  if (!transient && !IsActive() && !HasCapture())
    return false;
  return ui::AtkUtilAuraLinux::HandleAtkKeyEvent(atk_event) ==
         ui::DiscardAtkKeyEvent::Discard;
}
#endif

bool DesktopWindowTreeHostLinux::IsOverrideRedirect(bool is_tiling_wm) const {
  // BrowserDesktopWindowTreeHostLinux implements this for browser windows.
  return false;
}

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

  DCHECK(!properties->x11_extension_delegate);
  properties->x11_extension_delegate = this;
}

void DesktopWindowTreeHostLinux::OnCompleteSwapWithNewSize(
    const gfx::Size& size) {
  if (GetX11Extension())
    GetX11Extension()->OnCompleteSwapAfterResize();
}

void DesktopWindowTreeHostLinux::CreateNonClientEventFilter() {
  DCHECK(!non_client_window_event_filter_);
  non_client_window_event_filter_ = std::make_unique<WindowEventFilterLinux>(
      this, GetWmMoveResizeHandler(*platform_window()));
}

void DesktopWindowTreeHostLinux::DestroyNonClientEventFilter() {
  non_client_window_event_filter_.reset();
}

void DesktopWindowTreeHostLinux::OnLostMouseGrab() {
  dispatcher()->OnHostLostMouseGrab();
}

void DesktopWindowTreeHostLinux::EnableEventListening() {
  DCHECK_GT(modal_dialog_counter_, 0UL);
  if (!--modal_dialog_counter_)
    targeter_for_modal_.reset();
}

std::list<gfx::AcceleratedWidget>& DesktopWindowTreeHostLinux::open_windows() {
  if (!open_windows_)
    open_windows_ = new std::list<gfx::AcceleratedWidget>();
  return *open_windows_;
}

// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostLinux(native_widget_delegate,
                                        desktop_native_widget_aura);
}

}  // namespace views
