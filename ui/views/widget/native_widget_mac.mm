// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/mac/scoped_nsobject.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/crash/core/common/crash_key.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#import "components/remote_cocoa/app_shim/views_nswindow_delegate.h"
#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/gestures/gesture_recognizer_impl_mac.h"
#include "ui/gfx/font_list.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_mac.h"
#import "ui/views/cocoa/drag_drop_client_mac.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/native_frame_view.h"

using remote_cocoa::mojom::WindowVisibilityState;

namespace views {

namespace {

static base::RepeatingCallback<void(NativeWidgetMac*)>*
    g_init_native_widget_callback = nullptr;

NSInteger StyleMaskForParams(const Widget::InitParams& params) {
  // If the Widget is modal, it will be displayed as a sheet. This works best if
  // it has NSTitledWindowMask. For example, with NSBorderlessWindowMask, the
  // parent window still accepts input.
  // NSFullSizeContentViewWindowMask ensures that calculating the modal's
  // content rect doesn't account for a nonexistent title bar.
  if (params.delegate &&
      params.delegate->GetModalType() == ui::MODAL_TYPE_WINDOW)
    return NSTitledWindowMask | NSFullSizeContentViewWindowMask;

  // TODO(tapted): Determine better masks when there are use cases for it.
  if (params.remove_standard_frame)
    return NSBorderlessWindowMask;

  if (params.type == Widget::InitParams::TYPE_WINDOW) {
    return NSTitledWindowMask | NSClosableWindowMask |
           NSMiniaturizableWindowMask | NSResizableWindowMask |
           NSTexturedBackgroundWindowMask;
  }
  return NSBorderlessWindowMask;
}

CGWindowLevel CGWindowLevelForZOrderLevel(ui::ZOrderLevel level,
                                          Widget::InitParams::Type type) {
  switch (level) {
    case ui::ZOrderLevel::kNormal:
      return kCGNormalWindowLevel;
    case ui::ZOrderLevel::kFloatingWindow:
      if (type == Widget::InitParams::TYPE_MENU)
        return kCGPopUpMenuWindowLevel;
      else
        return kCGFloatingWindowLevel;
    case ui::ZOrderLevel::kFloatingUIElement:
      if (type == Widget::InitParams::TYPE_DRAG)
        return kCGDraggingWindowLevel;
      else
        return kCGStatusWindowLevel;
    case ui::ZOrderLevel::kSecuritySurface:
      return kCGScreenSaverWindowLevel - 1;
  }
}

}  // namespace

// Implements zoom following focus for macOS accessibility zoom.
class NativeWidgetMac::ZoomFocusMonitor : public FocusChangeListener {
 public:
  ZoomFocusMonitor() = default;
  ~ZoomFocusMonitor() override {}
  void OnWillChangeFocus(View* focused_before, View* focused_now) override {}
  void OnDidChangeFocus(View* focused_before, View* focused_now) override {
    if (!focused_now || !UAZoomEnabled())
      return;
    // Web content handles its own zooming.
    if (strcmp("WebView", focused_now->GetClassName()) == 0)
      return;
    NSRect rect = NSRectFromCGRect(focused_now->GetBoundsInScreen().ToCGRect());
    UAZoomChangeFocus(&rect, nullptr, kUAZoomFocusTypeOther);
  }
};

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, public:

NativeWidgetMac::NativeWidgetMac(internal::NativeWidgetDelegate* delegate)
    : zoom_focus_monitor_(std::make_unique<ZoomFocusMonitor>()),
      delegate_(delegate),
      ns_window_host_(new NativeWidgetMacNSWindowHost(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {}

NativeWidgetMac::~NativeWidgetMac() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
  else
    CloseNow();
}

void NativeWidgetMac::WindowDestroying() {
  if (auto* focus_manager = GetWidget()->GetFocusManager())
    focus_manager->RemoveFocusChangeListener(zoom_focus_monitor_.get());
  OnWindowDestroying(GetNativeWindow());
  delegate_->OnNativeWidgetDestroying();
}

void NativeWidgetMac::WindowDestroyed() {
  DCHECK(GetNSWindowMojo());
  ns_window_host_.reset();
  // |OnNativeWidgetDestroyed| may delete |this| if the object does not own
  // itself.
  bool should_delete_this =
      (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  delegate_->OnNativeWidgetDestroyed();
  if (should_delete_this)
    delete this;
}

int32_t NativeWidgetMac::SheetOffsetY() {
  return 0;
}

void NativeWidgetMac::GetWindowFrameTitlebarHeight(
    bool* override_titlebar_height,
    float* titlebar_height) {
  *override_titlebar_height = false;
  *titlebar_height = 0;
}

bool NativeWidgetMac::ExecuteCommand(
    int32_t command,
    WindowOpenDisposition window_open_disposition,
    bool is_before_first_responder) {
  // This is supported only by subclasses in chrome/browser/ui.
  NOTIMPLEMENTED();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, internal::NativeWidgetPrivate implementation:

void NativeWidgetMac::InitNativeWidget(Widget::InitParams params) {
  ownership_ = params.ownership;
  name_ = params.name;
  type_ = params.type;
  NativeWidgetMacNSWindowHost* parent_host =
      NativeWidgetMacNSWindowHost::GetFromNativeView(params.parent);

  // Determine the factory through which to create the bridge
  remote_cocoa::ApplicationHost* application_host =
      parent_host ? parent_host->application_host()
                  : GetRemoteCocoaApplicationHost();

  // Compute the parameters to describe the NSWindow.
  auto create_window_params = remote_cocoa::mojom::CreateWindowParams::New();
  create_window_params->window_class =
      remote_cocoa::mojom::WindowClass::kDefault;
  create_window_params->style_mask = StyleMaskForParams(params);
  create_window_params->titlebar_appears_transparent = false;
  create_window_params->window_title_hidden = false;
  PopulateCreateWindowParams(params, create_window_params.get());

  if (application_host) {
    ns_window_host_->CreateRemoteNSWindow(application_host,
                                          std::move(create_window_params));
  } else {
    base::scoped_nsobject<NativeWidgetMacNSWindow> window(
        [CreateNSWindow(create_window_params.get()) retain]);
    ns_window_host_->CreateInProcessNSWindowBridge(std::move(window));
  }
  ns_window_host_->SetParent(parent_host);
  ns_window_host_->InitWindow(params);
  OnWindowInitialized();

  // Only set the z-order here if it is non-default since setting it may affect
  // how the window is treated by Expose.
  if (params.EffectiveZOrderLevel() != ui::ZOrderLevel::kNormal)
    SetZOrderLevel(params.EffectiveZOrderLevel());

  GetNSWindowMojo()->SetIgnoresMouseEvents(!params.accept_events);

  delegate_->OnNativeWidgetCreated();

  DCHECK(GetWidget()->GetRootView());
  ns_window_host_->SetRootView(GetWidget()->GetRootView());
  GetNSWindowMojo()->CreateContentView(ns_window_host_->GetRootViewNSViewId(),
                                       GetWidget()->GetRootView()->bounds());
  if (auto* focus_manager = GetWidget()->GetFocusManager()) {
    GetNSWindowMojo()->MakeFirstResponder();
    ns_window_host_->SetFocusManager(focus_manager);
    // Non-top-level widgets use the the top level widget's focus manager.
    if (GetWidget() == GetTopLevelWidget())
      focus_manager->AddFocusChangeListener(zoom_focus_monitor_.get());
  }

  ns_window_host_->CreateCompositor(params);

  if (g_init_native_widget_callback)
    g_init_native_widget_callback->Run(this);
}

void NativeWidgetMac::OnWidgetInitDone() {
  OnSizeConstraintsChanged();
  ns_window_host_->OnWidgetInitDone();
}

NonClientFrameView* NativeWidgetMac::CreateNonClientFrameView() {
  return new NativeFrameView(GetWidget());
}

bool NativeWidgetMac::ShouldUseNativeFrame() const {
  return true;
}

bool NativeWidgetMac::ShouldWindowContentsBeTransparent() const {
  // On Windows, this returns true when Aero is enabled which draws the titlebar
  // with translucency.
  return false;
}

void NativeWidgetMac::FrameTypeChanged() {
  // This is called when the Theme has changed; forward the event to the root
  // widget.
  GetWidget()->ThemeChanged();
  GetWidget()->GetRootView()->SchedulePaint();
}

Widget* NativeWidgetMac::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetMac::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetMac::GetNativeView() const {
  // Returns a BridgedContentView, unless there is no views::RootView set.
  return [GetNativeWindow().GetNativeNSWindow() contentView];
}

gfx::NativeWindow NativeWidgetMac::GetNativeWindow() const {
  return ns_window_host_ ? ns_window_host_->GetInProcessNSWindow() : nil;
}

Widget* NativeWidgetMac::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : nullptr;
}

const ui::Compositor* NativeWidgetMac::GetCompositor() const {
  return ns_window_host_ && ns_window_host_->layer()
             ? ns_window_host_->layer()->GetCompositor()
             : nullptr;
}

const ui::Layer* NativeWidgetMac::GetLayer() const {
  return ns_window_host_ ? ns_window_host_->layer() : nullptr;
}

void NativeWidgetMac::ReorderNativeViews() {
  if (ns_window_host_)
    ns_window_host_->ReorderChildViews();
}

void NativeWidgetMac::ViewRemoved(View* view) {
  DragDropClientMac* client =
      ns_window_host_ ? ns_window_host_->drag_drop_client() : nullptr;
  if (client)
    client->drop_helper()->ResetTargetViewIfEquals(view);
}

void NativeWidgetMac::SetNativeWindowProperty(const char* name, void* value) {
  if (ns_window_host_)
    ns_window_host_->SetNativeWindowProperty(name, value);
}

void* NativeWidgetMac::GetNativeWindowProperty(const char* name) const {
  if (ns_window_host_)
    return ns_window_host_->GetNativeWindowProperty(name);

  return nullptr;
}

TooltipManager* NativeWidgetMac::GetTooltipManager() const {
  if (ns_window_host_)
    return ns_window_host_->tooltip_manager();

  return nullptr;
}

void NativeWidgetMac::SetCapture() {
  if (GetNSWindowMojo())
    GetNSWindowMojo()->AcquireCapture();
}

void NativeWidgetMac::ReleaseCapture() {
  if (GetNSWindowMojo())
    GetNSWindowMojo()->ReleaseCapture();
}

bool NativeWidgetMac::HasCapture() const {
  return ns_window_host_ && ns_window_host_->IsMouseCaptureActive();
}

ui::InputMethod* NativeWidgetMac::GetInputMethod() {
  return ns_window_host_ ? ns_window_host_->GetInputMethod() : nullptr;
}

void NativeWidgetMac::CenterWindow(const gfx::Size& size) {
  GetNSWindowMojo()->SetSizeAndCenter(size, GetWidget()->GetMinimumSize());
}

void NativeWidgetMac::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = GetRestoredBounds();
  if (IsFullscreen())
    *show_state = ui::SHOW_STATE_FULLSCREEN;
  else if (IsMinimized())
    *show_state = ui::SHOW_STATE_MINIMIZED;
  else
    *show_state = ui::SHOW_STATE_NORMAL;
}

bool NativeWidgetMac::SetWindowTitle(const base::string16& title) {
  if (!ns_window_host_)
    return false;
  return ns_window_host_->SetWindowTitle(title);
}

void NativeWidgetMac::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon) {
  // Per-window icons are not really a thing on Mac, so do nothing.
  // TODO(tapted): Investigate whether to use NSWindowDocumentIconButton to set
  // an icon next to the window title. See http://crbug.com/766897.
}

void NativeWidgetMac::InitModalType(ui::ModalType modal_type) {
  if (modal_type == ui::MODAL_TYPE_NONE)
    return;

  // System modal windows not implemented (or used) on Mac.
  DCHECK_NE(ui::MODAL_TYPE_SYSTEM, modal_type);

  // A peculiarity of the constrained window framework is that it permits a
  // dialog of MODAL_TYPE_WINDOW to have a null parent window; falling back to
  // a non-modal window in this case.
  DCHECK(ns_window_host_->parent() || modal_type == ui::MODAL_TYPE_WINDOW);

  // Everything happens upon show.
}

gfx::Rect NativeWidgetMac::GetWindowBoundsInScreen() const {
  return ns_window_host_ ? ns_window_host_->GetWindowBoundsInScreen()
                         : gfx::Rect();
}

gfx::Rect NativeWidgetMac::GetClientAreaBoundsInScreen() const {
  return ns_window_host_ ? ns_window_host_->GetContentBoundsInScreen()
                         : gfx::Rect();
}

gfx::Rect NativeWidgetMac::GetRestoredBounds() const {
  return ns_window_host_ ? ns_window_host_->GetRestoredBounds() : gfx::Rect();
}

std::string NativeWidgetMac::GetWorkspace() const {
  return ns_window_host_ ? base::Base64Encode(
                               ns_window_host_->GetWindowStateRestorationData())
                         : std::string();
}

void NativeWidgetMac::SetBounds(const gfx::Rect& bounds) {
  if (ns_window_host_)
    ns_window_host_->SetBounds(bounds);
}

void NativeWidgetMac::SetBoundsConstrained(const gfx::Rect& bounds) {
  if (!ns_window_host_)
    return;
  gfx::Rect new_bounds(bounds);
  if (ns_window_host_->parent()) {
    new_bounds.AdjustToFit(
        gfx::Rect(ns_window_host_->parent()->GetWindowBoundsInScreen().size()));
  } else {
    new_bounds = ConstrainBoundsToDisplayWorkArea(new_bounds);
  }
  SetBounds(new_bounds);
}

void NativeWidgetMac::SetSize(const gfx::Size& size) {
  // Ensure the top-left corner stays in-place (rather than the bottom-left,
  // which -[NSWindow setContentSize:] would do).
  SetBounds(gfx::Rect(GetWindowBoundsInScreen().origin(), size));
}

void NativeWidgetMac::StackAbove(gfx::NativeView native_view) {
  if (!GetNSWindowMojo())
    return;

  auto* sibling_host =
      NativeWidgetMacNSWindowHost::GetFromNativeView(native_view);

  if (!sibling_host) {
    // This will only work if |this| is in-process.
    DCHECK(!ns_window_host_->application_host());
    NSInteger view_parent = native_view.GetNativeNSView().window.windowNumber;
    [GetNativeWindow().GetNativeNSWindow() orderWindow:NSWindowAbove
                                            relativeTo:view_parent];
    return;
  }

  if (ns_window_host_->application_host() == sibling_host->application_host()) {
    // Check if |native_view|'s NativeWidgetMacNSWindowHost corresponds to the
    // same process as |this|.
    GetNSWindowMojo()->StackAbove(sibling_host->bridged_native_widget_id());
    return;
  }

  NOTREACHED() << "|native_view|'s NativeWidgetMacNSWindowHost isn't same "
                  "process |this|";
}

void NativeWidgetMac::StackAtTop() {
  if (GetNSWindowMojo())
    GetNSWindowMojo()->StackAtTop();
}

void NativeWidgetMac::SetShape(std::unique_ptr<Widget::ShapeRects> shape) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Close() {
  if (GetNSWindowMojo())
    GetNSWindowMojo()->CloseWindow();
}

void NativeWidgetMac::CloseNow() {
  if (ns_window_host_)
    ns_window_host_->CloseWindowNow();
  // Note: |ns_window_host_| will be deleted here, and |this| will be deleted
  // here when ownership_ == NATIVE_WIDGET_OWNS_WIDGET,
}

void NativeWidgetMac::Show(ui::WindowShowState show_state,
                           const gfx::Rect& restore_bounds) {
  if (!GetNSWindowMojo())
    return;

  switch (show_state) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL:
    case ui::SHOW_STATE_INACTIVE:
    case ui::SHOW_STATE_MINIMIZED:
      break;
    case ui::SHOW_STATE_MAXIMIZED:
    case ui::SHOW_STATE_FULLSCREEN:
      NOTIMPLEMENTED();
      break;
    case ui::SHOW_STATE_END:
      NOTREACHED();
      break;
  }
  auto window_state = WindowVisibilityState::kShowAndActivateWindow;
  if (show_state == ui::SHOW_STATE_INACTIVE)
    window_state = WindowVisibilityState::kShowInactive;
  else if (show_state == ui::SHOW_STATE_MINIMIZED)
    window_state = WindowVisibilityState::kHideWindow;
  GetNSWindowMojo()->SetVisibilityState(window_state);

