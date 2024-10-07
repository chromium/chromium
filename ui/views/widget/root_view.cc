// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/root_view.h"

#include <algorithm>
#include <memory>
#include <string_view>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/root_view_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using DispatchDetails = ui::EventDispatchDetails;

namespace views::internal {

namespace {

class MouseEnterExitEvent : public ui::MouseEvent {
 public:
  MouseEnterExitEvent(const ui::MouseEvent& event, ui::EventType type)
      : ui::MouseEvent(event,
                       static_cast<View*>(nullptr),
                       static_cast<View*>(nullptr)) {
    DCHECK(type == ui::EventType::kMouseEntered ||
           type == ui::EventType::kMouseExited);
    SetType(type);
  }

  ~MouseEnterExitEvent() override = default;

  // Event:
  std::unique_ptr<ui::Event> Clone() const override {
    return std::make_unique<MouseEnterExitEvent>(*this);
  }
};

}  // namespace

// Used by RootView to create a hidden child that can be used to make screen
// reader announcements on platforms that don't have a reliable system API call
// to do that.
//
// We use a separate view because the RootView itself supplies the widget's
// accessible name and cannot serve double-duty (the inability for views to make
// their own announcements without changing their accessible name or description
// is the reason this system exists at all).
class AnnounceTextView : public View {
  METADATA_HEADER(AnnounceTextView, View)

 public:
  AnnounceTextView() { UpdateAccessibleRole(); }

  ~AnnounceTextView() override = default;

  void AnnounceTextAs(const std::u16string& text,
                      ui::AXPlatformNode::AnnouncementType announcement_type) {
    announce_text_ = text;
    switch (announcement_type) {
      case ui::AXPlatformNode::AnnouncementType::kAlert:
        announce_event_type_ = ax::mojom::Event::kAlert;
        announce_role_ = ax::mojom::Role::kAlert;
        break;
      case ui::AXPlatformNode::AnnouncementType::kPolite:
        announce_event_type_ = ax::mojom::Event::kLiveRegionChanged;
        announce_role_ = ax::mojom::Role::kStatus;
        break;
    }
    if (base::FeatureList::IsEnabled(
            features::kAnnounceTextAdditionalAttributes)) {
      GetViewAccessibility().SetContainerLiveStatus("polite");
    } else {
      GetViewAccessibility().RemoveContainerLiveStatus();
    }

    UpdateAccessibleRole();

    NotifyAccessibilityEvent(announce_event_type_, /*send_native_event=*/true);
  }

  void UpdateAccessibleRole() {
#if BUILDFLAG(IS_CHROMEOS)
    // On ChromeOS, kAlert role can invoke an unnecessary event on reparenting.
    GetViewAccessibility().SetRole(ax::mojom::Role::kStaticText);
#elif BUILDFLAG(IS_LINUX)
    // TODO(crbug.com/40658933): Use live regions (do not use alerts).
    // May require setting kLiveStatus, kContainerLiveStatus to "polite".
    GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
#else
    GetViewAccessibility().SetRole(announce_role_);
#endif
  }

  // View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic, true);
    node_data->AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                  "polite");
    if (base::FeatureList::IsEnabled(
            features::kAnnounceTextAdditionalAttributes)) {
      node_data->AddStringAttribute(ax::mojom::StringAttribute::kLiveRelevant,
                                    "additions text");
      node_data->AddStringAttribute(
          ax::mojom::StringAttribute::kContainerLiveRelevant, "additions text");
    }

    !announce_text_.empty() ? node_data->SetNameChecked(announce_text_)
                            : node_data->SetNameExplicitlyEmpty();
    node_data->AddState(ax::mojom::State::kInvisible);
  }

 private:
  std::u16string announce_text_;
  ax::mojom::Event announce_event_type_ = ax::mojom::Event::kNone;
  // View should have a initial accessible role, and it will later change
  // depending on the announce_role_ accordingly.
  ax::mojom::Role announce_role_ = ax::mojom::Role::kStatus;
};

BEGIN_METADATA(AnnounceTextView)
END_METADATA

