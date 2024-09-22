// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/common/crash_key.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#import "components/remote_cocoa/app_shim/views_nswindow_delegate.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_recognizer_impl_mac.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/font_list.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_mac.h"
#import "ui/views/cocoa/drag_drop_client_mac.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/cocoa/text_input_host.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/native_frame_view_mac.h"

using remote_cocoa::mojom::WindowVisibilityState;

namespace views {

namespace {

base::LazyInstance<base::RepeatingCallbackList<void(NativeWidgetMac*)>>::
    DestructorAtExit g_init_native_widget_callbacks = LAZY_INSTANCE_INITIALIZER;

uint64_t StyleMaskForParams(const Widget::InitParams& params) {
  // If the Widget is modal, it will be displayed as a sheet. This works best if
  // it has NSWindowStyleMaskTitled. For example, with
  // NSWindowStyleMaskBorderless, the parent window still accepts input.
  // NSWindowStyleMaskFullSizeContentView ensures that calculating the modal's
  // content rect doesn't account for a nonexistent title bar.
  if (params.delegate &&
      params.delegate->GetModalType() == ui::mojom::ModalType::kWindow) {
    return NSWindowStyleMaskTitled | NSWindowStyleMaskFullSizeContentView;
  }

  // TODO(tapted): Determine better masks when there are use cases for it.
  if (params.remove_standard_frame)
    return NSWindowStyleMaskBorderless;

  if (params.type == Widget::InitParams::TYPE_WINDOW) {
    return NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  }
  return NSWindowStyleMaskBorderless;
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
  ~ZoomFocusMonitor() override = default;
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
// NativeWidgetMac:

NativeWidgetMac::NativeWidgetMac(internal::NativeWidgetDelegate* delegate)
    // TODO(crbug.com/346814969): Investigate where a null `delegate` should
    // be allowed.
    : delegate_(delegate ? delegate->AsWidget()->GetWeakPtr() : nullptr),
      ns_window_host_(new NativeWidgetMacNSWindowHost(this)) {}

NativeWidgetMac::~NativeWidgetMac() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    owned_delegate_.reset();
  else
    CloseNow();
}

void NativeWidgetMac::WindowDestroying() {
  OnWindowDestroying(GetNativeWindow());
  if (delegate_) {
    delegate_->OnNativeWidgetDestroying();
  }
}

void NativeWidgetMac::WindowDestroyed() {
  DCHECK(GetNSWindowMojo());
  SetFocusManager(nullptr);
  ns_window_host_.reset();
  // |OnNativeWidgetDestroyed| may delete |this| if the object does not own
  // itself.
  bool should_delete_this =
      (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) ||
      (ownership_ == Widget::InitParams::CLIENT_OWNS_WIDGET);
  if (delegate_) {
    delegate_->OnNativeWidgetDestroyed();
  }
  if (should_delete_this)
    delete this;
}

void NativeWidgetMac::OnWindowKeyStatusChanged(
    bool is_key,
    bool is_content_first_responder) {
  Widget* widget = GetWidget();
  if (!widget || !widget->OnNativeWidgetActivationChanged(is_key)) {
    return;
  }
  // The contentView is the BridgedContentView hosting the views::RootView. The
  // focus manager will already know if a native subview has focus.
  if (!is_content_first_responder)
    return;

  if (is_key) {
    widget->OnNativeFocus();
    widget->GetFocusManager()->RestoreFocusedView();
  } else {
    widget->OnNativeBlur();
    widget->GetFocusManager()->StoreFocusedView(true);
    parent_key_lock_.reset();
  }
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

bool NativeWidgetMac::WillExecuteCommand(
    int32_t command,
    WindowOpenDisposition window_open_disposition,
    bool is_before_first_responder) {
  // This is supported only by subclasses in chrome/browser/ui.
  NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetMac::ExecuteCommand(
    int32_t command,
    WindowOpenDisposition window_open_disposition,
    bool is_before_first_responder) {
  // This is supported only by subclasses in chrome/browser/ui.
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::InitNativeWidget(Widget::InitParams params) {
  ownership_ = params.ownership;
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
    owned_delegate_ = base::WrapUnique(delegate_.get());
  }
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
    NativeWidgetMacNSWindow* window =
        CreateNSWindow(create_window_params.get());
    ns_window_host_->CreateInProcessNSWindowBridge(window);
  }

  // If the z-order wasn't specifically set to something other than `kNormal`,
  // then override it if it would leave the widget z-ordered incorrectly in some
  // platform-specific corner cases.
  if (params.parent &&
      (!params.z_order || params.z_order == ui::ZOrderLevel::kNormal)) {
    if (auto* parent_widget = Widget::GetWidgetForNativeView(params.parent)) {
      // If our parent is z-ordered above us, then float a bit higher.
      params.z_order =
          std::max(params.z_order.value_or(params.EffectiveZOrderLevel()),
                   parent_widget->GetZOrderLevel());
    }
  }

  ns_window_host_->SetParent(parent_host);
  ns_window_host_->InitWindow(params,
                              ConvertBoundsToScreenIfNeeded(params.bounds));

  OnWindowInitialized();

  // Only set the z-order here if it is non-default since setting it may affect
  // how the window is treated by Expose.
  if (params.EffectiveZOrderLevel() != ui::ZOrderLevel::kNormal)
    SetZOrderLevel(params.EffectiveZOrderLevel());

  GetNSWindowMojo()->SetIgnoresMouseEvents(!params.accept_events);
  GetNSWindowMojo()->SetVisibleOnAllSpaces(params.visible_on_all_workspaces);

  delegate_->OnNativeWidgetCreated();

  DCHECK(GetWidget()->GetRootView());
  ns_window_host_->SetRootView(GetWidget()->GetRootView());
  GetNSWindowMojo()->CreateContentView(ns_window_host_->GetRootViewNSViewId(),
                                       GetWidget()->GetRootView()->bounds());
  if (auto* focus_manager = GetWidget()->GetFocusManager()) {
    GetNSWindowMojo()->MakeFirstResponder();
    // Only one ZoomFocusMonitor is needed per FocusManager, so create one only
    // for top-level widgets.
    if (GetWidget()->is_top_level())
      zoom_focus_monitor_ = std::make_unique<ZoomFocusMonitor>();
    SetFocusManager(focus_manager);
  }
  ns_window_host_->CreateCompositor(params);

  g_init_native_widget_callbacks.Get().Notify(this);
}

void NativeWidgetMac::OnWidgetInitDone() {
  OnSizeConstraintsChanged();
  ns_window_host_->OnWidgetInitDone();
}

void NativeWidgetMac::ReparentNativeViewImpl(gfx::NativeView new_parent) {
  gfx::NativeView child = GetNativeView();
  DCHECK_NE(child, new_parent);
  DCHECK([new_parent.GetNativeNSView() window]);
  CHECK(new_parent);
  CHECK_NE([child.GetNativeNSView() superview], new_parent.GetNativeNSView());

  NativeWidgetMacNSWindowHost* child_window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeView(child);
  DCHECK(child_window_host);
  gfx::NativeView widget_view =
      child_window_host->native_widget_mac()->GetNativeView();
  DCHECK_EQ(child, widget_view);
  gfx::NativeWindow widget_window =
      child_window_host->native_widget_mac()->GetNativeWindow();
  DCHECK(
      [child.GetNativeNSView() isDescendantOf:widget_view.GetNativeNSView()]);
  DCHECK(widget_window && ![widget_window.GetNativeNSWindow() isSheet]);

  NativeWidgetMacNSWindowHost* parent_window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeView(new_parent);

  // Early out for no-op changes.
  if (child == widget_view &&
      child_window_host->parent() == parent_window_host) {
    return;
  }

  // First notify all the widgets that they are being disassociated from their
  // previous parent.
  Widget::Widgets widgets;
  GetAllChildWidgets(child, &widgets);
  for (Widget* widget : widgets) {
    widget->NotifyNativeViewHierarchyWillChange();
  }

  child_window_host->SetParent(parent_window_host);

  // And now, notify them that they have a brand new parent.
  for (Widget* widget : widgets) {
    widget->NotifyNativeViewHierarchyChanged();
  }
}

std::unique_ptr<NonClientFrameView>
NativeWidgetMac::CreateNonClientFrameView() {
  return GetWidget() ? std::make_unique<NativeFrameViewMac>(GetWidget())
                     : nullptr;
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
  if (!GetWidget()) {
    return;
  }
  // This is called when the Theme has changed; forward the event to the root
  // widget.
  GetWidget()->ThemeChanged();
  GetWidget()->GetRootView()->SchedulePaint();
}

Widget* NativeWidgetMac::GetWidget() {
  return delegate_ ? delegate_->AsWidget() : nullptr;
}

const Widget* NativeWidgetMac::GetWidget() const {
  return delegate_ ? delegate_->AsWidget() : nullptr;
}

gfx::NativeView NativeWidgetMac::GetNativeView() const {
  // When a widget becomes a subwidget, its contentView moves to an another
  // NSWindow. When this happens, the window's contentView will be nil.
  // Return the cached original contentView instead.
  NSView* contentView = (__bridge NSView*)GetNativeWindowProperty(
      views::NativeWidgetMacNSWindowHost::kMovedContentNSView);
  if (contentView) {
    return gfx::NativeView(contentView);
  }
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
  if (!input_method_) {
    input_method_ = ui::CreateInputMethod(this, gfx::kNullAcceleratedWidget);
    // For now, use always-focused mode on Mac for the input method.
    // TODO(tapted): Move this to OnWindowKeyStatusChangedTo() and balance.
    input_method_->OnFocus();
  }
  return input_method_.get();
}

void NativeWidgetMac::CenterWindow(const gfx::Size& size) {
  if (GetNSWindowMojo() && GetWidget()) {
    GetNSWindowMojo()->SetSizeAndCenter(size, GetWidget()->GetMinimumSize());
  }
}

void NativeWidgetMac::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  *bounds = GetRestoredBounds();
  if (IsFullscreen())
    *show_state = ui::mojom::WindowShowState::kFullscreen;
  else if (IsMinimized())
    *show_state = ui::mojom::WindowShowState::kMinimized;
  else
    *show_state = ui::mojom::WindowShowState::kNormal;
}

bool NativeWidgetMac::SetWindowTitle(const std::u16string& title) {
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

void NativeWidgetMac::InitModalType(ui::mojom::ModalType modal_type) {
  if (modal_type == ui::mojom::ModalType::kNone) {
    return;
  }

  // System modal windows not implemented (or used) on Mac.
  DCHECK_NE(ui::mojom::ModalType::kSystem, modal_type);

  // A peculiarity of the constrained window framework is that it permits a
  // dialog of MODAL_TYPE_WINDOW to have a null parent window; falling back to
  // a non-modal window in this case.
  DCHECK(ns_window_host_->parent() ||
         modal_type == ui::mojom::ModalType::kWindow);

  // Everything happens upon show.
}

const gfx::ImageSkia* NativeWidgetMac::GetWindowIcon() {
  return nullptr;
}
const gfx::ImageSkia* NativeWidgetMac::GetWindowAppIcon() {
  return nullptr;
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

gfx::Rect NativeWidgetMac::ConvertBoundsToScreenIfNeeded(
    const gfx::Rect& bounds) const {
  // If there isn't a parent widget, then bounds cannot be relative to the
  // parent.
  if (!ns_window_host_ || !ns_window_host_->parent() || !GetWidget())
    return bounds;

  // Replicate the logic in desktop_aura/desktop_screen_position_client.cc.
  if (GetAuraWindowTypeForWidgetType(type_) ==
          aura::client::WINDOW_TYPE_POPUP ||
      GetWidget()->is_top_level()) {
    return bounds;
  }

  // Empty bounds are only allowed to be specified at initialization and are
  // expected not to be translated.
  if (bounds.IsEmpty())
    return bounds;

  gfx::Rect bounds_in_screen = bounds;
  bounds_in_screen.Offset(
      ns_window_host_->parent()->GetWindowBoundsInScreen().OffsetFromOrigin());
  return bounds_in_screen;
}

void NativeWidgetMac::SetBounds(const gfx::Rect& bounds) {
  if (!ns_window_host_)
    return;
  ns_window_host_->SetBoundsInScreen(ConvertBoundsToScreenIfNeeded(bounds));
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
  if (!ns_window_host_)
    return;
  ns_window_host_->SetSize(size);
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

  CHECK_EQ(ns_window_host_->application_host(),
           sibling_host->application_host())
      << "|native_view|'s NativeWidgetMacNSWindowHost isn't same "
         "process |this|";
  // Check if |native_view|'s NativeWidgetMacNSWindowHost corresponds to the
  // same process as |this|.
  GetNSWindowMojo()->StackAbove(sibling_host->bridged_native_widget_id());
  return;
}

void NativeWidgetMac::StackAtTop() {
  if (GetNSWindowMojo())
    GetNSWindowMojo()->StackAtTop();
}

bool NativeWidgetMac::IsStackedAbove(gfx::NativeView native_view) {
  if (!GetNSWindowMojo())
    return false;

  // -[NSApplication orderedWindows] are ordered front-to-back.
  NSWindow* first = GetNativeWindow().GetNativeNSWindow();
  NSWindow* second = [native_view.GetNativeNSView() window];

  for (NSWindow* window in [NSApp orderedWindows]) {
    if (window == second)
      return !first;

    if (window == first)
      first = nil;
  }

  return false;
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

void NativeWidgetMac::Show(ui::mojom::WindowShowState show_state,
                           const gfx::Rect& restore_bounds) {
  if (!GetNSWindowHost() || !delegate_) {
    return;
  }

  switch (show_state) {
    case ui::mojom::WindowShowState::kDefault:
    case ui::mojom::WindowShowState::kNormal:
    case ui::mojom::WindowShowState::kInactive:
    case ui::mojom::WindowShowState::kMinimized:
      break;
    case ui::mojom::WindowShowState::kMaximized:
    case ui::mojom::WindowShowState::kFullscreen:
      NOTIMPLEMENTED();
      break;
    case ui::mojom::WindowShowState::kEnd:
      NOTREACHED();
  }
  auto window_state = WindowVisibilityState::kShowAndActivateWindow;
  if (show_state == ui::mojom::WindowShowState::kInactive) {
    window_state = WindowVisibilityState::kShowInactive;
  } else if (show_state == ui::mojom::WindowShowState::kMinimized) {
    window_state = WindowVisibilityState::kMiniaturizeWindow;
  } else if (show_state == ui::mojom::WindowShowState::kDefault) {
    window_state = delegate_->CanActivate()
                       ? window_state
                       : WindowVisibilityState::kShowInactive;
  }
  GetNSWindowHost()->SetVisibilityState(window_state);

  // Ignore the SetInitialFocus() result. BridgedContentView should get
  // firstResponder status regardless.
  delegate_->SetInitialFocus(show_state);
}

void NativeWidgetMac::Hide() {
  if (!GetNSWindowHost()) {
    return;
  }
  GetNSWindowHost()->SetVisibilityState(WindowVisibilityState::kHideWindow);
}

bool NativeWidgetMac::IsVisible() const {
  return ns_window_host_ && ns_window_host_->IsVisible();
}

void NativeWidgetMac::Activate() {
  if (!GetNSWindowHost()) {
    return;
  }
  GetNSWindowHost()->SetVisibilityState(
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
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetZoomed(true);
}

void NativeWidgetMac::Minimize() {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetMiniaturized(true);
}

bool NativeWidgetMac::IsMaximized() const {
  if (!ns_window_host_)
    return false;
  return ns_window_host_->IsZoomed();
}

bool NativeWidgetMac::IsMinimized() const {
  if (!ns_window_host_)
    return false;
  return ns_window_host_->IsMiniaturized();
}

void NativeWidgetMac::Restore() {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->ExitFullscreen();
  GetNSWindowMojo()->SetMiniaturized(false);
  GetNSWindowMojo()->SetZoomed(false);
}

void NativeWidgetMac::SetFullscreen(bool fullscreen,
                                    int64_t target_display_id) {
  if (!ns_window_host_)
    return;
  ns_window_host_->SetFullscreen(fullscreen, target_display_id);
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

void NativeWidgetMac::SetAspectRatio(const gfx::SizeF& aspect_ratio,
                                     const gfx::Size& excluded_margin) {
  if (!GetNSWindowMojo())
    return;
  GetNSWindowMojo()->SetAspectRatio(aspect_ratio, excluded_margin);
}

void NativeWidgetMac::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::RunShellDrag(std::unique_ptr<ui::OSExchangeData> data,
                                   const gfx::Point& location,
                                   int operation,
                                   ui::mojom::DragEventSource source) {
  if (!ns_window_host_)
    return;
  ns_window_host_->drag_drop_client()->StartDragAndDrop(std::move(data),
                                                        operation, source);
}

void NativeWidgetMac::CancelShellDrag(View* view) {
  if (!ns_window_host_) {
    return;
  }
  ns_window_host_->drag_drop_client()->EndDrag();
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

void NativeWidgetMac::SetCursor(const ui::Cursor& cursor) {
  if (GetNSWindowMojo())
    GetNSWindowMojo()->SetCursor(cursor);
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
    return Widget::MoveLoopResult::kCanceled;

  ReleaseCapture();
  return GetInProcessNSWindowBridge()->RunMoveLoop(drag_offset)
             ? Widget::MoveLoopResult::kSuccessful
             : Widget::MoveLoopResult::kCanceled;
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

ui::GestureRecognizer* NativeWidgetMac::GetGestureRecognizer() {
  static base::NoDestructor<ui::GestureRecognizerImplMac> recognizer;
  return recognizer.get();
}

ui::GestureConsumer* NativeWidgetMac::GetGestureConsumer() {
  NOTIMPLEMENTED();
  return nullptr;
}

void NativeWidgetMac::OnSizeConstraintsChanged() {
  if (!GetNSWindowMojo() || !GetWidget()) {
    return;
  }

  Widget* widget = GetWidget();
  GetNSWindowMojo()->SetSizeConstraints(
      widget->GetMinimumSize(), widget->GetMaximumSize(),
      widget->widget_delegate()->CanResize(),
      widget->widget_delegate()->CanMaximize());
}

void NativeWidgetMac::OnNativeViewHierarchyWillChange() {
  // If this is not top-level, then the FocusManager may change, so remove our
  // listeners.
  if (GetWidget() && !GetWidget()->is_top_level()) {
    SetFocusManager(nullptr);
  }
  parent_key_lock_.reset();
}

void NativeWidgetMac::OnNativeViewHierarchyChanged() {
  if (GetWidget() && !GetWidget()->is_top_level()) {
    SetFocusManager(GetWidget()->GetFocusManager());
  }
}

bool NativeWidgetMac::SetAllowScreenshots(bool allow) {
  if (ns_window_host_) {
    ns_window_host_->SetAllowScreenshots(allow);
    return true;
  }
  return false;
}

bool NativeWidgetMac::AreScreenshotsAllowed() {
  if (ns_window_host_) {
    return ns_window_host_->AllowScreenshots();
  }
  return true;
}

std::string NativeWidgetMac::GetName() const {
  return name_;
}

base::WeakPtr<internal::NativeWidgetPrivate> NativeWidgetMac::GetWeakPtr() {
  return weak_factory.GetWeakPtr();
}

// static
base::CallbackListSubscription
NativeWidgetMac::RegisterInitNativeWidgetCallback(
    const base::RepeatingCallback<void(NativeWidgetMac*)>& callback) {
  DCHECK(!callback.is_null());
  return g_init_native_widget_callbacks.Get().Add(callback);
}

void NativeWidgetMac::PopulateCreateWindowParams(
    const Widget::InitParams& widget_params,
    remote_cocoa::mojom::CreateWindowParams* params) {
  if (widget_params.is_overlay) {
    params->window_class = remote_cocoa::mojom::WindowClass::kOverlay;
  }
}

NativeWidgetMacNSWindow* NativeWidgetMac::CreateNSWindow(
    const remote_cocoa::mojom::CreateWindowParams* params) {
  return remote_cocoa::NativeWidgetNSWindowBridge::CreateNSWindow(params);
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

void NativeWidgetMac::SetFocusManager(FocusManager* new_focus_manager) {
  if (focus_manager_) {
    if (View* old_focus = focus_manager_->GetFocusedView())
      OnDidChangeFocus(old_focus, nullptr);
    focus_manager_->RemoveFocusChangeListener(this);
    if (zoom_focus_monitor_)
      focus_manager_->RemoveFocusChangeListener(zoom_focus_monitor_.get());
  }
  focus_manager_ = new_focus_manager;
  if (focus_manager_) {
    if (View* new_focus = focus_manager_->GetFocusedView())
      OnDidChangeFocus(nullptr, new_focus);
    focus_manager_->AddFocusChangeListener(this);
    if (zoom_focus_monitor_)
      focus_manager_->AddFocusChangeListener(zoom_focus_monitor_.get());

    if (!widget_observation_.IsObserving()) {
      CHECK(GetWidget());
      widget_observation_.Observe(GetWidget());
    }
  }
}

void NativeWidgetMac::OnWillChangeFocus(View* focused_before,
                                        View* focused_now) {}

void NativeWidgetMac::OnDidChangeFocus(View* focused_before,
                                       View* focused_now) {
  ui::InputMethod* input_method = GetWidget()->GetInputMethod();
  if (!input_method)
    return;

  ui::TextInputClient* new_text_input_client =
      input_method->GetTextInputClient();
  // Sanity check: For a top level widget, when focus moves away from the widget
  // (i.e. |focused_now| is nil), then the textInputClient will be cleared.
  DCHECK(!!focused_now || !new_text_input_client ||
         !GetWidget()->is_top_level());
  if (ns_window_host_) {
    ns_window_host_->text_input_host()->SetTextInputClient(
        new_text_input_client);
  }
}

ui::EventDispatchDetails NativeWidgetMac::DispatchKeyEventPostIME(
    ui::KeyEvent* key) {
  DCHECK(focus_manager_);
  if (!focus_manager_->OnKeyEvent(*key))
    key->StopPropagation();
  else
    GetWidget()->OnKeyEvent(key);
  return ui::EventDispatchDetails();
}

void NativeWidgetMac::OnWidgetDestroyed(Widget* widget) {
  widget_observation_.Reset();
  // The `widget` owns the `FocusManager`. As such, `NativeWidgetMac` must
  // unregister itself as a focus change listener here if it hasn't done so
  // already, or it may retain a dead focus manager pointer (risking
  // use-after-free).
  SetFocusManager(nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// Widget:

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

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate:

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
  while (window_host->parent()) {
    if (!window_host->native_widget_mac()->GetWidget()) {
      return nullptr;
    }

    if (window_host->native_widget_mac()->GetWidget()->is_top_level())
      break;
    window_host = window_host->parent();
  }
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
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView child,
                                             gfx::NativeView new_parent) {
  Widget::GetWidgetForNativeView(child)
      ->native_widget_private()
      ->ReparentNativeViewImpl(new_parent);
}

// static
gfx::NativeView NativeWidgetPrivate::GetGlobalCapture(
    gfx::NativeView native_view) {
  return NativeWidgetMacNSWindowHost::GetGlobalCaptureView();
}

}  // namespace internal
}  // namespace views