  // Ignore the SetInitialFocus() result. BridgedContentView should get
  // firstResponder status regardless.
  delegate_->SetInitialFocus(show_state);
}

void NativeWidgetMac::Hide() {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetVisibilityState(WindowVisibilityState::kHideWindow);
}

bool NativeWidgetMac::IsVisible() const {
  return ns_window_host_ && ns_window_host_->IsVisible();
}

void NativeWidgetMac::Activate() {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetVisibilityState(
      WindowVisibilityState::kShowAndActivateWindow);
}

void NativeWidgetMac::Deactivate() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsActive() const {
  return ns_window_host_ ? ns_window_host_->IsWindowKey() : false;
}

void NativeWidgetMac::SetZOrderLevel(ui::ZOrderLevel order) {
  if (!GetNSWindowMojo())
    return;
  z_order_level_ = order;
  GetNSWindowMojo()->SetWindowLevel(CGWindowLevelForZOrderLevel(order, type_));
}

ui::ZOrderLevel NativeWidgetMac::GetZOrderLevel() const {
  return z_order_level_;
}

void NativeWidgetMac::SetVisibleOnAllWorkspaces(bool always_visible) {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetVisibleOnAllSpaces(always_visible);
}

bool NativeWidgetMac::IsVisibleOnAllWorkspaces() const {
  return false;
}