// This event handler receives events in the pre-target phase and takes care of
// the following:
//   - Shows keyboard-triggered context menus.
class PreEventDispatchHandler : public ui::EventHandler {
 public:
  explicit PreEventDispatchHandler(View* owner) : owner_(owner) {
    owner_->AddPreTargetHandler(this);
  }
  PreEventDispatchHandler(const PreEventDispatchHandler&) = delete;
  PreEventDispatchHandler& operator=(const PreEventDispatchHandler&) = delete;
  ~PreEventDispatchHandler() override { owner_->RemovePreTargetHandler(this); }

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    CHECK_EQ(ui::EP_PRETARGET, event->phase());
// macOS doesn't have keyboard-triggered context menus.
#if !BUILDFLAG(IS_MAC)
    if (event->handled())
      return;

    View* v = nullptr;
    if (owner_->GetFocusManager())  // Can be NULL in unittests.
      v = owner_->GetFocusManager()->GetFocusedView();
    // Special case to handle keyboard-triggered context menus.
    if (v && v->GetEnabled() &&
        ((event->key_code() == ui::VKEY_APPS) ||
         (event->key_code() == ui::VKEY_F10 && event->IsShiftDown()))) {
      // Clamp the menu location within the visible bounds of each ancestor view
      // to avoid showing the menu over a completely different view or window.
      gfx::Point location = v->GetKeyboardContextMenuLocation();
      for (View* parent = v->parent(); parent; parent = parent->parent()) {
        const gfx::Rect& parent_bounds = parent->GetBoundsInScreen();
        location.SetToMax(parent_bounds.origin());
        location.SetToMin(parent_bounds.bottom_right());
      }
      v->ShowContextMenu(location, ui::MENU_SOURCE_KEYBOARD);
      event->StopPropagation();
    }
#endif
  }

  std::string_view GetLogContext() const override {
    return "PreEventDispatchHandler";
  }

  raw_ptr<View> owner_;
};

// This event handler receives events in the post-target phase and takes care of
// the following:
//   - Generates context menu, or initiates drag-and-drop, from gesture events.
class PostEventDispatchHandler : public ui::EventHandler {
 public:
  PostEventDispatchHandler()
      : touch_dnd_enabled_(::switches::IsTouchDragDropEnabled()) {}
  PostEventDispatchHandler(const PostEventDispatchHandler&) = delete;
  PostEventDispatchHandler& operator=(const PostEventDispatchHandler&) = delete;
  ~PostEventDispatchHandler() override = default;

 private:
  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {
    DCHECK_EQ(ui::EP_POSTTARGET, event->phase());
    if (event->handled())
      return;

    View* target = static_cast<View*>(event->target());

    gfx::Point location = event->location();
    if (touch_dnd_enabled_ &&
        event->type() == ui::EventType::kGestureLongPress &&
        (!target->drag_controller() ||
         target->drag_controller()->CanStartDragForView(target, location,
                                                        location))) {
      if (target->DoDrag(*event, location,
                         ui::mojom::DragEventSource::kTouch)) {
        event->StopPropagation();
        return;
      }
    }

    if (target->context_menu_controller() &&
        (event->type() == ui::EventType::kGestureLongPress ||
         event->type() == ui::EventType::kGestureLongTap ||
         event->type() == ui::EventType::kGestureTwoFingerTap)) {
      gfx::Point screen_location(location);
      View::ConvertPointToScreen(target, &screen_location);
      target->ShowContextMenu(screen_location, ui::MENU_SOURCE_TOUCH);
      event->StopPropagation();
    }
  }

  std::string_view GetLogContext() const override {
    return "PostEventDispatchHandler";
  }

  bool touch_dnd_enabled_;
};

////////////////////////////////////////////////////////////////////////////////
// RootView, public:

// Creation and lifetime -------------------------------------------------------

RootView::RootView(Widget* widget)
    : widget_(widget),
      pre_dispatch_handler_(
          std::make_unique<internal::PreEventDispatchHandler>(this)),
      post_dispatch_handler_(
          std::make_unique<internal::PostEventDispatchHandler>()) {
  AddPostTargetHandler(post_dispatch_handler_.get());
  SetEventTargeter(
      std::unique_ptr<ViewTargeter>(new RootViewTargeter(this, this)));

  auto* widget_delegate = widget->widget_delegate();
  if (widget_delegate) {
    if (ax::mojom::Role::kUnknown == GetViewAccessibility().GetCachedRole()) {
      GetViewAccessibility().SetRole(
          widget_delegate->GetAccessibleWindowRole());
    }
  }
}

