// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/aura/null_window_targeter.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/linux/linux_ui.h"
#include "ui/platform_window/extensions/desk_extension.h"
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/extensions/x11_extension.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(USE_ATK)
#include "ui/accessibility/platform/atk_util_auralinux.h"
#endif

#include "ui/views/widget/desktop_aura/window_event_filter_linux.h"

namespace views {
namespace {

class SwapWithNewSizeObserverHelper : public ui::CompositorObserver {
 public:
  using HelperCallback = base::RepeatingCallback<void(const gfx::Size&)>;
  SwapWithNewSizeObserverHelper(ui::Compositor* compositor,
                                const HelperCallback& callback)
      : compositor_(compositor), callback_(callback) {
    compositor_->AddObserver(this);
  }

  SwapWithNewSizeObserverHelper(const SwapWithNewSizeObserverHelper&) = delete;
  SwapWithNewSizeObserverHelper& operator=(
      const SwapWithNewSizeObserverHelper&) = delete;

  ~SwapWithNewSizeObserverHelper() override {
    if (compositor_)
      compositor_->RemoveObserver(this);
  }

 private:
  // ui::CompositorObserver:
#if BUILDFLAG(IS_OZONE_X11)
  void OnCompositingCompleteSwapWithNewSize(ui::Compositor* compositor,
                                            const gfx::Size& size) override {
    DCHECK_EQ(compositor, compositor_);
    callback_.Run(size);
  }
#endif  // BUILDFLAG(IS_OZONE_X11)

  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    DCHECK_EQ(compositor, compositor_);
    compositor_->RemoveObserver(this);
    compositor_ = nullptr;
  }

  raw_ptr<ui::Compositor> compositor_;
  const HelperCallback callback_;
};

}  // namespace

// static
const char DesktopWindowTreeHostLinux::kWindowKey[] =
    "DesktopWindowTreeHostLinux";

