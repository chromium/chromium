// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/bridged_native_widget_host_impl.h"

#include <utility>

#include "base/mac/foundation_util.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/models/dialog_model.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/native_theme/native_theme_mac.h"
#include "ui/views/cocoa/tooltip_manager_mac.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/word_lookup_client.h"
#include "ui/views_bridge_mac/bridged_native_widget_impl.h"
#include "ui/views_bridge_mac/cocoa_mouse_capture.h"
#include "ui/views_bridge_mac/native_widget_mac_nswindow.h"

using views_bridge_mac::mojom::BridgedNativeWidgetInitParams;
using views_bridge_mac::mojom::WindowVisibilityState;

namespace views {

namespace {

// Returns true if bounds passed to window in SetBounds should be treated as
// though they are in screen coordinates.
bool PositionWindowInScreenCoordinates(Widget* widget,
                                       Widget::InitParams::Type type) {
  // Replicate the logic in desktop_aura/desktop_screen_position_client.cc.
  if (GetAuraWindowTypeForWidgetType(type) == aura::client::WINDOW_TYPE_POPUP)
    return true;

  return widget && widget->is_top_level();
}

std::map<uint64_t, BridgedNativeWidgetHostImpl*>& GetIdToWidgetHostImplMap() {
  static base::NoDestructor<std::map<uint64_t, BridgedNativeWidgetHostImpl*>>
      id_map;
  return *id_map;
}

uint64_t g_last_bridged_native_widget_id = 0;

}  // namespace

// static
BridgedNativeWidgetHostImpl* BridgedNativeWidgetHostImpl::GetFromNativeWindow(
    gfx::NativeWindow native_window) {
  NSWindow* window = native_window.GetNativeNSWindow();
  if (NativeWidgetMacNSWindow* widget_window =
          base::mac::ObjCCast<NativeWidgetMacNSWindow>(window)) {
    return GetFromId([widget_window bridgedNativeWidgetId]);
  }
  return nullptr;  // Not created by NativeWidgetMac.
}

// static
BridgedNativeWidgetHostImpl* BridgedNativeWidgetHostImpl::GetFromNativeView(
    gfx::NativeView native_view) {
  return GetFromNativeWindow([native_view.GetNativeNSView() window]);
}

// static
BridgedNativeWidgetHostImpl* BridgedNativeWidgetHostImpl::GetFromId(
    uint64_t bridged_native_widget_id) {
  auto found = GetIdToWidgetHostImplMap().find(bridged_native_widget_id);
  if (found == GetIdToWidgetHostImplMap().end())
    return nullptr;
  return found->second;
}

BridgedNativeWidgetHostImpl::BridgedNativeWidgetHostImpl(NativeWidgetMac* owner)
    : widget_id_(++g_last_bridged_native_widget_id),
      native_widget_mac_(owner),
      root_view_id_(ui::NSViewIds::GetNewId()),
      host_mojo_binding_(this) {
  DCHECK(GetIdToWidgetHostImplMap().find(widget_id_) ==
         GetIdToWidgetHostImplMap().end());
  GetIdToWidgetHostImplMap().insert(std::make_pair(widget_id_, this));
  DCHECK(owner);
}

BridgedNativeWidgetHostImpl::~BridgedNativeWidgetHostImpl() {
  DCHECK(children_.empty());
  if (bridge_factory_host_) {
    bridge_ptr_.reset();
    host_mojo_binding_.Unbind();
    bridge_factory_host_->RemoveObserver(this);
    bridge_factory_host_ = nullptr;
  }

  // Ensure that |this| cannot be reached by its id while it is being destroyed.
  auto found = GetIdToWidgetHostImplMap().find(widget_id_);
  DCHECK(found != GetIdToWidgetHostImplMap().end());
  DCHECK_EQ(found->second, this);
  GetIdToWidgetHostImplMap().erase(found);

  // Destroy the bridge first to prevent any calls back into this during
  // destruction.
  // TODO(ccameron): When all communication from |bridge_| to this goes through
  // the BridgedNativeWidgetHost, this can be replaced with closing that pipe.
  bridge_impl_.reset();
  SetFocusManager(nullptr);
  DestroyCompositor();
}

NativeWidgetMacNSWindow* BridgedNativeWidgetHostImpl::GetLocalNSWindow() const {
  return local_window_.get();
}

views_bridge_mac::mojom::BridgedNativeWidget*
BridgedNativeWidgetHostImpl::bridge() const {
  if (bridge_ptr_)
    return bridge_ptr_.get();
  if (bridge_impl_)
    return bridge_impl_.get();
  return nullptr;
}

void BridgedNativeWidgetHostImpl::CreateLocalBridge(
    base::scoped_nsobject<NativeWidgetMacNSWindow> window) {
  local_window_ = window;
  bridge_impl_ =
      std::make_unique<BridgedNativeWidgetImpl>(widget_id_, this, this);
  bridge_impl_->SetWindow(window);
}

void BridgedNativeWidgetHostImpl::CreateRemoteBridge(
    BridgeFactoryHost* bridge_factory_host,
    views_bridge_mac::mojom::CreateWindowParamsPtr window_create_params) {
  bridge_factory_host_ = bridge_factory_host;
  bridge_factory_host_->AddObserver(this);

  // Create a local invisible window that will be used as the gfx::NativeWindow
  // handle to track this window in this process.
  {
    auto local_window_create_params =
        views_bridge_mac::mojom::CreateWindowParams::New();
    local_window_create_params->style_mask = NSBorderlessWindowMask;
    local_window_ = BridgedNativeWidgetImpl::CreateNSWindow(
        local_window_create_params.get());
    [local_window_ setBridgedNativeWidgetId:widget_id_];
    [local_window_ setAlphaValue:0.0];
  }

  // Initialize |bridge_ptr_| to point to a bridge created by |factory|.
  views_bridge_mac::mojom::BridgedNativeWidgetHostAssociatedPtr host_ptr;
  host_mojo_binding_.Bind(mojo::MakeRequest(&host_ptr),
                          ui::WindowResizeHelperMac::Get()->task_runner());
  bridge_factory_host_->GetFactory()->CreateBridgedNativeWidget(
      widget_id_, mojo::MakeRequest(&bridge_ptr_), host_ptr.PassInterface());

  // Create the window in its process, and attach it to its parent window.
  bridge()->CreateWindow(std::move(window_create_params));
}

void BridgedNativeWidgetHostImpl::InitWindow(const Widget::InitParams& params) {
  Widget* widget = native_widget_mac_->GetWidget();
  // Tooltip Widgets shouldn't have their own tooltip manager, but tooltips are
  // native on Mac, so nothing should ever want one in Widget form.
  DCHECK_NE(params.type, Widget::InitParams::TYPE_TOOLTIP);
  widget_type_ = params.type;
  tooltip_manager_.reset(new TooltipManagerMac(bridge()));

  // Initialize the window.
  {
    auto bridge_params = BridgedNativeWidgetInitParams::New();
    bridge_params->modal_type = widget->widget_delegate()->GetModalType();
    bridge_params->is_translucent =
        params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW;
    bridge_params->widget_is_top_level = widget->is_top_level();
    bridge_params->position_window_in_screen_coords =
        PositionWindowInScreenCoordinates(widget, widget_type_);

    // OSX likes to put shadows on most things. However, frameless windows (with
    // styleMask = NSBorderlessWindowMask) default to no shadow. So change that.
    // SHADOW_TYPE_DROP is used for Menus, which get the same shadow style on
    // Mac.
    switch (params.shadow_type) {
      case Widget::InitParams::SHADOW_TYPE_NONE:
        bridge_params->has_window_server_shadow = false;
        break;
      case Widget::InitParams::SHADOW_TYPE_DEFAULT:
        // Controls should get views shadows instead of native shadows.
        bridge_params->has_window_server_shadow =
            params.type != Widget::InitParams::TYPE_CONTROL;
        break;
      case Widget::InitParams::SHADOW_TYPE_DROP:
        bridge_params->has_window_server_shadow = true;
        break;
    }  // No default case, to pick up new types.

    // Include "regular" windows without the standard frame in the window cycle.
    // These use NSBorderlessWindowMask so do not get it by default.
    bridge_params->force_into_collection_cycle =
        widget_type_ == Widget::InitParams::TYPE_WINDOW &&
        params.remove_standard_frame;

    bridge()->InitWindow(std::move(bridge_params));
  }

  // Set a meaningful initial bounds. Note that except for frameless widgets
  // with no WidgetDelegate, the bounds will be set again by Widget after
  // initializing the non-client view. In the former case, if bounds were not
  // set at all, the creator of the Widget is expected to call SetBounds()
  // before calling Widget::Show() to avoid a kWindowSizeDeterminedLater-sized
  // (i.e. 1x1) window appearing.
  bridge()->SetInitialBounds(params.bounds, widget->GetMinimumSize());

  // TODO(ccameron): Correctly set these based |local_window_|.
  window_bounds_in_screen_ = params.bounds;
  content_bounds_in_screen_ = params.bounds;

  // Widgets for UI controls (usually layered above web contents) start visible.
  if (widget_type_ == Widget::InitParams::TYPE_CONTROL)
    bridge()->SetVisibilityState(WindowVisibilityState::kShowInactive);
}

void BridgedNativeWidgetHostImpl::SetBounds(const gfx::Rect& bounds) {
  bridge()->SetBounds(bounds,
                      native_widget_mac_->GetWidget()->GetMinimumSize());
}

void BridgedNativeWidgetHostImpl::SetFullscreen(bool fullscreen) {
  // Note that when the NSWindow begins a fullscreen transition, the value of
  // |target_fullscreen_state_| updates via OnWindowFullscreenTransitionStart.
  // The update here is necessary for the case where we are currently in
  // transition (and therefore OnWindowFullscreenTransitionStart will not be
  // called until the current transition completes).
  target_fullscreen_state_ = fullscreen;
  bridge()->SetFullscreen(target_fullscreen_state_);
}

void BridgedNativeWidgetHostImpl::SetRootView(views::View* root_view) {
  root_view_ = root_view;
  if (root_view_) {
    // TODO(ccameron): Drag-drop functionality does not yet run over mojo.
    if (bridge_impl_) {
      drag_drop_client_.reset(
          new DragDropClientMac(bridge_impl_.get(), root_view_));
    }
  } else {
    drag_drop_client_.reset();
  }
}

void BridgedNativeWidgetHostImpl::CreateCompositor(
    const Widget::InitParams& params) {
  DCHECK(!compositor_);
  DCHECK(!layer());
  DCHECK(ViewsDelegate::GetInstance());

  // "Infer" must be handled by ViewsDelegate::OnBeforeWidgetInit().
  DCHECK_NE(Widget::InitParams::INFER_OPACITY, params.opacity);
  bool translucent = params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW;

  // Create the layer.
  SetLayer(std::make_unique<ui::Layer>(params.layer_type));
  layer()->set_delegate(this);
  layer()->SetFillsBoundsOpaquely(!translucent);

  // Create the compositor and attach the layer to it.
  ui::ContextFactory* context_factory =
      ViewsDelegate::GetInstance()->GetContextFactory();
  DCHECK(context_factory);
  ui::ContextFactoryPrivate* context_factory_private =
      ViewsDelegate::GetInstance()->GetContextFactoryPrivate();
  compositor_ = ui::RecyclableCompositorMacFactory::Get()->CreateCompositor(
      context_factory, context_factory_private);
  compositor_->widget()->SetNSView(this);
  compositor_->compositor()->SetBackgroundColor(
      translucent ? SK_ColorTRANSPARENT : SK_ColorWHITE);
  compositor_->compositor()->SetRootLayer(layer());

  // The compositor is locked (prevented from producing frames) until the widget
  // is made visible.
  UpdateCompositorProperties();
  layer()->SetVisible(is_visible_);
  if (is_visible_)
    compositor_->Unsuspend();

  bridge()->InitCompositorView();
}

void BridgedNativeWidgetHostImpl::UpdateCompositorProperties() {
  if (!compositor_)
    return;
  gfx::Size surface_size_in_dip = content_bounds_in_screen_.size();
  layer()->SetBounds(gfx::Rect(surface_size_in_dip));
  compositor_->UpdateSurface(
      ConvertSizeToPixel(display_.device_scale_factor(), surface_size_in_dip),
      display_.device_scale_factor());
}

void BridgedNativeWidgetHostImpl::DestroyCompositor() {
  if (layer()) {
    // LayerOwner supports a change in ownership, e.g., to animate a closing
    // window, but that won't work as expected for the root layer in
    // BridgedNativeWidgetImpl.
    DCHECK_EQ(this, layer()->owner());
    layer()->CompleteAllAnimations();
    layer()->SuppressPaint();
    layer()->set_delegate(nullptr);
  }
  DestroyLayer();
  if (!compositor_)
    return;
  compositor_->widget()->ResetNSView();
  compositor_->compositor()->SetRootLayer(nullptr);
  ui::RecyclableCompositorMacFactory::Get()->RecycleCompositor(
      std::move(compositor_));
}

void BridgedNativeWidgetHostImpl::SetFocusManager(FocusManager* focus_manager) {
  if (focus_manager_ == focus_manager)
    return;

  if (focus_manager_) {
    // Only the destructor can replace the focus manager (and it passes null).
    DCHECK(!focus_manager);
    if (View* old_focus = focus_manager_->GetFocusedView())
      OnDidChangeFocus(old_focus, nullptr);
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
    return;
  }

  focus_manager_ = focus_manager;
  focus_manager_->AddFocusChangeListener(this);
  if (View* new_focus = focus_manager_->GetFocusedView())
    OnDidChangeFocus(nullptr, new_focus);
}

bool BridgedNativeWidgetHostImpl::SetWindowTitle(const base::string16& title) {
  if (window_title_ == title)
    return false;
  window_title_ = title;
  bridge()->SetWindowTitle(window_title_);
  return true;
}

void BridgedNativeWidgetHostImpl::OnWidgetInitDone() {
  Widget* widget = native_widget_mac_->GetWidget();
  if (DialogDelegate* dialog = widget->widget_delegate()->AsDialogDelegate())
    dialog->AddObserver(this);
}

ui::InputMethod* BridgedNativeWidgetHostImpl::GetInputMethod() {
  if (!input_method_) {
    input_method_ = ui::CreateInputMethod(this, gfx::kNullAcceleratedWidget);
    // For now, use always-focused mode on Mac for the input method.
    // TODO(tapted): Move this to OnWindowKeyStatusChangedTo() and balance.
    input_method_->OnFocus();
  }
  return input_method_.get();
}

gfx::Rect BridgedNativeWidgetHostImpl::GetRestoredBounds() const {
  if (target_fullscreen_state_ || in_fullscreen_transition_)
    return window_bounds_before_fullscreen_;
  return window_bounds_in_screen_;
}

void BridgedNativeWidgetHostImpl::SetNativeWindowProperty(const char* name,
                                                          void* value) {
  if (value)
    native_window_properties_[name] = value;
  else
    native_window_properties_.erase(name);
}

void* BridgedNativeWidgetHostImpl::GetNativeWindowProperty(
    const char* name) const {
  auto found = native_window_properties_.find(name);
  if (found == native_window_properties_.end())
    return nullptr;
  return found->second;
}

void BridgedNativeWidgetHostImpl::SetParent(
    BridgedNativeWidgetHostImpl* new_parent) {
  if (new_parent == parent_)
    return;

  if (parent_) {
    auto found =
        std::find(parent_->children_.begin(), parent_->children_.end(), this);
    DCHECK(found != parent_->children_.end());
    parent_->children_.erase(found);
  }

  parent_ = new_parent;
  if (parent_) {
    // We can only re-parent to another Widget if that Widget is hosted in the
    // same process that we were already hosted by.
    CHECK_EQ(bridge_factory_host_, parent_->bridge_factory_host());
    parent_->children_.push_back(this);
    bridge()->SetParent(parent_->bridged_native_widget_id());
  } else {
    bridge()->SetParent(0);
  }
}

void BridgedNativeWidgetHostImpl::SetAssociationForView(const View* view,
                                                        NSView* native_view) {
  DCHECK_EQ(0u, associated_views_.count(view));
  associated_views_[view] = native_view;
  native_widget_mac_->GetWidget()->ReorderNativeViews();
}

void BridgedNativeWidgetHostImpl::ClearAssociationForView(const View* view) {
  auto it = associated_views_.find(view);
  DCHECK(it != associated_views_.end());
  associated_views_.erase(it);
}

void BridgedNativeWidgetHostImpl::ReorderChildViews() {
  std::map<NSView*, int> rank;
  Widget* widget = native_widget_mac_->GetWidget();
  RankNSViewsRecursive(widget->GetRootView(), &rank);
  if (bridge_impl_)
    bridge_impl_->SortSubviews(std::move(rank));
}

void BridgedNativeWidgetHostImpl::RankNSViewsRecursive(
    View* view,
    std::map<NSView*, int>* rank) const {
  auto it = associated_views_.find(view);
  if (it != associated_views_.end())
    rank->emplace(it->second, rank->size());
  for (int i = 0; i < view->child_count(); ++i)
    RankNSViewsRecursive(view->child_at(i), rank);
}

// static
NSView* BridgedNativeWidgetHostImpl::GetGlobalCaptureView() {
  // TODO(ccameron): This will not work across process boundaries.
  return [CocoaMouseCapture::GetGlobalCaptureWindow() contentView];
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, views_bridge_mac::BridgedNativeWidgetHostHelper:

NSView* BridgedNativeWidgetHostImpl::GetNativeViewAccessible() {
  return root_view_ ? root_view_->GetNativeViewAccessible() : nil;
}

void BridgedNativeWidgetHostImpl::DispatchKeyEvent(ui::KeyEvent* event) {
  ignore_result(
      root_view_->GetWidget()->GetInputMethod()->DispatchKeyEvent(event));
}

bool BridgedNativeWidgetHostImpl::DispatchKeyEventToMenuController(
    ui::KeyEvent* event) {
  MenuController* menu_controller = MenuController::GetActiveInstance();
  if (menu_controller && root_view_ &&
      menu_controller->owner() == root_view_->GetWidget()) {
    return menu_controller->OnWillDispatchKeyEvent(event) ==
           ui::POST_DISPATCH_NONE;
  }
  return false;
}

double BridgedNativeWidgetHostImpl::SheetPositionY() {
  return native_widget_mac_->SheetPositionY();
}

views_bridge_mac::DragDropClient*
BridgedNativeWidgetHostImpl::GetDragDropClient() {
  return drag_drop_client_.get();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, BridgeFactoryHost::Observer:
void BridgedNativeWidgetHostImpl::OnBridgeFactoryHostDestroying(
    BridgeFactoryHost* host) {
  DCHECK_EQ(host, bridge_factory_host_);
  bridge_factory_host_->RemoveObserver(this);
  bridge_factory_host_ = nullptr;

  // Because the process hosting this window has ended, close the window by
  // sending the window close messages that the bridge would have sent.
  OnWindowWillClose();
  // Explicitly propagate this message to all children (they are also observers,
  // but may not be destroyed before |this| is destroyed, which would violate
  // tear-down assumptions). This would have been done by the bridge, had it
  // shut down cleanly.
  while (!children_.empty())
    children_.front()->OnBridgeFactoryHostDestroying(host);
  OnWindowHasClosed();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl,
// views_bridge_mac::mojom::BridgedNativeWidgetHost:

void BridgedNativeWidgetHostImpl::OnVisibilityChanged(bool window_visible) {
  is_visible_ = window_visible;
  if (compositor_) {
    layer()->SetVisible(window_visible);
    if (window_visible) {
      compositor_->Unsuspend();
      layer()->SchedulePaint(layer()->bounds());
    } else {
      compositor_->Suspend();
    }
  }
  native_widget_mac_->GetWidget()->OnNativeWidgetVisibilityChanged(
      window_visible);
}

void BridgedNativeWidgetHostImpl::OnWindowNativeThemeChanged() {
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyObservers();
}

void BridgedNativeWidgetHostImpl::OnScrollEvent(
    std::unique_ptr<ui::Event> event) {
  root_view_->GetWidget()->OnScrollEvent(event->AsScrollEvent());
}

void BridgedNativeWidgetHostImpl::OnMouseEvent(
    std::unique_ptr<ui::Event> event) {
  root_view_->GetWidget()->OnMouseEvent(event->AsMouseEvent());
}

void BridgedNativeWidgetHostImpl::OnGestureEvent(
    std::unique_ptr<ui::Event> event) {
  root_view_->GetWidget()->OnGestureEvent(event->AsGestureEvent());
}

bool BridgedNativeWidgetHostImpl::DispatchKeyEventRemote(
    std::unique_ptr<ui::Event> event,
    bool* event_handled) {
  DispatchKeyEvent(event->AsKeyEvent());
  *event_handled = event->handled();
  return true;
}

bool BridgedNativeWidgetHostImpl::DispatchKeyEventToMenuControllerRemote(
    std::unique_ptr<ui::Event> event,
    bool* event_swallowed,
    bool* event_handled) {
  *event_swallowed = DispatchKeyEventToMenuController(event->AsKeyEvent());
  *event_handled = event->handled();
  return true;
}

bool BridgedNativeWidgetHostImpl::GetHasMenuController(
    bool* has_menu_controller) {
  MenuController* menu_controller = MenuController::GetActiveInstance();
  *has_menu_controller = menu_controller && root_view_ &&
                         menu_controller->owner() == root_view_->GetWidget();
  return true;
}

void BridgedNativeWidgetHostImpl::OnViewSizeChanged(const gfx::Size& new_size) {
  root_view_->SetSize(new_size);
}

void BridgedNativeWidgetHostImpl::SetKeyboardAccessible(bool enabled) {
  views::FocusManager* focus_manager =
      root_view_->GetWidget()->GetFocusManager();
  if (focus_manager)
    focus_manager->SetKeyboardAccessible(enabled);
}

void BridgedNativeWidgetHostImpl::OnIsFirstResponderChanged(
    bool is_first_responder) {
  if (is_first_responder) {
    root_view_->GetWidget()->GetFocusManager()->RestoreFocusedView();
  } else {
    // Do not call ClearNativeFocus because that will re-make the
    // BridgedNativeWidget first responder (and this is called to indicate that
    // it is no longer first responder).
    root_view_->GetWidget()->GetFocusManager()->StoreFocusedView(
        false /* clear_native_focus */);
  }
}

void BridgedNativeWidgetHostImpl::OnMouseCaptureActiveChanged(bool is_active) {
  DCHECK_NE(is_mouse_capture_active_, is_active);
  is_mouse_capture_active_ = is_active;
  if (!is_mouse_capture_active_)
    native_widget_mac_->GetWidget()->OnMouseCaptureLost();
}

bool BridgedNativeWidgetHostImpl::GetIsDraggableBackgroundAt(
    const gfx::Point& location_in_content,
    bool* is_draggable_background) {
  int component =
      root_view_->GetWidget()->GetNonClientComponent(location_in_content);
  *is_draggable_background = component == HTCAPTION;
  return true;
}

bool BridgedNativeWidgetHostImpl::GetTooltipTextAt(
    const gfx::Point& location_in_content,
    base::string16* new_tooltip_text) {
  views::View* view =
      root_view_->GetTooltipHandlerForPoint(location_in_content);
  if (view) {
    gfx::Point view_point = location_in_content;
    views::View::ConvertPointToScreen(root_view_, &view_point);
    views::View::ConvertPointFromScreen(view, &view_point);
    if (!view->GetTooltipText(view_point, new_tooltip_text))
      DCHECK(new_tooltip_text->empty());
  }
  return true;
}

void BridgedNativeWidgetHostImpl::GetWordAt(
    const gfx::Point& location_in_content,
    bool* found_word,
    gfx::DecoratedText* decorated_word,
    gfx::Point* baseline_point) {
  *found_word = false;

  views::View* target =
      root_view_->GetEventHandlerForPoint(location_in_content);
  if (!target)
    return;

  views::WordLookupClient* word_lookup_client = target->GetWordLookupClient();
  if (!word_lookup_client)
    return;

  gfx::Point location_in_target = location_in_content;
  views::View::ConvertPointToTarget(root_view_, target, &location_in_target);
  if (!word_lookup_client->GetWordLookupDataAtPoint(
          location_in_target, decorated_word, baseline_point)) {
    return;
  }

  // Convert |baselinePoint| to the coordinate system of |root_view_|.
  views::View::ConvertPointToTarget(target, root_view_, baseline_point);
  *found_word = true;
}

bool BridgedNativeWidgetHostImpl::GetWidgetIsModal(bool* widget_is_modal) {
  *widget_is_modal = native_widget_mac_->GetWidget()->IsModal();
  return true;
}

bool BridgedNativeWidgetHostImpl::GetIsFocusedViewTextual(bool* is_textual) {
  views::FocusManager* focus_manager =
      root_view_ ? root_view_->GetWidget()->GetFocusManager() : nullptr;
  *is_textual = focus_manager && focus_manager->GetFocusedView() &&
                focus_manager->GetFocusedView()->GetClassName() ==
                    views::Label::kViewClassName;
  return true;
}

void BridgedNativeWidgetHostImpl::OnWindowGeometryChanged(
    const gfx::Rect& new_window_bounds_in_screen,
    const gfx::Rect& new_content_bounds_in_screen) {
  // If we are accessing the BridgedNativeWidget through mojo, then
  // |local_window_| is not the true window that was just resized. Update
  // the frame of |local_window_| to keep it in sync for any native calls
  // that may use it (e.g, for context menu positioning).
  if (bridge_ptr_) {
    [local_window_ setFrame:gfx::ScreenRectToNSRect(new_window_bounds_in_screen)
                    display:NO
                    animate:NO];
  }

  bool window_has_moved =
      new_window_bounds_in_screen.origin() != window_bounds_in_screen_.origin();
  bool content_has_resized =
      new_content_bounds_in_screen.size() != content_bounds_in_screen_.size();

  window_bounds_in_screen_ = new_window_bounds_in_screen;
  content_bounds_in_screen_ = new_content_bounds_in_screen;

  // When a window grows vertically, the AppKit origin changes, but as far as
  // tookit-views is concerned, the window hasn't moved. Suppress these.
  if (window_has_moved)
    native_widget_mac_->GetWidget()->OnNativeWidgetMove();

  // Note we can't use new_window_bounds_in_screen.size(), since it includes the
  // titlebar for the purposes of detecting a window move.
  if (content_has_resized)
    native_widget_mac_->GetWidget()->OnNativeWidgetSizeChanged(
        content_bounds_in_screen_.size());

  // Update the compositor surface and layer size.
  UpdateCompositorProperties();
}

void BridgedNativeWidgetHostImpl::OnWindowFullscreenTransitionStart(
    bool target_fullscreen_state) {
  target_fullscreen_state_ = target_fullscreen_state;
  in_fullscreen_transition_ = true;

  // If going into fullscreen, store an answer for GetRestoredBounds().
  if (target_fullscreen_state)
    window_bounds_before_fullscreen_ = window_bounds_in_screen_;

  // Notify that fullscreen state changed.
  native_widget_mac_->OnWindowFullscreenStateChange();
}

void BridgedNativeWidgetHostImpl::OnWindowFullscreenTransitionComplete(
    bool actual_fullscreen_state) {
  in_fullscreen_transition_ = false;

  // Ensure constraints are re-applied when completing a transition.
  native_widget_mac_->OnSizeConstraintsChanged();
}

void BridgedNativeWidgetHostImpl::OnWindowMiniaturizedChanged(
    bool miniaturized) {
  is_miniaturized_ = miniaturized;
  if (native_widget_mac_)
    native_widget_mac_->GetWidget()->OnNativeWidgetWindowShowStateChanged();
}

void BridgedNativeWidgetHostImpl::OnWindowDisplayChanged(
    const display::Display& new_display) {
  bool scale_factor_changed =
      display_.device_scale_factor() != new_display.device_scale_factor();
  bool display_id_changed = display_.id() != new_display.id();
  display_ = new_display;
  if (scale_factor_changed && compositor_) {
    compositor_->UpdateSurface(
        ConvertSizeToPixel(display_.device_scale_factor(),
                           content_bounds_in_screen_.size()),
        display_.device_scale_factor());
  }
  if (display_id_changed) {
    display_link_ = ui::DisplayLinkMac::GetForDisplay(display_.id());
    if (!display_link_) {
      // Note that on some headless systems, the display link will fail to be
      // created, so this should not be a fatal error.
      LOG(ERROR) << "Failed to create display link.";
    }
  }
}

void BridgedNativeWidgetHostImpl::OnWindowWillClose() {
  Widget* widget = native_widget_mac_->GetWidget();
  if (DialogDelegate* dialog = widget->widget_delegate()->AsDialogDelegate())
    dialog->RemoveObserver(this);
  native_widget_mac_->WindowDestroying();
  // Remove |this| from the parent's list of children.
  SetParent(nullptr);
}

void BridgedNativeWidgetHostImpl::OnWindowHasClosed() {
  // OnWindowHasClosed will be called only after all child windows have had
  // OnWindowWillClose called on them.
  DCHECK(children_.empty());
  native_widget_mac_->WindowDestroyed();
}

void BridgedNativeWidgetHostImpl::OnWindowKeyStatusChanged(
    bool is_key,
    bool is_content_first_responder,
    bool full_keyboard_access_enabled) {
  is_window_key_ = is_key;
  Widget* widget = native_widget_mac_->GetWidget();
  if (!widget->OnNativeWidgetActivationChanged(is_key))
    return;
  // The contentView is the BridgedContentView hosting the views::RootView. The
  // focus manager will already know if a native subview has focus.
  if (is_content_first_responder) {
    if (is_key) {
      widget->OnNativeFocus();
      // Explicitly set the keyboard accessibility state on regaining key
      // window status.
      SetKeyboardAccessible(full_keyboard_access_enabled);
      widget->GetFocusManager()->RestoreFocusedView();
    } else {
      widget->OnNativeBlur();
      widget->GetFocusManager()->StoreFocusedView(true);
    }
  }
}

void BridgedNativeWidgetHostImpl::DoDialogButtonAction(
    ui::DialogButton button) {
  views::DialogDelegate* dialog =
      root_view_->GetWidget()->widget_delegate()->AsDialogDelegate();
  DCHECK(dialog);
  views::DialogClientView* client = dialog->GetDialogClientView();
  if (button == ui::DIALOG_BUTTON_OK) {
    client->AcceptWindow();
  } else {
    DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
    client->CancelWindow();
  }
}

bool BridgedNativeWidgetHostImpl::GetDialogButtonInfo(
    ui::DialogButton button,
    bool* button_exists,
    base::string16* button_label,
    bool* is_button_enabled,
    bool* is_button_default) {
  *button_exists = false;
  ui::DialogModel* model =
      root_view_->GetWidget()->widget_delegate()->AsDialogDelegate();
  if (!model || !(model->GetDialogButtons() & button))
    return true;
  *button_exists = true;
  *button_label = model->GetDialogButtonLabel(button);
  *is_button_enabled = model->IsDialogButtonEnabled(button);
  *is_button_default = button == model->GetDefaultDialogButton();
  return true;
}

bool BridgedNativeWidgetHostImpl::GetDoDialogButtonsExist(bool* buttons_exist) {
  ui::DialogModel* model =
      root_view_->GetWidget()->widget_delegate()->AsDialogDelegate();
  *buttons_exist = model && model->GetDialogButtons();
  return true;
}

bool BridgedNativeWidgetHostImpl::GetShouldShowWindowTitle(
    bool* should_show_window_title) {
  *should_show_window_title =
      root_view_
          ? root_view_->GetWidget()->widget_delegate()->ShouldShowWindowTitle()
          : true;
  return true;
}

bool BridgedNativeWidgetHostImpl::GetCanWindowBecomeKey(
    bool* can_window_become_key) {
  *can_window_become_key =
      root_view_ ? root_view_->GetWidget()->CanActivate() : false;
  return true;
}

bool BridgedNativeWidgetHostImpl::GetAlwaysRenderWindowAsKey(
    bool* always_render_as_key) {
  *always_render_as_key =
      root_view_ ? root_view_->GetWidget()->IsAlwaysRenderAsActive() : false;
  return true;
}

bool BridgedNativeWidgetHostImpl::GetCanWindowClose(bool* can_window_close) {
  *can_window_close = true;
  views::NonClientView* non_client_view =
      root_view_ ? root_view_->GetWidget()->non_client_view() : nullptr;
  if (non_client_view)
    *can_window_close = non_client_view->CanClose();
  return true;
}

bool BridgedNativeWidgetHostImpl::GetWindowFrameTitlebarHeight(
    bool* override_titlebar_height,
    float* titlebar_height) {
  native_widget_mac_->GetWindowFrameTitlebarHeight(override_titlebar_height,
                                                   titlebar_height);
  return true;
}

void BridgedNativeWidgetHostImpl::OnFocusWindowToolbar() {
  native_widget_mac_->OnFocusWindowToolbar();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl,
// views_bridge_mac::mojom::BridgedNativeWidgetHost synchronous callbacks:

void BridgedNativeWidgetHostImpl::DispatchKeyEventRemote(
    std::unique_ptr<ui::Event> event,
    DispatchKeyEventRemoteCallback callback) {
  bool event_handled = false;
  DispatchKeyEventRemote(std::move(event), &event_handled);
  std::move(callback).Run(event_handled);
}

void BridgedNativeWidgetHostImpl::DispatchKeyEventToMenuControllerRemote(
    std::unique_ptr<ui::Event> event,
    DispatchKeyEventToMenuControllerRemoteCallback callback) {
  ui::KeyEvent* key_event = event->AsKeyEvent();
  bool event_swallowed = DispatchKeyEventToMenuController(key_event);
  std::move(callback).Run(event_swallowed, key_event->handled());
}

void BridgedNativeWidgetHostImpl::GetHasMenuController(
    GetHasMenuControllerCallback callback) {
  bool has_menu_controller = false;
  GetHasMenuController(&has_menu_controller);
  std::move(callback).Run(has_menu_controller);
}

void BridgedNativeWidgetHostImpl::GetIsDraggableBackgroundAt(
    const gfx::Point& location_in_content,
    GetIsDraggableBackgroundAtCallback callback) {
  bool is_draggable_background = false;
  GetIsDraggableBackgroundAt(location_in_content, &is_draggable_background);
  std::move(callback).Run(is_draggable_background);
}

void BridgedNativeWidgetHostImpl::GetTooltipTextAt(
    const gfx::Point& location_in_content,
    GetTooltipTextAtCallback callback) {
  base::string16 new_tooltip_text;
  GetTooltipTextAt(location_in_content, &new_tooltip_text);
  std::move(callback).Run(new_tooltip_text);
}

void BridgedNativeWidgetHostImpl::GetIsFocusedViewTextual(
    GetIsFocusedViewTextualCallback callback) {
  bool is_textual = false;
  GetIsFocusedViewTextual(&is_textual);
  std::move(callback).Run(is_textual);
}

void BridgedNativeWidgetHostImpl::GetWidgetIsModal(
    GetWidgetIsModalCallback callback) {
  bool widget_is_modal = false;
  GetWidgetIsModal(&widget_is_modal);
  std::move(callback).Run(widget_is_modal);
}

void BridgedNativeWidgetHostImpl::GetDialogButtonInfo(
    ui::DialogButton button,
    GetDialogButtonInfoCallback callback) {
  bool exists = false;
  base::string16 label;
  bool is_enabled = false;
  bool is_default = false;
  GetDialogButtonInfo(button, &exists, &label, &is_enabled, &is_default);
  std::move(callback).Run(exists, label, is_enabled, is_default);
}

void BridgedNativeWidgetHostImpl::GetDoDialogButtonsExist(
    GetDoDialogButtonsExistCallback callback) {
  bool buttons_exist = false;
  GetDoDialogButtonsExist(&buttons_exist);
  std::move(callback).Run(buttons_exist);
}

void BridgedNativeWidgetHostImpl::GetShouldShowWindowTitle(
    GetShouldShowWindowTitleCallback callback) {
  bool should_show_window_title = false;
  GetShouldShowWindowTitle(&should_show_window_title);
  std::move(callback).Run(should_show_window_title);
}

void BridgedNativeWidgetHostImpl::GetCanWindowBecomeKey(
    GetCanWindowBecomeKeyCallback callback) {
  bool can_window_become_key = false;
  GetCanWindowBecomeKey(&can_window_become_key);
  std::move(callback).Run(can_window_become_key);
}

void BridgedNativeWidgetHostImpl::GetAlwaysRenderWindowAsKey(
    GetAlwaysRenderWindowAsKeyCallback callback) {
  bool always_render_as_key = false;
  GetAlwaysRenderWindowAsKey(&always_render_as_key);
  std::move(callback).Run(always_render_as_key);
}

void BridgedNativeWidgetHostImpl::GetCanWindowClose(
    GetCanWindowCloseCallback callback) {
  bool can_window_close = false;
  GetCanWindowClose(&can_window_close);
  std::move(callback).Run(can_window_close);
}

void BridgedNativeWidgetHostImpl::GetWindowFrameTitlebarHeight(
    GetWindowFrameTitlebarHeightCallback callback) {
  bool override_titlebar_height = false;
  float titlebar_height = 0;
  GetWindowFrameTitlebarHeight(&override_titlebar_height, &titlebar_height);
  std::move(callback).Run(override_titlebar_height, titlebar_height);
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, DialogObserver:

void BridgedNativeWidgetHostImpl::OnDialogModelChanged() {
  // Note it's only necessary to clear the TouchBar. If the OS needs it again,
  // a new one will be created.
  bridge()->ClearTouchBar();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, FocusChangeListener:

void BridgedNativeWidgetHostImpl::OnWillChangeFocus(View* focused_before,
                                                    View* focused_now) {}

void BridgedNativeWidgetHostImpl::OnDidChangeFocus(View* focused_before,
                                                   View* focused_now) {
  ui::InputMethod* input_method =
      native_widget_mac_->GetWidget()->GetInputMethod();
  if (input_method) {
    ui::TextInputClient* input_client = input_method->GetTextInputClient();
    // Sanity check: When focus moves away from the widget (i.e. |focused_now|
    // is nil), then the textInputClient will be cleared.
    DCHECK(!!focused_now || !input_client);
    // TODO(ccameron): TextInputClient is not handled across process borders
    // yet.
    if (bridge_impl_)
      bridge_impl_->SetTextInputClient(input_client);
  }
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetImpl, internal::InputMethodDelegate:

ui::EventDispatchDetails BridgedNativeWidgetHostImpl::DispatchKeyEventPostIME(
    ui::KeyEvent* key,
    base::OnceCallback<void(bool)> ack_callback) {
  DCHECK(focus_manager_);
  if (!focus_manager_->OnKeyEvent(*key))
    key->StopPropagation();
  else
    native_widget_mac_->GetWidget()->OnKeyEvent(key);
  CallDispatchKeyEventPostIMEAck(key, std::move(ack_callback));
  return ui::EventDispatchDetails();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, LayerDelegate:

void BridgedNativeWidgetHostImpl::OnPaintLayer(
    const ui::PaintContext& context) {
  native_widget_mac_->GetWidget()->OnNativeWidgetPaint(context);
}

void BridgedNativeWidgetHostImpl::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  native_widget_mac_->GetWidget()->DeviceScaleFactorChanged(
      old_device_scale_factor, new_device_scale_factor);
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, AcceleratedWidgetMac:

void BridgedNativeWidgetHostImpl::AcceleratedWidgetCALayerParamsUpdated() {
  const gfx::CALayerParams* ca_layer_params =
      compositor_->widget()->GetCALayerParams();
  if (ca_layer_params)
    bridge()->SetCALayerParams(*ca_layer_params);

  // Take this opportunity to update the VSync parameters, if needed.
  if (display_link_) {
    base::TimeTicks timebase;
    base::TimeDelta interval;
    if (display_link_->GetVSyncParameters(&timebase, &interval))
      compositor_->compositor()->SetDisplayVSyncParameters(timebase, interval);
  }
}

}  // namespace views