RootView::~RootView() {
  // If we have children remove them explicitly so to make sure a remove
  // notification is sent for each one of them.
  RemoveAllChildViews();
}

// Tree operations -------------------------------------------------------------

void RootView::SetContentsView(View* contents_view) {
  DCHECK(contents_view && GetWidget()->native_widget())
      << "Can't be called because the widget is not initialized or is "
         "destroyed";
  // The ContentsView must be set up _after_ the window is created so that its
  // Widget pointer is valid.
  SetUseDefaultFillLayout(true);
  if (!children().empty())
    RemoveAllChildViews();
  AddChildView(contents_view);
}

View* RootView::GetContentsView() {
  return children().empty() ? nullptr : children().front();
}

void RootView::NotifyNativeViewHierarchyChanged() {
  PropagateNativeViewHierarchyChanged();
}

// Focus -----------------------------------------------------------------------

void RootView::SetFocusTraversableParent(FocusTraversable* focus_traversable) {
  DCHECK(focus_traversable != this);
  focus_traversable_parent_ = focus_traversable;
}

void RootView::SetFocusTraversableParentView(View* view) {
  focus_traversable_parent_view_ = view;
}

// System events ---------------------------------------------------------------

void RootView::ThemeChanged() {
  View::PropagateThemeChanged();
}

void RootView::ResetEventHandlers() {
  explicit_mouse_handler_ = false;
  mouse_pressed_handler_ = nullptr;
  mouse_move_handler_ = nullptr;
  gesture_handler_ = nullptr;
  event_dispatch_target_ = nullptr;
  old_dispatch_target_ = nullptr;
}

void RootView::DeviceScaleFactorChanged(float old_device_scale_factor,
                                        float new_device_scale_factor) {
  View::PropagateDeviceScaleFactorChanged(old_device_scale_factor,
                                          new_device_scale_factor);
}

// Accessibility ---------------------------------------------------------------

AnnounceTextView* RootView::GetOrCreateAnnounceView() {
  if (!announce_view_) {
    announce_view_ = AddChildView(std::make_unique<AnnounceTextView>());
    announce_view_->SetProperty(kViewIgnoredByLayoutKey, true);
  }
  return announce_view_.get();
}

void RootView::AnnounceTextAs(
    const std::u16string& text,
    ui::AXPlatformNode::AnnouncementType announcement_type) {
  if (text.empty()) {
    return;
  }
#if BUILDFLAG(IS_MAC)
  gfx::NativeViewAccessible native = GetViewAccessibility().GetNativeObject();
  if (auto* ax_node = ui::AXPlatformNode::FromNativeViewAccessible(native)) {
    ax_node->AnnounceTextAs(text, announcement_type);
  }
#else
  CHECK(GetWidget());
  CHECK(GetContentsView());
  GetOrCreateAnnounceView()->AnnounceTextAs(text, announcement_type);
#endif
}

View* RootView::GetAnnounceViewForTesting() {
  return GetOrCreateAnnounceView();
}

////////////////////////////////////////////////////////////////////////////////
// RootView, FocusTraversable implementation:

FocusSearch* RootView::GetFocusSearch() {
  return &focus_search_;
}

FocusTraversable* RootView::GetFocusTraversableParent() {
  return focus_traversable_parent_;
}

View* RootView::GetFocusTraversableParentView() {
  return focus_traversable_parent_view_;
}

////////////////////////////////////////////////////////////////////////////////
// RootView, ui::EventProcessor overrides:

ui::EventTarget* RootView::GetRootForEvent(ui::Event* event) {
  return this;
}

ui::EventTargeter* RootView::GetDefaultEventTargeter() {
  return this->GetEventTargeter();
}

void RootView::OnEventProcessingStarted(ui::Event* event) {
  VLOG(5) << "RootView::OnEventProcessingStarted(" << event->ToString() << ")";
  if (!event->IsGestureEvent())
    return;

  ui::GestureEvent* gesture_event = event->AsGestureEvent();

  // Do not process ui::EventType::kGestureBegin events.
  if (gesture_event->type() == ui::EventType::kGestureBegin) {
    event->SetHandled();
    return;
  }

  // Do not process ui::EventType::kGestureEnd events if they do not correspond
  // to the removal of the final touch point or if no gesture handler has
  // already been set.
  if (gesture_event->type() == ui::EventType::kGestureEnd &&
      (gesture_event->details().touch_points() > 1 || !gesture_handler_)) {
    event->SetHandled();
    return;
  }

  // Do not process subsequent gesture scroll events if no handler was set for
  // a ui::EventType::kGestureScrollBegin event.
  if (!gesture_handler_ &&
      (gesture_event->type() == ui::EventType::kGestureScrollUpdate ||
       gesture_event->type() == ui::EventType::kGestureScrollEnd ||
       gesture_event->type() == ui::EventType::kScrollFlingStart)) {
    event->SetHandled();
    return;
  }

  gesture_handler_set_before_processing_ = !!gesture_handler_;
}

