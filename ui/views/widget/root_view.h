// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_ROOT_VIEW_H_
#define UI_VIEWS_WIDGET_ROOT_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/events/event_processor.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {

namespace test {
class ViewTargeterTest;
class WidgetTest;
}  // namespace test

class RootViewTargeter;
class Widget;

// This is a views-internal API and should not be used externally.
// Widget exposes this object as a View*.
namespace internal {
class AnnounceTextView;
class PreEventDispatchHandler;

////////////////////////////////////////////////////////////////////////////////
// RootView class
//
// The RootView is the root of a View hierarchy. A RootView is attached to a
// Widget. The Widget is responsible for receiving events from the host
// environment, converting them to views-compatible events and then forwarding
// them to the RootView for propagation into the View hierarchy.
//
// A RootView can have only one child, called its "Contents View" which is
// sized to fill the bounds of the RootView (and hence the client area of the
// Widget). Call SetContentsView() after the associated Widget has been
// initialized to attach the contents view to the RootView.
// TODO(beng): Enforce no other callers to AddChildView/tree functions by
//             overriding those methods as private here.
// TODO(beng): Clean up API further, make Widget a friend.
// TODO(sky): We don't really want to export this class.
//
class VIEWS_EXPORT RootView : public View,
                              public ViewTargeterDelegate,
                              public FocusTraversable,
                              public ui::EventProcessor {
  METADATA_HEADER(RootView, View)

 public:
  // Creation and lifetime -----------------------------------------------------
  explicit RootView(Widget* widget);
  RootView(const RootView&) = delete;
  RootView& operator=(const RootView&) = delete;
  ~RootView() override;

  // Tree operations -----------------------------------------------------------

  // Sets the "contents view" of the RootView. This is the single child view
  // that is responsible for laying out the contents of the widget.
  void SetContentsView(View* contents_view);
  View* GetContentsView();

  // Called when parent of the host changed.
  void NotifyNativeViewHierarchyChanged();

  // Focus ---------------------------------------------------------------------

  // Used to set the FocusTraversable parent after the view has been created
  // (typically when the hierarchy changes and this RootView is added/removed).
  virtual void SetFocusTraversableParent(FocusTraversable* focus_traversable);

  // Used to set the View parent after the view has been created.
  virtual void SetFocusTraversableParentView(View* view);

  // System events -------------------------------------------------------------

  // Public API for broadcasting theme change notifications to this View
  // hierarchy.
  void ThemeChanged();

  // Used to clear event handlers so events aren't captured by old event
  // handlers, e.g., when the widget is minimized.
  void ResetEventHandlers();

  // Public API for broadcasting device scale factor change notifications to
  // this View hierarchy.
  void DeviceScaleFactorChanged(float old_device_scale_factor,
                                float new_device_scale_factor);

  // Accessibility -------------------------------------------------------------

  // See AXPlatformNode::AnnounceTextAs for documentation of this.
  void AnnounceTextAs(const std::u16string& text,
                      ui::AXPlatformNode::AnnouncementType announcement_type);
  View* GetAnnounceViewForTesting();

  // FocusTraversable:
  FocusSearch* GetFocusSearch() override;
  FocusTraversable* GetFocusTraversableParent() override;
  View* GetFocusTraversableParentView() override;

  // ui::EventProcessor:
  ui::EventTarget* GetRootForEvent(ui::Event* event) override;
  ui::EventTargeter* GetDefaultEventTargeter() override;
  void OnEventProcessingStarted(ui::Event* event) override;
  void OnEventProcessingFinished(ui::Event* event) override;

  // View:
  const Widget* GetWidget() const override;
  Widget* GetWidget() override;
  bool IsDrawn() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void SetMouseAndGestureHandler(View* new_handler) override;
  void SetMouseHandler(View* new_mouse_handler) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void UpdateParentLayer() override;

  const views::View* gesture_handler_for_testing() const {
    return gesture_handler_;
  }
  const views::View* mouse_pressed_handler_for_testing() const {
    return mouse_pressed_handler_.get();
  }

 protected:
  // View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnDidSchedulePaint(const gfx::Rect& rect) override;
  void OnPaint(gfx::Canvas* canvas) override;
  View::LayerOffsetData CalculateOffsetToAncestorWithLayer(
      ui::Layer** layer_parent) override;
  View::DragInfo* GetDragInfo() override;

 private:
  friend class ::views::RootViewTargeter;
  friend class ::views::View;
  friend class ::views::Widget;
  friend class ::views::test::ViewTargeterTest;
  friend class ::views::test::WidgetTest;

  // Input ---------------------------------------------------------------------

  // Update the cursor given a mouse event. This is called by non mouse_move
  // event handlers to honor the cursor desired by views located under the
  // cursor during drag operations. The location of the mouse should be in the
  // current coordinate system (i.e. any necessary transformation should be
  // applied to the point prior to calling this).
  void UpdateCursor(const ui::MouseEvent& event);

  // Updates the last_mouse_* fields from e. The location of the mouse should be
  // in the current coordinate system (i.e. any necessary transformation should
  // be applied to the point prior to calling this).
  void SetMouseLocationAndFlags(const ui::MouseEvent& event);

  // Returns announce_view_, a hidden view used to make announcements to the
  // screen reader via an alert or live region update.
  AnnounceTextView* GetOrCreateAnnounceView();

  // EventType::kMouseEntered events require the same handling as
  // EventType::kMouseMoved, except that for the former we don't send
  // EventType::kMouseMoved to |mouse_move_handler_|.
  void HandleMouseEnteredOrMoved(const ui::MouseEvent& event);

  // |view| is the view receiving |event|. This function sends the event to all
  // the Views up the hierarchy that has |notify_enter_exit_on_child_| flag
  // turned on, but does not contain |sibling|.
  [[nodiscard]] ui::EventDispatchDetails NotifyEnterExitOfDescendant(
      const ui::MouseEvent& event,
      ui::EventType type,
      View* view,
      View* sibling);

  // Send synthesized gesture end events to `gesture_handler` before replacement
  // if `gesture_handler` is in progress of gesture handling.
  void MaybeNotifyGestureHandlerBeforeReplacement();

  // ui::EventDispatcherDelegate:
  bool CanDispatchToTarget(ui::EventTarget* target) override;
  ui::EventDispatchDetails PreDispatchEvent(ui::EventTarget* target,
                                            ui::Event* event) override;
  ui::EventDispatchDetails PostDispatchEvent(ui::EventTarget* target,
                                             const ui::Event& event) override;

  //////////////////////////////////////////////////////////////////////////////
  // Tree operations -----------------------------------------------------------

  // The host Widget
  const raw_ptr<Widget, DanglingUntriaged> widget_;

  // Input ---------------------------------------------------------------------

  // TODO(tdanderson): Consider moving the input-related members into
  //                   ViewTargeter / RootViewTargeter.

  // The view currently handing down - drag - up
  raw_ptr<View> mouse_pressed_handler_ = nullptr;

  // The view currently handling enter / exit
  raw_ptr<View, AcrossTasksDanglingUntriaged> mouse_move_handler_ = nullptr;

  // The last view to handle a mouse click, so that we can determine if
  // a double-click lands on the same view as its single-click part.
  raw_ptr<View, AcrossTasksDanglingUntriaged> last_click_handler_ = nullptr;

  // true if mouse_pressed_handler_ has been explicitly set
  bool explicit_mouse_handler_ = false;

  // Last position/flag of a mouse press/drag. Used if capture stops and we need
  // to synthesize a release.
  int last_mouse_event_flags_ = 0;
  int last_mouse_event_x_ = -1;
  int last_mouse_event_y_ = -1;

  // The View currently handling gesture events.
  raw_ptr<View, AcrossTasksDanglingUntriaged> gesture_handler_ = nullptr;

  // Used to indicate if the |gesture_handler_| member was set prior to the
  // processing of the current event (i.e., if |gesture_handler_| was set
  // by the dispatch of a previous gesture event).
  // TODO(tdanderson): It may be possible to eliminate the need for this
  //                   member if |event_dispatch_target_| can be used in
  //                   its place.
  bool gesture_handler_set_before_processing_ = false;

  std::unique_ptr<internal::PreEventDispatchHandler> pre_dispatch_handler_;
  std::unique_ptr<internal::PostEventDispatchHandler> post_dispatch_handler_;

  // Focus ---------------------------------------------------------------------

  // The focus search algorithm.
  FocusSearch focus_search_{this, false, false};

  // Whether this root view belongs to the current active window.
  // bool activated_;

  // The parent FocusTraversable, used for focus traversal.
  raw_ptr<FocusTraversable, AcrossTasksDanglingUntriaged>
      focus_traversable_parent_ = nullptr;

  // The View that contains this RootView. This is used when we have RootView
  // wrapped inside native components, and is used for the focus traversal.
  raw_ptr<View, AcrossTasksDanglingUntriaged> focus_traversable_parent_view_ =
      nullptr;

  raw_ptr<View, AcrossTasksDanglingUntriaged> event_dispatch_target_ = nullptr;
  raw_ptr<View, AcrossTasksDanglingUntriaged> old_dispatch_target_ = nullptr;

  // Drag and drop -------------------------------------------------------------

  // Tracks drag state for a view.
  View::DragInfo drag_info_;

  // Accessibility -------------------------------------------------------------

  // Hidden view used to make announcements to the screen reader via an alert or
  // live region update.
  raw_ptr<AnnounceTextView, AcrossTasksDanglingUntriaged> announce_view_ =
      nullptr;
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_WIDGET_ROOT_VIEW_H_