void NativeWidgetMac::Maximize() {
  NOTIMPLEMENTED();  // See IsMaximized().
}

void NativeWidgetMac::Minimize() {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetMiniaturized(true);
}

bool NativeWidgetMac::IsMaximized() const {
  // The window frame isn't altered on Mac unless going fullscreen. The green
  // "+" button just makes the window bigger. So, always false.
  return false;
}

bool NativeWidgetMac::IsMinimized() const {
  if (!ns_window_host_)
    return false;
  return ns_window_host_->IsMiniaturized();
}

void NativeWidgetMac::Restore() {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetFullscreen(false);
  GetNSWindowMojo()->SetMiniaturized(false);
}

void NativeWidgetMac::SetFullscreen(bool fullscreen) {
  if (!ns_window_host_)
    return;
  ns_window_host_->SetFullscreen(fullscreen);
}

bool NativeWidgetMac::IsFullscreen() const {
  return ns_window_host_ && ns_window_host_->target_fullscreen_state();
}

void NativeWidgetMac::SetCanAppearInExistingFullscreenSpaces(
    bool can_appear_in_existing_fullscreen_spaces) {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetCanAppearInExistingFullscreenSpaces(
      can_appear_in_existing_fullscreen_spaces);
}

void NativeWidgetMac::SetOpacity(float opacity) {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetOpacity(opacity);
}