void RootView::OnEventProcessingFinished(ui::Event* event) {
  VLOG(5) << "RootView::OnEventProcessingFinished(" << event->ToString() << ")";
  // If |event| was not handled and |gesture_handler_| was not set by the
  // dispatch of a previous gesture event, then no default gesture handler
  // should be set prior to the next gesture event being received.
  if (event->IsGestureEvent() && !event->handled() &&
      !gesture_handler_set_before_processing_) {
    gesture_handler_ = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
// RootView, View overrides:

const Widget* RootView::GetWidget() const {
  return widget_;
}

Widget* RootView::GetWidget() {
  return const_cast<Widget*>(const_cast<const RootView*>(this)->GetWidget());
}

bool RootView::IsDrawn() const {
  return GetVisible();
}

bool RootView::OnMousePressed(const ui::MouseEvent& event) {
  UpdateCursor(event);
  SetMouseLocationAndFlags(event);

  // If mouse_pressed_handler_ is non null, we are currently processing
  // a pressed -> drag -> released session. In that case we send the
  // event to mouse_pressed_handler_
  if (mouse_pressed_handler_) {
    ui::MouseEvent mouse_pressed_event(event, static_cast<View*>(this),
                                       mouse_pressed_handler_.get());
    drag_info_.Reset();
    ui::EventDispatchDetails dispatch_details =
        DispatchEvent(mouse_pressed_handler_, &mouse_pressed_event);
    if (dispatch_details.dispatcher_destroyed)
      return true;
    return true;
  }
  DCHECK(!explicit_mouse_handler_);

  // Walk up the tree from the target until we find a view that wants
  // the mouse event.
  for (mouse_pressed_handler_ = GetEventHandlerForPoint(event.location());
       mouse_pressed_handler_ && (mouse_pressed_handler_ != this);
       mouse_pressed_handler_ = mouse_pressed_handler_->parent()) {
    DVLOG(1) << "OnMousePressed testing "
             << mouse_pressed_handler_->GetClassName();
    DCHECK(mouse_pressed_handler_->GetEnabled());

    // See if this view wants to handle the mouse press.
    ui::MouseEvent mouse_pressed_event(event, static_cast<View*>(this),
                                       mouse_pressed_handler_.get());

    // Remove the double-click flag if the handler is different than the
    // one which got the first click part of the double-click.
    if (mouse_pressed_handler_ != last_click_handler_)
      mouse_pressed_event.SetFlags(event.flags() & ~ui::EF_IS_DOUBLE_CLICK);

    drag_info_.Reset();
    ui::EventDispatchDetails dispatch_details =
        DispatchEvent(mouse_pressed_handler_, &mouse_pressed_event);
    if (dispatch_details.dispatcher_destroyed)
      return mouse_pressed_event.handled();

    // The view could have removed itself from the tree when handling
    // OnMousePressed().  In this case, the removal notification will have
    // reset mouse_pressed_handler_ to NULL out from under us.  Detect this
    // case and stop.  (See comments in view.h.)
    //
    // NOTE: Don't return true here, because we don't want the frame to
    // forward future events to us when there's no handler.
    if (!mouse_pressed_handler_)
      break;

    // If the view handled the event, leave mouse_pressed_handler_ set and
    // return true, which will cause subsequent drag/release events to get
    // forwarded to that view.
    if (mouse_pressed_event.handled()) {
      last_click_handler_ = mouse_pressed_handler_;
      DVLOG(1) << "OnMousePressed handled by "
               << mouse_pressed_handler_->GetClassName();
      return true;
    }
  }

  // Reset mouse_pressed_handler_ to indicate that no processing is occurring.
  mouse_pressed_handler_ = nullptr;

  const bool last_click_was_handled = (last_click_handler_ != nullptr);
  last_click_handler_ = nullptr;

  // In the event that a double-click is not handled after traversing the
  // entire hierarchy (even as a single-click when sent to a different view),
  // it must be marked as handled to avoid anything happening from default
  // processing if it the first click-part was handled by us.
  return last_click_was_handled && (event.flags() & ui::EF_IS_DOUBLE_CLICK);
}

bool RootView::OnMouseDragged(const ui::MouseEvent& event) {
  if (mouse_pressed_handler_) {
    SetMouseLocationAndFlags(event);

    ui::MouseEvent mouse_event(event, static_cast<View*>(this),
                               mouse_pressed_handler_.get());
    ui::EventDispatchDetails dispatch_details =
        DispatchEvent(mouse_pressed_handler_, &mouse_event);
    if (dispatch_details.dispatcher_destroyed)
      return false;
    return true;
  }
  return false;
}

void RootView::OnMouseReleased(const ui::MouseEvent& event) {
  UpdateCursor(event);

  if (mouse_pressed_handler_) {
    ui::MouseEvent mouse_released(event, static_cast<View*>(this),
                                  mouse_pressed_handler_.get());
    // We allow the view to delete us from the event dispatch callback. As such,
    // configure state such that we're done first, then call View.
    View* mouse_pressed_handler = mouse_pressed_handler_;

    // During mouse event handling, `SetMouseAndGestureHandler()` may be called
    // to set the gesture handler. Therefore we should reset the gesture handler
    // when mouse is released.
    SetMouseAndGestureHandler(nullptr);
    ui::EventDispatchDetails dispatch_details =
        DispatchEvent(mouse_pressed_handler, &mouse_released);
    if (dispatch_details.dispatcher_destroyed)
      return;
  }
}

void RootView::OnMouseCaptureLost() {
  if (mouse_pressed_handler_ || gesture_handler_) {
    // Synthesize a release event for UpdateCursor.
    if (mouse_pressed_handler_) {
      gfx::Point last_point(last_mouse_event_x_, last_mouse_event_y_);
      ui::MouseEvent release_event(ui::EventType::kMouseReleased, last_point,
                                   last_point, ui::EventTimeForNow(),
                                   last_mouse_event_flags_, 0);
      UpdateCursor(release_event);
    }
    // We allow the view to delete us from OnMouseCaptureLost. As such,
    // configure state such that we're done first, then call View.
    View* mouse_pressed_handler = mouse_pressed_handler_;
    View* gesture_handler = gesture_handler_;
    SetMouseAndGestureHandler(nullptr);
    if (mouse_pressed_handler)
      mouse_pressed_handler->OnMouseCaptureLost();
    else
      gesture_handler->OnMouseCaptureLost();
    // WARNING: we may have been deleted.
  }
}

void RootView::OnMouseMoved(const ui::MouseEvent& event) {
  HandleMouseEnteredOrMoved(event);
}

void RootView::OnMouseEntered(const ui::MouseEvent& event) {
  HandleMouseEnteredOrMoved(event);
}

void RootView::OnMouseExited(const ui::MouseEvent& event) {
  if (mouse_move_handler_ != nullptr) {
    MouseEnterExitEvent exited(event, ui::EventType::kMouseExited);
    ui::EventDispatchDetails dispatch_details =
        DispatchEvent(mouse_move_handler_, &exited);
    if (dispatch_details.dispatcher_destroyed)
      return;
    // The mouse_move_handler_ could have been destroyed in the context of the
    // mouse exit event. b/312400341
    if (!dispatch_details.target_destroyed && mouse_move_handler_) {
      dispatch_details = NotifyEnterExitOfDescendant(
          event, ui::EventType::kMouseExited, mouse_move_handler_, nullptr);
      if (dispatch_details.dispatcher_destroyed)
        return;
    }
    mouse_move_handler_ = nullptr;
  }
}

bool RootView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  for (View* v = GetEventHandlerForPoint(event.location());
       v && v != this && !event.handled(); v = v->parent()) {
    ui::EventDispatchDetails dispatch_details =
        DispatchEvent(v, const_cast<ui::MouseWheelEvent*>(&event));
    if (dispatch_details.dispatcher_destroyed ||
        dispatch_details.target_destroyed) {
      return event.handled();
    }
  }
  return event.handled();
}

void RootView::MaybeNotifyGestureHandlerBeforeReplacement() {
#if defined(USE_AURA)
  ui::GestureRecognizer* gesture_recognizer =
      (gesture_handler_ && widget_ ? widget_->GetGestureRecognizer() : nullptr);
  if (!gesture_recognizer)
    return;

  ui::GestureConsumer* gesture_consumer = widget_->GetGestureConsumer();
  if (!gesture_recognizer->DoesConsumerHaveActiveTouch(gesture_consumer))
    return;

  gesture_recognizer->SendSynthesizedEndEvents(gesture_consumer);
#endif
}

void RootView::SetMouseAndGestureHandler(View* new_handler) {
  SetMouseHandler(new_handler);

  if (new_handler == gesture_handler_)
    return;

  MaybeNotifyGestureHandlerBeforeReplacement();
  gesture_handler_ = new_handler;
}

void RootView::SetMouseHandler(View* new_mouse_handler) {
  // If we're clearing the mouse handler, clear explicit_mouse_handler_ as well.
  explicit_mouse_handler_ = (new_mouse_handler != nullptr);
  mouse_pressed_handler_ = new_mouse_handler;
  drag_info_.Reset();
}

void RootView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);

  DCHECK(GetWidget());
  auto* widget_delegate = GetWidget()->widget_delegate();
  // On linux, we could now get to a situation where when we are first
  // constructing the RootView we try to call GetViewAccessibility.SetRole() and
  // since the VirtualAccessibility object is not yet created, we will try to
  // create one first. This can lead to a crash because on Linux we end up
  // querying GetAccessibleNodeData on the view when we are creating the
  // VirtualAccessibility object, and so we will end up here and if we don't
  // exit early we will try to set the name on an object with no role (we are in
  // the middle of setting it) and so it will crash. This check will prevent us
  // from crashing in that scenario, and will have no other effects since at
  // every other point in time we will have a valid role since its set on the
  // constructor.
  if (!widget_delegate || node_data->role == ax::mojom::Role::kUnknown) {
    return;
  }

  if (node_data->GetStringAttribute(ax::mojom::StringAttribute::kName)
          .empty() &&
      static_cast<ax::mojom::NameFrom>(
          node_data->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom)) !=
          ax::mojom::NameFrom::kAttributeExplicitlyEmpty) {
    std::u16string name = widget_delegate->GetAccessibleWindowTitle();
    if (name.empty()) {
      node_data->SetNameExplicitlyEmpty();
    } else {
      node_data->SetNameChecked(name);
    }
  }
}