DesktopWindowTreeHostLinux::DesktopWindowTreeHostLinux(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostPlatform(native_widget_delegate,
                                    desktop_native_widget_aura) {
  window()->SetNativeWindowProperty(kWindowKey, this);
}

DesktopWindowTreeHostLinux::~DesktopWindowTreeHostLinux() = default;

gfx::Rect DesktopWindowTreeHostLinux::GetXRootWindowOuterBounds() const {
  // TODO(msisov): must be removed as soon as all X11 low-level bits are moved
  // to Ozone.
  DCHECK(GetX11Extension());
  return GetX11Extension()->GetXRootWindowOuterBounds();
}

void DesktopWindowTreeHostLinux::LowerWindow() {
  if (GetX11Extension())
    GetX11Extension()->LowerXWindow();
  else
    NOTIMPLEMENTED_LOG_ONCE();
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

void DesktopWindowTreeHostLinux::UpdateFrameHints() {
  if (!GetContentWindow()->targeter()) {
    platform_window()->SetInputRegion(std::nullopt);
  } else {
    // Content window can have a window_targeter that allows located events fall
    // to the window underneath it. There is no window underneath the content
    // window from aura's point of view so wayland platform needs to know about
    // it.
    gfx::Rect hit_test_rect_mouse_dp{
        platform_window()->GetBoundsInDIP().size()};
    gfx::Rect hit_test_rect_touch_dp = gfx::Rect{hit_test_rect_mouse_dp};
    GetContentWindow()->targeter()->GetHitTestRects(
        GetContentWindow(), &hit_test_rect_mouse_dp, &hit_test_rect_touch_dp);
    platform_window()->SetInputRegion(std::optional<std::vector<gfx::Rect>>(
        {ConvertRectToPixels(hit_test_rect_mouse_dp)}));
  }
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

void DesktopWindowTreeHostLinux::InitModalType(
    ui::mojom::ModalType modal_type) {
  switch (modal_type) {
    case ui::mojom::ModalType::kNone:
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

  // DesktopWindowTreeHostLinux::RunMoveLoop() may result in |this| being
  // deleted. As an extra safety guard, keep track of |this| with a weak
  // pointer, and only call ReleaseCapture() if it still exists.
  //
  // TODO(crbug.com/40212051): Consider removing capture set/unset
  // during window drag 'n drop (detached).
  auto weak_this = weak_factory_.GetWeakPtr();

  Widget::MoveLoopResult result = DesktopWindowTreeHostPlatform::RunMoveLoop(
      drag_offset, source, escape_behavior);
  if (weak_this.get())
    GetContentWindow()->ReleaseCapture();

  return result;
}

void DesktopWindowTreeHostLinux::DispatchEvent(ui::Event* event) {
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
  if (event->IsMouseEvent() || event->IsTouchEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    if (GetContentWindow() && GetContentWindow()->delegate()) {
      int flags = located_event->flags();
      gfx::PointF location = located_event->location_f();
      gfx::PointF location_in_dip =
          GetRootTransform().InverseMapPoint(location).value_or(location);
      hit_test_code = GetContentWindow()->delegate()->GetNonClientComponent(
          gfx::ToRoundedPoint(location_in_dip));
      if (hit_test_code != HTCLIENT && hit_test_code != HTNOWHERE)
        flags |= ui::EF_IS_NON_CLIENT;
      located_event->SetFlags(flags);
    }

    // While we unset the urgency hint when we gain focus, we also must remove
    // it on mouse clicks because we can call FlashFrame() on an active window.
    if (located_event->IsMouseEvent() &&
        (located_event->AsMouseEvent()->IsAnyButton() ||
         located_event->IsMouseWheelEvent()))
      FlashFrame(false);
  }

  // Prehandle the event as long as as we are not able to track if it is handled
  // or not as SendEventToSink results in copying the event and our copy of the
  // event will not set to handled unless a dispatcher or a target are
  // destroyed.
  if ((event->IsMouseEvent() || event->IsTouchEvent()) &&
      non_client_window_event_filter_) {
    non_client_window_event_filter_->HandleLocatedEventWithHitTest(
        hit_test_code, event->AsLocatedEvent());
  }

  if (!event->handled())
    WindowTreeHostPlatform::DispatchEvent(event);
}

void DesktopWindowTreeHostLinux::OnClosed() {
  DestroyNonClientEventFilter();
  DesktopWindowTreeHostPlatform::OnClosed();
}

void DesktopWindowTreeHostLinux::OnBoundsChanged(const BoundsChange& change) {
  // DesktopWindowTreeHostPlatform::OnBoundsChanged() may result in |this| being
  // deleted. As an extra safety guard, keep track of |this| with a weak
  // pointer, and only call UpdateFrameHints() if it still exists.
  auto weak_this = weak_factory_.GetWeakPtr();
  DesktopWindowTreeHostPlatform::OnBoundsChanged(change);

  if (weak_this.get()) {
    UpdateFrameHints();
  }
}

ui::X11Extension* DesktopWindowTreeHostLinux::GetX11Extension() {
  return platform_window() ? ui::GetX11Extension(*(platform_window()))
                           : nullptr;
}

const ui::X11Extension* DesktopWindowTreeHostLinux::GetX11Extension() const {
  return platform_window() ? ui::GetX11Extension(*(platform_window()))
                           : nullptr;
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

bool DesktopWindowTreeHostLinux::IsOverrideRedirect() const {
  // BrowserDesktopWindowTreeHostLinux implements this for browser windows.
  return false;
}

gfx::Rect DesktopWindowTreeHostLinux::GetGuessedFullScreenSizeInPx() const {
  display::Screen* screen = display::Screen::GetScreen();
  const display::Display display =
      screen->GetDisplayMatching(GetWindowBoundsInScreen());
  return gfx::Rect(gfx::ScaleToFlooredPoint(display.bounds().origin(),
                                            display.device_scale_factor()),
                   display.GetSizeInPixel());
}

void DesktopWindowTreeHostLinux::AddAdditionalInitProperties(
    const Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {
  // Set the background color on startup to make the initial flickering
  // happening between the XWindow is mapped and the first expose event
  // is completely handled less annoying. If possible, we use the content
  // window's background color, otherwise we fallback to white.
  ui::ColorId target_color;
  switch (properties->type) {
    case ui::PlatformWindowType::kBubble:
      target_color = ui::kColorBubbleBackground;
      break;
    case ui::PlatformWindowType::kTooltip:
      target_color = ui::kColorTooltipBackground;
      break;
    default:
      target_color = ui::kColorWindowBackground;
      break;
  }
  properties->background_color =
      GetWidget()->GetColorProvider()->GetColor(target_color);

  properties->icon = ViewsDelegate::GetInstance()->GetDefaultWindowIcon();

  properties->wm_class_name = params.wm_class_name;
  properties->wm_class_class = params.wm_class_class;
  properties->wm_role_name = params.wm_role_name;

  properties->wayland_app_id = params.wayland_app_id;

  DCHECK(!properties->x11_extension_delegate);
  properties->x11_extension_delegate = this;
}

base::flat_map<std::string, std::string>
DesktopWindowTreeHostLinux::GetKeyboardLayoutMap() {
  if (auto* linux_ui = ui::LinuxUi::instance())
    return linux_ui->GetKeyboardLayoutMap();
  return WindowTreeHostPlatform::GetKeyboardLayoutMap();
}

void DesktopWindowTreeHostLinux::OnCompleteSwapWithNewSize(
    const gfx::Size& size) {
  if (GetX11Extension())
    GetX11Extension()->OnCompleteSwapAfterResize();
}

void DesktopWindowTreeHostLinux::CreateNonClientEventFilter() {
  DCHECK(!non_client_window_event_filter_);
  non_client_window_event_filter_ = std::make_unique<WindowEventFilterClass>(
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

// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostLinux(native_widget_delegate,
                                        desktop_native_widget_aura);
}

}  // namespace views