void NativeWidgetMac::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetContentAspectRatio(aspect_ratio);
}

void NativeWidgetMac::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::RunShellDrag(View* view,
                                   std::unique_ptr<ui::OSExchangeData> data,
                                   const gfx::Point& location,
                                   int operation,
                                   ui::DragDropTypes::DragEventSource source) {
  ns_window_host_->drag_drop_client()->StartDragAndDrop(view, std::move(data),
                                                        operation, source);
}

void NativeWidgetMac::SchedulePaintInRect(const gfx::Rect& rect) {
  // |rect| is relative to client area of the window.
  NSWindow* window = GetNativeWindow().GetNativeNSWindow();
  NSRect client_rect = [window contentRectForFrameRect:[window frame]];
  NSRect target_rect = rect.ToCGRect();

  // Convert to Appkit coordinate system (origin at bottom left).
  target_rect.origin.y =
      NSHeight(client_rect) - target_rect.origin.y - NSHeight(target_rect);
  [GetNativeView().GetNativeNSView() setNeedsDisplayInRect:target_rect];
  if (ns_window_host_ && ns_window_host_->layer())
    ns_window_host_->layer()->SchedulePaint(rect);
}

void NativeWidgetMac::ScheduleLayout() {
  ui::Compositor* compositor = GetCompositor();
  if (compositor)
    compositor->ScheduleDraw();
}