void RootView::UpdateParentLayer() {
  if (layer())
    ReparentLayer(widget_->GetLayer());
}

////////////////////////////////////////////////////////////////////////////////
// RootView, protected:

void RootView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  widget_->ViewHierarchyChanged(details);

  if (!details.is_add && !details.move_view) {
    if (mouse_pressed_handler_ == details.child) {
      SetMouseHandler(nullptr);
    }
    if (mouse_move_handler_ == details.child) {
      mouse_move_handler_ = nullptr;
    }
    if (gesture_handler_ == details.child) {
      gesture_handler_ = nullptr;
    }
    if (event_dispatch_target_ == details.child) {
      event_dispatch_target_ = nullptr;
    }
    if (old_dispatch_target_ == details.child) {
      old_dispatch_target_ = nullptr;
    }
  }
}

void RootView::VisibilityChanged(View* /*starting_from*/, bool is_visible) {
  if (!is_visible) {
    // When the root view is being hidden (e.g. when widget is minimized)
    // handlers are reset, so that after it is reshown, events are not captured
    // by old handlers.
    ResetEventHandlers();
  }
}

void RootView::OnDidSchedulePaint(const gfx::Rect& rect) {
  if (!layer()) {
    gfx::Rect xrect = ConvertRectToParent(rect);
    gfx::Rect invalid_rect = gfx::IntersectRects(GetLocalBounds(), xrect);
    if (!invalid_rect.IsEmpty())
      widget_->SchedulePaintInRect(invalid_rect);
  }
}

