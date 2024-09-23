// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
#define UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/selection_bound.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/views/touchui/touch_selection_controller.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class TouchSelectionMagnifierAura;
}

namespace views {

// Responsible for displaying selection handles and menu elements relevant in a
// touch interface.
class VIEWS_EXPORT TouchSelectionControllerImpl
    : public TouchSelectionController,
      public ui::TouchSelectionMenuClient,
      public WidgetObserver,
      public ui::EventObserver {
 public:
  class EditingHandleView;

  explicit TouchSelectionControllerImpl(ui::TouchEditable* client_view);
  TouchSelectionControllerImpl(const TouchSelectionControllerImpl&) = delete;
  TouchSelectionControllerImpl& operator=(const TouchSelectionControllerImpl&) =
      delete;
  ~TouchSelectionControllerImpl() override;

  // TouchSelectionController:
  void SelectionChanged() override;
  void ToggleQuickMenu() override;

  void ShowQuickMenuImmediatelyForTesting();

 private:
  friend class TouchSelectionControllerImplTest;

  // Callbacks to inform the client view of handle drag events, so that the
  // client view can perform selection updates if needed. `drag_pos` is the new
  // position for the bottom of the selection bound corresponding to the handle
  // currently being dragged, specified in the handle's coordinates.
  void OnDragBegin(EditingHandleView* handle);
  void OnDragUpdate(EditingHandleView* handle, const gfx::Point& drag_pos);
  void OnDragEnd();

  // Convenience method to convert a point from a selection handle's coordinate
  // system to that of the client view.
  void ConvertPointToClientView(EditingHandleView* source, gfx::Point* point);

  // Convenience method to set a handle's selection bound and hide it if it is
  // located out of client view.
  void SetHandleBound(EditingHandleView* handle,
                      const gfx::SelectionBound& bound,
                      const gfx::SelectionBound& bound_in_screen);

  // Checks if handle should be shown for selection bound.
  // |bound| should be the clipped version of the selection bound.
  bool ShouldShowHandleFor(const gfx::SelectionBound& bound) const;

  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void RunContextMenu() override;
  bool ShouldShowQuickMenu() override;
  std::u16string GetSelectedText() override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;
  void OnWidgetBoundsChanged(Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // Time to show quick menu.
  void QuickMenuTimerFired();

  void StartQuickMenuTimer();

  // Convenience method to update the position/visibility of the quick menu.
  void UpdateQuickMenu();

  // Convenience method for hiding quick menu.
  void HideQuickMenu();

  // Convenience method to calculate anchor rect for quick menu, in screen
  // coordinates.
  gfx::Rect GetQuickMenuAnchorRect() const;

  // Shows the touch selection magnifier (if there is one) at the focus bound.
  void ShowMagnifier(const gfx::SelectionBound& focus_bound_in_screen);

  // Hides the touch selection magnifier.
  void HideMagnifier();

  // Creates widgets for the selection handles and cursor handle.
  void CreateHandleWidgets();

  // Gets the contents views of the handle widgets. Returns nullptr if the
  // handle widget has been closed.
  EditingHandleView* GetSelectionHandle1();
  EditingHandleView* GetSelectionHandle2();
  EditingHandleView* GetCursorHandle();

  // Gets the handle that is currently being dragged, or nullptr if no handle is
  // being dragged.
  EditingHandleView* GetDraggingHandle();

  // Convenience methods for testing.
  gfx::NativeView GetCursorHandleNativeView();
  gfx::SelectionBound::Type GetSelectionHandle1Type();
  gfx::Rect GetSelectionHandle1Bounds();
  gfx::Rect GetSelectionHandle2Bounds();
  gfx::Rect GetCursorHandleBounds();
  bool IsSelectionHandle1Visible();
  bool IsSelectionHandle2Visible();
  bool IsCursorHandleVisible();
  gfx::Rect GetExpectedHandleBounds(const gfx::SelectionBound& bound);
  View* GetHandle1View();
  View* GetHandle2View();

  raw_ptr<ui::TouchEditable> client_view_ = nullptr;
  raw_ptr<Widget> client_widget_ = nullptr;

  // Widgets for the selection handles and cursor handle.
  std::unique_ptr<Widget> selection_handle_1_widget_;
  std::unique_ptr<Widget> selection_handle_2_widget_;
  std::unique_ptr<Widget> cursor_handle_widget_;

  // Magnifier which is shown when touch dragging to adjust the selection.
  std::unique_ptr<ui::TouchSelectionMagnifierAura> touch_selection_magnifier_;

  // Whether to enable toggling the menu by tapping the cursor or cursor handle.
  // If enabled, the menu defaults to being hidden when the cursor handle is
  // initially created.
  bool toggle_menu_enabled_ = false;

  // Whether the quick menu has been requested to be shown.
  bool quick_menu_requested_ = false;

  // Timer to trigger quick menu after it has been requested. If a touch handle
  // is being dragged, the menu will be hidden and the timer will only start
  // after the drag is lifted.
  base::OneShotTimer quick_menu_timer_;

  // In cursor mode, the two selection bounds are the same and correspond to
  // |cursor_handle_|; otherwise, they correspond to |selection_handle_1_| and
  // |selection_handle_2_|, respectively. These values should be used when
  // selection bounds needed rather than position of handles which might be
  // invalid when handles are hidden.
  gfx::SelectionBound selection_bound_1_;
  gfx::SelectionBound selection_bound_2_;

  // Selection bounds, clipped to client view's boundaries.
  gfx::SelectionBound selection_bound_1_clipped_;
  gfx::SelectionBound selection_bound_2_clipped_;

  // Used to track whether the client is selection dragging. If the client's
  // selection dragging state changes, then the handles need to be updated on
  // the next selection change notification.
  bool is_client_selection_dragging_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_IMPL_H_