void NativeWidgetMac::SetCursor(gfx::NativeCursor cursor) {
  if (GetInProcessNSWindowBridge())
    GetInProcessNSWindowBridge()->SetCursor(cursor);
}

void NativeWidgetMac::ShowEmojiPanel() {
  // We must plumb the call to ui::ShowEmojiPanel() over the bridge so that it
  // is called from the correct process.
  if (GetNSWindowMojo())
    GetNSWindowMojo()->ShowEmojiPanel();
}

bool NativeWidgetMac::IsMouseEventsEnabled() const {
  // On platforms with touch, mouse events get disabled and calls to this method
  // can affect hover states. Since there is no touch on desktop Mac, this is
  // always true. Touch on Mac is tracked in http://crbug.com/445520.
  return true;
}

bool NativeWidgetMac::IsMouseButtonDown() const {
  return [NSEvent pressedMouseButtons] != 0;
}

void NativeWidgetMac::ClearNativeFocus() {
  // To quote DesktopWindowTreeHostX11, "This method is weird and misnamed."
  // The goal is to set focus to the content window, thereby removing focus from
  // any NSView in the window that doesn't belong to toolkit-views.
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->MakeFirstResponder();
}

gfx::Rect NativeWidgetMac::GetWorkAreaBoundsInScreen() const {
  return ns_window_host_ ? ns_window_host_->GetCurrentDisplay().work_area()
                         : gfx::Rect();
}