void RootView::OnPaint(gfx::Canvas* canvas) {
  if (!layer() || !layer()->fills_bounds_opaquely())
    canvas->DrawColor(SK_ColorTRANSPARENT, SkBlendMode::kClear);

  View::OnPaint(canvas);
}

View::LayerOffsetData RootView::CalculateOffsetToAncestorWithLayer(
    ui::Layer** layer_parent) {
  if (layer() || !widget_->GetLayer())
    return View::CalculateOffsetToAncestorWithLayer(layer_parent);
  if (layer_parent)
    *layer_parent = widget_->GetLayer();
  return LayerOffsetData(widget_->GetLayer()->device_scale_factor());
}

View::DragInfo* RootView::GetDragInfo() {
  return &drag_info_;
}

////////////////////////////////////////////////////////////////////////////////
// RootView, private:

void RootView::UpdateCursor(const ui::MouseEvent& event) {
  if (!(event.flags() & ui::EF_IS_NON_CLIENT)) {
    View* v = GetEventHandlerForPoint(event.location());
    ui::MouseEvent me(event, static_cast<View*>(this), v);
    widget_->SetCursor(v->GetCursor(me));
  }
}

void RootView::SetMouseLocationAndFlags(const ui::MouseEvent& event) {
  last_mouse_event_flags_ = event.flags();
  last_mouse_event_x_ = event.x();
  last_mouse_event_y_ = event.y();
}

void RootView::HandleMouseEnteredOrMoved(const ui::MouseEvent& event) {
  View* v = GetEventHandlerForPoint(event.location());
  // Check for a disabled move handler. If the move handler became
  // disabled while handling moves, it's wrong to suddenly send
  // EventType::kMouseExited and EventType::kMouseEntered events, because the
  // mouse hasn't actually exited yet.
  if (mouse_move_handler_ && !mouse_move_handler_->GetEnabled() &&
      v->Contains(mouse_move_handler_)) {
    v = mouse_move_handler_;
  }

  if (v && v != this) {
    if (v != mouse_move_handler_) {
      if (mouse_move_handler_ != nullptr &&
          (!mouse_move_handler_->GetNotifyEnterExitOnChild() ||
           !mouse_move_handler_->Contains(v))) {
        MouseEnterExitEvent exit(event, ui::EventType::kMouseExited);
        exit.ConvertLocationToTarget(static_cast<View*>(this),
                                     mouse_move_handler_.get());
        ui::EventDispatchDetails dispatch_details =
            DispatchEvent(mouse_move_handler_, &exit);
        if (dispatch_details.dispatcher_destroyed) {
          return;
        }
        // The mouse_move_handler_ could have been destroyed in the context of
        // the mouse exit event.
        if (!dispatch_details.target_destroyed) {
          // View was removed by EventType::kMouseExited, or
          // |mouse_move_handler_| was cleared, perhaps by a nested event
          // handler, so return and wait for the next mouse move event.
          if (!mouse_move_handler_) {
            return;
          }
          dispatch_details = NotifyEnterExitOfDescendant(
              event, ui::EventType::kMouseExited, mouse_move_handler_, v);
          if (dispatch_details.dispatcher_destroyed) {
            return;
          }
        }
      }
      View* old_handler = mouse_move_handler_;
      mouse_move_handler_ = v;
      // TODO(crbug.com/40821061): This is for debug purpose only.
      // Remove it after resolving the issue.
      if (!mouse_move_handler_->GetNotifyEnterExitOnChild() ||
          !mouse_move_handler_->Contains(old_handler)) {
        MouseEnterExitEvent entered(event, ui::EventType::kMouseEntered);
        entered.ConvertLocationToTarget(static_cast<View*>(this),
                                        mouse_move_handler_.get());
        ui::EventDispatchDetails dispatch_details =
            DispatchEvent(mouse_move_handler_, &entered);
        if (dispatch_details.dispatcher_destroyed ||
            dispatch_details.target_destroyed) {
          return;
        }
        // View was removed by EventType::kMouseEntered, or
        // |mouse_move_handler_| was cleared, perhaps by a nested event handler,
        // so return and wait for the next mouse move event.
        if (!mouse_move_handler_) {
          return;
        }
        dispatch_details =
            NotifyEnterExitOfDescendant(event, ui::EventType::kMouseEntered,
                                        mouse_move_handler_, old_handler);
        if (dispatch_details.dispatcher_destroyed ||
            dispatch_details.target_destroyed) {
          return;
        }
      }
    }

    if (event.type() == ui::EventType::kMouseMoved) {
      ui::MouseEvent moved_event(event, static_cast<View*>(this),
                                 mouse_move_handler_.get());
      mouse_move_handler_->OnMouseMoved(moved_event);
      // TODO(tdanderson): It may be possible to avoid setting the cursor twice
      //                   (once here and once from CompoundEventFilter) on a
      //                   mousemove. See crbug.com/351469.
      if (!(moved_event.flags() & ui::EF_IS_NON_CLIENT)) {
        widget_->SetCursor(mouse_move_handler_->GetCursor(moved_event));
      }
    }
  } else if (mouse_move_handler_ != nullptr) {
    MouseEnterExitEvent exited(event, ui::EventType::kMouseExited);
    ui::EventDispatchDetails dispatch_details =
        DispatchEvent(mouse_move_handler_, &exited);
    if (dispatch_details.dispatcher_destroyed) {
      return;
    }
    // The mouse_move_handler_ could have been destroyed in the context of the
    // mouse exit event.
    if (!dispatch_details.target_destroyed) {
      // View was removed by EventType::kMouseExited, or |mouse_move_handler_|
      // was cleared, perhaps by a nested event handler, so return and wait for
      // the next mouse move event.
      if (!mouse_move_handler_) {
        return;
      }
      dispatch_details = NotifyEnterExitOfDescendant(
          event, ui::EventType::kMouseExited, mouse_move_handler_, v);
      if (dispatch_details.dispatcher_destroyed) {
        return;
      }
    }
    // On Aura the non-client area extends slightly outside the root view for
    // some windows.  Let the non-client cursor handling code set the cursor
    // as we do above.
    if (!(event.flags() & ui::EF_IS_NON_CLIENT)) {
      widget_->SetCursor(ui::Cursor());
    }
    mouse_move_handler_ = nullptr;
  }
}