Widget::MoveLoopResult NativeWidgetMac::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  if (!GetInProcessNSWindowBridge())
    return Widget::MOVE_LOOP_CANCELED;

  ReleaseCapture();
  return GetInProcessNSWindowBridge()->RunMoveLoop(drag_offset)
             ? Widget::MOVE_LOOP_SUCCESSFUL
             : Widget::MOVE_LOOP_CANCELED;
}

void NativeWidgetMac::EndMoveLoop() {
  if (GetInProcessNSWindowBridge())
    GetInProcessNSWindowBridge()->EndMoveLoop();
}

void NativeWidgetMac::SetVisibilityChangedAnimationsEnabled(bool value) {
  if (GetNSWindowMojo())
    GetNSWindowMojo()->SetAnimationEnabled(value);
}

void NativeWidgetMac::SetVisibilityAnimationDuration(
    const base::TimeDelta& duration) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetVisibilityAnimationTransition(
    Widget::VisibilityTransition widget_transitions) {
  remote_cocoa::mojom::VisibilityTransition transitions =
      remote_cocoa::mojom::VisibilityTransition::kNone;
  switch (widget_transitions) {
    case Widget::ANIMATE_NONE:
      transitions = remote_cocoa::mojom::VisibilityTransition::kNone;
      break;
    case Widget::ANIMATE_SHOW:
      transitions = remote_cocoa::mojom::VisibilityTransition::kShow;
      break;
    case Widget::ANIMATE_HIDE:
      transitions = remote_cocoa::mojom::VisibilityTransition::kHide;
      break;
    case Widget::ANIMATE_BOTH:
      transitions = remote_cocoa::mojom::VisibilityTransition::kBoth;
      break;
  }
  if (GetNSWindowMojo())
    GetNSWindowMojo()->SetTransitionsToAnimate(transitions);
}

bool NativeWidgetMac::IsTranslucentWindowOpacitySupported() const {
  return false;
}

ui::GestureRecognizer* NativeWidgetMac::GetGestureRecognizer() {
  static base::NoDestructor<ui::GestureRecognizerImplMac> recognizer;
  return recognizer.get();
}

void NativeWidgetMac::OnSizeConstraintsChanged() {
  Widget* widget = GetWidget();
  GetNSWindowMojo()->SetSizeConstraints(
      widget->GetMinimumSize(), widget->GetMaximumSize(),
      widget->widget_delegate()->CanResize(),
      widget->widget_delegate()->CanMaximize());
}

std::string NativeWidgetMac::GetName() const {
  return name_;
}