ui::EventDispatchDetails RootView::NotifyEnterExitOfDescendant(
    const ui::MouseEvent& event,
    ui::EventType type,
    View* view,
    View* sibling) {
  for (View* p = view->parent(); p; p = p->parent()) {
    if (!p->GetNotifyEnterExitOnChild())
      continue;
    if (sibling && p->Contains(sibling))
      break;
    // It is necessary to recreate the notify-event for each dispatch, since one
    // of the callbacks can mark the event as handled, and that would cause
    // incorrect event dispatch.
    MouseEnterExitEvent notify_event(event, type);
    ui::EventDispatchDetails dispatch_details = DispatchEvent(p, &notify_event);
    if (dispatch_details.dispatcher_destroyed ||
        dispatch_details.target_destroyed) {
      return dispatch_details;
    }
  }
  return ui::EventDispatchDetails();
}

bool RootView::CanDispatchToTarget(ui::EventTarget* target) {
  return event_dispatch_target_ == target;
}

ui::EventDispatchDetails RootView::PreDispatchEvent(ui::EventTarget* target,
                                                    ui::Event* event) {
  View* view = static_cast<View*>(target);
  if (event->IsGestureEvent()) {
    // Update |gesture_handler_| to indicate which View is currently handling
    // gesture events.
    // TODO(tdanderson): Look into moving this to PostDispatchEvent() and
    //                   using |event_dispatch_target_| instead of
    //                   |gesture_handler_| to detect if the view has been
    //                   removed from the tree.
    gesture_handler_ = view;
  }

  old_dispatch_target_ = event_dispatch_target_;
  event_dispatch_target_ = view;
  return DispatchDetails();
}

ui::EventDispatchDetails RootView::PostDispatchEvent(ui::EventTarget* target,
                                                     const ui::Event& event) {
  // The GESTURE_END event corresponding to the removal of the final touch
  // point marks the end of a gesture sequence, so reset |gesture_handler_|
  // to NULL.
  if (event.type() == ui::EventType::kGestureEnd) {
    // In case a drag was in progress, reset all the handlers. Otherwise, just
    // reset the gesture handler.
    if (gesture_handler_ && gesture_handler_ == mouse_pressed_handler_)
      SetMouseAndGestureHandler(nullptr);
    else
      gesture_handler_ = nullptr;
  }

  DispatchDetails details;
  if (target != event_dispatch_target_)
    details.target_destroyed = true;

  event_dispatch_target_ = old_dispatch_target_;
  old_dispatch_target_ = nullptr;

#ifndef NDEBUG
  DCHECK(!event_dispatch_target_ || Contains(event_dispatch_target_));
#endif

  return details;
}

BEGIN_METADATA(RootView)
END_METADATA
}  // namespace views::internal