// static
void NativeWidgetMac::SetInitNativeWidgetCallback(
    base::RepeatingCallback<void(NativeWidgetMac*)> callback) {
  DCHECK(!g_init_native_widget_callback || callback.is_null());
  if (callback.is_null()) {
    if (g_init_native_widget_callback) {
      delete g_init_native_widget_callback;
      g_init_native_widget_callback = nullptr;
    }
    return;
  }
  g_init_native_widget_callback =
      new base::RepeatingCallback<void(NativeWidgetMac*)>(std::move(callback));
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, protected:

NativeWidgetMacNSWindow* NativeWidgetMac::CreateNSWindow(
    const remote_cocoa::mojom::CreateWindowParams* params) {
  return remote_cocoa::NativeWidgetNSWindowBridge::CreateNSWindow(params)
      .autorelease();
}

remote_cocoa::ApplicationHost*
NativeWidgetMac::GetRemoteCocoaApplicationHost() {
  return nullptr;
}

remote_cocoa::mojom::NativeWidgetNSWindow* NativeWidgetMac::GetNSWindowMojo()
    const {
  return ns_window_host_ ? ns_window_host_->GetNSWindowMojo() : nullptr;
}

remote_cocoa::NativeWidgetNSWindowBridge*
NativeWidgetMac::GetInProcessNSWindowBridge() const {
  return ns_window_host_ ? ns_window_host_->GetInProcessNSWindowBridge()
                         : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
void Widget::CloseAllSecondaryWidgets() {
  NSArray* starting_windows = [NSApp windows];  // Creates an autoreleased copy.
  for (NSWindow* window in starting_windows) {
    // Ignore any windows that couldn't have been created by NativeWidgetMac or
    // a subclass. GetNativeWidgetForNativeWindow() will later interrogate the
    // NSWindow delegate, but we can't trust that delegate to be a valid object.
    if (![window isKindOfClass:[NativeWidgetMacNSWindow class]])
      continue;

    // Record a crash key to detect when client code may destroy a
    // WidgetObserver without removing it (possibly leaking the Widget).
    // A crash can occur in generic Widget teardown paths when trying to notify.
    // See http://crbug.com/808318.
    static crash_reporter::CrashKeyString<256> window_info_key("windowInfo");
    std::string value = base::SysNSStringToUTF8(
        [NSString stringWithFormat:@"Closing %@ (%@)", [window title],
                                   [window className]]);
    crash_reporter::ScopedCrashKeyString scopedWindowKey(&window_info_key,
                                                         value);

    Widget* widget = GetWidgetForNativeWindow(window);
    if (widget && widget->is_secondary_widget())
      [window close];
  }
}

const ui::NativeTheme* Widget::GetNativeTheme() const {
  return ui::NativeTheme::GetInstanceForNativeUi();
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetMac(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  return GetNativeWidgetForNativeWindow([native_view.GetNativeNSView() window]);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow window) {
  if (NativeWidgetMacNSWindowHost* ns_window_host_impl =
          NativeWidgetMacNSWindowHost::GetFromNativeWindow(window)) {
    return ns_window_host_impl->native_widget_mac();
  }
  return nullptr;  // Not created by NativeWidgetMac.
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  NativeWidgetMacNSWindowHost* window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeView(native_view);
  if (!window_host)
    return nullptr;
  while (window_host->parent())
    window_host = window_host->parent();
  return window_host->native_widget_mac();
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  NativeWidgetMacNSWindowHost* window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeView(native_view);
  if (!window_host) {
    NSView* ns_view = native_view.GetNativeNSView();
    // The NSWindow is not itself a views::Widget, but it may have children that
    // are. Support returning Widgets that are parented to the NSWindow, except:
    // - Ignore requests for children of an NSView that is not a contentView.
    // - We do not add a Widget for |native_view| to |children| (there is none).
    if ([[ns_view window] contentView] != ns_view)
      return;

    // Collect -sheets and -childWindows. A window should never appear in both,
    // since that causes AppKit to glitch.
    NSArray* sheet_children = [[ns_view window] sheets];
    for (NSWindow* native_child in sheet_children)
      GetAllChildWidgets([native_child contentView], children);

    for (NSWindow* native_child in [[ns_view window] childWindows]) {
      DCHECK(![sheet_children containsObject:native_child]);
      GetAllChildWidgets([native_child contentView], children);
    }
    return;
  }

  // If |native_view| is a subview of the contentView, it will share an
  // NSWindow, but will itself be a native child of the Widget. That is, adding
  // window_host->..->GetWidget() to |children| would be adding the _parent_ of
  // |native_view|, not the Widget for |native_view|. |native_view| doesn't have
  // a corresponding Widget of its own in this case (and so can't have Widget
  // children of its own on Mac).
  if (window_host->native_widget_mac()->GetNativeView() != native_view)
    return;

  // Code expects widget for |native_view| to be added to |children|.
  if (window_host->native_widget_mac()->GetWidget())
    children->insert(window_host->native_widget_mac()->GetWidget());

  // When the NSWindow *is* a Widget, only consider children(). I.e. do not
  // look through -[NSWindow childWindows] as done for the (!window_host) case
  // above. -childWindows does not support hidden windows, and anything in there
  // which is not in children() would have been added by AppKit.
  for (NativeWidgetMacNSWindowHost* child : window_host->children())
    GetAllChildWidgets(child->native_widget_mac()->GetNativeView(), children);
}

// static
void NativeWidgetPrivate::GetAllOwnedWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* owned) {
  NativeWidgetMacNSWindowHost* window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeView(native_view);
  if (!window_host) {
    GetAllChildWidgets(native_view, owned);
    return;
  }
  if (window_host->native_widget_mac()->GetNativeView() != native_view)
    return;
  for (NativeWidgetMacNSWindowHost* child : window_host->children())
    GetAllChildWidgets(child->native_widget_mac()->GetNativeView(), owned);
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  DCHECK_NE(native_view, new_parent);
  DCHECK([new_parent.GetNativeNSView() window]);
  if (!new_parent || [native_view.GetNativeNSView() superview] ==
                         new_parent.GetNativeNSView()) {
    NOTREACHED();
    return;
  }

  NativeWidgetMacNSWindowHost* window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeView(native_view);
  DCHECK(window_host);
  gfx::NativeView bridge_view =
      window_host->native_widget_mac()->GetNativeView();
  gfx::NativeWindow bridge_window =
      window_host->native_widget_mac()->GetNativeWindow();
  bool bridge_is_top_level =
      window_host->native_widget_mac()->GetWidget()->is_top_level();
  DCHECK([native_view.GetNativeNSView()
      isDescendantOf:bridge_view.GetNativeNSView()]);
  DCHECK(bridge_window && ![bridge_window.GetNativeNSWindow() isSheet]);

  NativeWidgetMacNSWindowHost* parent_window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeView(new_parent);

  // Early out for no-op changes.
  if (native_view == bridge_view && bridge_is_top_level &&
      window_host->parent() == parent_window_host) {
    return;
  }

  // First notify all the widgets that they are being disassociated from their
  // previous parent.
  Widget::Widgets widgets;
  GetAllChildWidgets(native_view, &widgets);
  for (auto* child : widgets)
    child->NotifyNativeViewHierarchyWillChange();

  // Update |bridge_host|'s parent only if
  // NativeWidgetNSWindowBridge::ReparentNativeView will.
  if (native_view == bridge_view) {
    window_host->SetParent(parent_window_host);
    if (!bridge_is_top_level) {
      // Make |window_host|'s NSView be a child of |new_parent| by adding it as
      // a subview. Note that this will have the effect of removing
      // |window_host|'s NSView from its NSWindow. The |NSWindow| must remain
      // visible because it controls the bounds and visibility of the ui::Layer,
      // so just hide it by setting alpha value to zero.
      // TODO(ccameron): This path likely violates assumptions. Verify that this
      // path is unused and remove it.
      LOG(ERROR) << "Reparenting a non-top-level BridgedNativeWidget. This is "
                    "likely unsupported.";
      [new_parent.GetNativeNSView() addSubview:native_view.GetNativeNSView()];
      [bridge_window.GetNativeNSWindow() setAlphaValue:0];
      [bridge_window.GetNativeNSWindow() setIgnoresMouseEvents:YES];
    }
  } else {
    // TODO(ccameron): This path likely violates assumptions. Verify that this
    // path is unused and remove it.
    LOG(ERROR) << "Reparenting with a non-root BridgedNativeWidget NSView. "
                  "This is likely unsupported.";
    [new_parent.GetNativeNSView() addSubview:native_view.GetNativeNSView()];
  }

  // And now, notify them that they have a brand new parent.
  for (auto* child : widgets)
    child->NotifyNativeViewHierarchyChanged();
}

// static
gfx::NativeView NativeWidgetPrivate::GetGlobalCapture(
    gfx::NativeView native_view) {
  return NativeWidgetMacNSWindowHost::GetGlobalCaptureView();
}

}  // namespace internal
}  // namespace views
