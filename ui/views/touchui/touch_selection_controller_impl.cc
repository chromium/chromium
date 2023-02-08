// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_controller_impl.h"

#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

// Constants defining the visual attributes of selection handles

// The distance by which a handle image is offset from the bottom of the
// selection/text baseline.
constexpr int kSelectionHandleVerticalVisualOffset = 2;

// When a handle is dragged, the drag position reported to the client view is
// offset vertically to represent the cursor position. This constant specifies
// the offset in pixels above the bottom of the selection (see pic below). This
// is required because say if this is zero, that means the drag position we
// report is right on the text baseline. In that case, a vertical movement of
// even one pixel will make the handle jump to the line below it. So when the
// user just starts dragging, the handle will jump to the next line if the user
// makes any vertical movement. So we have this non-zero offset to prevent this
// jumping.
//
// Editing handle widget showing the padding and difference between the position
// of the ET_GESTURE_SCROLL_UPDATE event and the drag position reported to the
// client:
//                            ___________
//    Selection Highlight --->_____|__|<-|---- Drag position reported to client
//                              _  |  O  |
//          Vertical Padding __|   |   <-|---- ET_GESTURE_SCROLL_UPDATE position
//                             |_  |_____|<--- Editing handle widget
//
//                                 | |
//                                  T
//                          Horizontal Padding
//
constexpr int kSelectionHandleVerticalDragOffset = 5;

// Padding around the selection handle defining the area that will be included
// in the touch target to make dragging the handle easier (see pic above).
constexpr int kSelectionHandleHorizPadding = 10;
constexpr int kSelectionHandleVertPadding = 20;

// Minimum height for selection handle bar. If the bar height is going to be
// less than this value, handle will not be shown.
constexpr int kSelectionHandleBarMinHeight = 5;
// Maximum amount that selection handle bar can stick out of client view's
// boundaries.
constexpr int kSelectionHandleBarBottomAllowance = 3;

gfx::Image* GetCenterHandleImage() {
  static gfx::Image* handle_image = nullptr;
  if (!handle_image) {
    handle_image = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TEXT_SELECTION_HANDLE_CENTER);
  }
  return handle_image;
}

gfx::Image* GetLeftHandleImage() {
  static gfx::Image* handle_image = nullptr;
  if (!handle_image) {
    handle_image = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TEXT_SELECTION_HANDLE_LEFT);
  }
  return handle_image;
}

gfx::Image* GetRightHandleImage() {
  static gfx::Image* handle_image = nullptr;
  if (!handle_image) {
    handle_image = &ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_TEXT_SELECTION_HANDLE_RIGHT);
  }
  return handle_image;
}

// Return the appropriate handle image based on the bound's type
gfx::Image* GetHandleImage(gfx::SelectionBound::Type bound_type) {
  switch (bound_type) {
    case gfx::SelectionBound::LEFT:
      return GetLeftHandleImage();
    case gfx::SelectionBound::CENTER:
      return GetCenterHandleImage();
    case gfx::SelectionBound::RIGHT:
      return GetRightHandleImage();
    default:
      NOTREACHED_NORETURN()
          << "Invalid touch handle bound type: " << bound_type;
  }
}

// Calculates the bounds of the widget containing the selection handle based
// on the SelectionBound's type and location.
gfx::Rect GetSelectionWidgetBounds(const gfx::SelectionBound& bound) {
  gfx::Size image_size = GetHandleImage(bound.type())->Size();
  int widget_width = image_size.width() + 2 * kSelectionHandleHorizPadding;
  // Extend the widget height to handle touch events below the painted image.
  int widget_height = bound.GetHeight() + image_size.height() +
                      kSelectionHandleVerticalVisualOffset +
                      kSelectionHandleVertPadding;
  // Due to the shape of the handle images, the widget is aligned differently to
  // the selection bound depending on the type of the bound.
  int widget_left = 0;
  switch (bound.type()) {
    case gfx::SelectionBound::LEFT:
      widget_left = bound.edge_start_rounded().x() - image_size.width() -
                    kSelectionHandleHorizPadding;
      break;
    case gfx::SelectionBound::RIGHT:
      widget_left =
          bound.edge_start_rounded().x() - kSelectionHandleHorizPadding;
      break;
    case gfx::SelectionBound::CENTER:
      widget_left = bound.edge_start_rounded().x() - widget_width / 2;
      break;
    default:
      NOTREACHED_NORETURN() << "Undefined bound type.";
  }
  return gfx::Rect(widget_left, bound.edge_start_rounded().y(), widget_width,
                   widget_height);
}

gfx::Size GetMaxHandleImageSize() {
  gfx::Rect center_rect = gfx::Rect(GetCenterHandleImage()->Size());
  gfx::Rect left_rect = gfx::Rect(GetLeftHandleImage()->Size());
  gfx::Rect right_rect = gfx::Rect(GetRightHandleImage()->Size());
  gfx::Rect union_rect = center_rect;
  union_rect.Union(left_rect);
  union_rect.Union(right_rect);
  return union_rect.size();
}

// Convenience methods to convert a |bound| from screen to the |client|'s
// coordinate system and vice versa.
// Note that this is not quite correct because it does not take into account
// transforms such as rotation and scaling. This should be in TouchEditable.
// TODO(varunjain): Fix this.
gfx::SelectionBound ConvertFromScreen(ui::TouchEditable* client,
                                      const gfx::SelectionBound& bound) {
  gfx::SelectionBound result = bound;
  gfx::Point edge_end = bound.edge_end_rounded();
  gfx::Point edge_start = bound.edge_start_rounded();
  client->ConvertPointFromScreen(&edge_end);
  client->ConvertPointFromScreen(&edge_start);
  result.SetEdge(gfx::PointF(edge_start), gfx::PointF(edge_end));
  return result;
}

gfx::SelectionBound ConvertToScreen(ui::TouchEditable* client,
                                    const gfx::SelectionBound& bound) {
  gfx::SelectionBound result = bound;
  gfx::Point edge_end = bound.edge_end_rounded();
  gfx::Point edge_start = bound.edge_start_rounded();
  client->ConvertPointToScreen(&edge_end);
  client->ConvertPointToScreen(&edge_start);
  result.SetEdge(gfx::PointF(edge_start), gfx::PointF(edge_end));
  return result;
}

gfx::Rect BoundToRect(const gfx::SelectionBound& bound) {
  return gfx::BoundingRect(bound.edge_start_rounded(),
                           bound.edge_end_rounded());
}

}  // namespace

namespace views {

using EditingHandleView = TouchSelectionControllerImpl::EditingHandleView;

// A View that displays the text selection handle.
class TouchSelectionControllerImpl::EditingHandleView : public View {
 public:
  METADATA_HEADER(EditingHandleView);
  EditingHandleView(TouchSelectionControllerImpl* controller,
                    gfx::NativeView parent,
                    bool is_cursor_handle)
      : controller_(controller),
        image_(GetCenterHandleImage()),
        is_cursor_handle_(is_cursor_handle),

        widget_(new views::Widget) {
    // Create a widget to host EditingHandleView.
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    params.parent = parent;
    widget_->Init(std::move(params));

    widget_->GetNativeWindow()->SetEventTargeter(
        std::make_unique<aura::WindowTargeter>());
    widget_->SetContentsView(this);
  }

  EditingHandleView(const EditingHandleView&) = delete;
  EditingHandleView& operator=(const EditingHandleView&) = delete;
  ~EditingHandleView() override = default;

  void CloseHandleWidget() {
    SetWidgetVisible(false);
    widget_->CloseNow();
  }

  gfx::SelectionBound::Type GetSelectionBoundType() const {
    return selection_bound_.type();
  }

  // View:
  void OnPaint(gfx::Canvas* canvas) override {
    if (draw_invisible_)
      return;

    // Draw the handle image.
    canvas->DrawImageInt(
        *image_->ToImageSkia(), kSelectionHandleHorizPadding,
        selection_bound_.GetHeight() + kSelectionHandleVerticalVisualOffset);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    event->SetHandled();
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN: {
        widget_->SetCapture(this);
        controller_->OnDragBegin(this);
        // Distance from the point which is |kSelectionHandleVerticalDragOffset|
        // pixels above the bottom of the selection bound edge to the event
        // location (aka the touch-drag point).
        drag_offset_ = selection_bound_.edge_end_rounded() -
                       gfx::Vector2d(0, kSelectionHandleVerticalDragOffset) -
                       event->location();
        break;
      }
      case ui::ET_GESTURE_SCROLL_UPDATE: {
        controller_->OnDragUpdate(event->location() + drag_offset_);
        break;
      }
      case ui::ET_GESTURE_SCROLL_END:
      case ui::ET_SCROLL_FLING_START: {
        widget_->ReleaseCapture();
        controller_->OnDragEnd();
        break;
      }
      default:
        break;
    }
  }

  gfx::Size CalculatePreferredSize() const override {
    // This function will be called during widget initialization, i.e. before
    // SetBoundInScreen has been called. No-op in that case.
    if (selection_bound_.type() == gfx::SelectionBound::EMPTY)
      return gfx::Size();
    return GetSelectionWidgetBounds(selection_bound_).size();
  }

  bool GetWidgetVisible() const { return widget_->IsVisible(); }

  void SetWidgetVisible(bool visible) {
    if (widget_->IsVisible() == visible)
      return;
    if (visible)
      widget_->Show();
    else
      widget_->Hide();
    OnPropertyChanged(&widget_, kPropertyEffectsNone);
  }

  // If |is_visible| is true, this will update the widget and trigger a repaint
  // if necessary. Otherwise this will only update the internal state:
  // |selection_bound_| and |image_|, so that the state is valid for the time
  // this becomes visible.
  void SetBoundInScreen(const gfx::SelectionBound& bound, bool is_visible) {
    bool update_bound_type = false;
    // Cursor handle should always have the bound type CENTER
    DCHECK(!is_cursor_handle_ || bound.type() == gfx::SelectionBound::CENTER);

    if (bound.type() != selection_bound_.type()) {
      // Unless this is a cursor handle, do not set the type to CENTER -
      // selection handles corresponding to a selection should always use left
      // or right handle image. If selection handles are dragged to be located
      // at the same spot, the |bound|'s type here will be CENTER for both of
      // them. In this case do not update the type of the |selection_bound_|.
      if (bound.type() != gfx::SelectionBound::CENTER || is_cursor_handle_)
        update_bound_type = true;
    }
    if (update_bound_type) {
      selection_bound_.set_type(bound.type());
      image_ = GetHandleImage(bound.type());
      if (is_visible)
        SchedulePaint();
    }

    if (is_visible) {
      selection_bound_.SetEdge(bound.edge_start(), bound.edge_end());

      widget_->SetBounds(GetSelectionWidgetBounds(selection_bound_));

      aura::Window* window = widget_->GetNativeView();
      gfx::Point edge_start = selection_bound_.edge_start_rounded();
      gfx::Point edge_end = selection_bound_.edge_end_rounded();
      wm::ConvertPointFromScreen(window, &edge_start);
      wm::ConvertPointFromScreen(window, &edge_end);
      selection_bound_.SetEdge(gfx::PointF(edge_start), gfx::PointF(edge_end));
    }

    const auto insets = gfx::Insets::TLBR(
        selection_bound_.GetHeight() + kSelectionHandleVerticalVisualOffset, 0,
        0, 0);

    // Shifts the hit-test target below the apparent bounds to make dragging
    // easier.
    widget_->GetNativeWindow()->targeter()->SetInsets(insets, insets);
  }

  void SetDrawInvisible(bool draw_invisible) {
    if (draw_invisible_ == draw_invisible)
      return;
    draw_invisible_ = draw_invisible;
    OnPropertyChanged(&draw_invisible_, kPropertyEffectsPaint);
  }
  bool GetDrawInvisible() const { return draw_invisible_; }

 private:
  raw_ptr<TouchSelectionControllerImpl> controller_;

  // In local coordinates
  gfx::SelectionBound selection_bound_;
  raw_ptr<gfx::Image> image_;

  // If true, this is a handle corresponding to the single cursor, otherwise it
  // is a handle corresponding to one of the two selection bounds.
  bool is_cursor_handle_;

  // Offset applied to the scroll events location when calling
  // TouchSelectionControllerImpl::OnDragUpdate while dragging the handle.
  gfx::Vector2d drag_offset_;

  // If set to true, the handle will not draw anything, hence providing an empty
  // widget. We need this because we may want to stop showing the handle while
  // it is being dragged. Since it is being dragged, we cannot destroy the
  // handle.
  bool draw_invisible_ = false;

  // Owning widget.
  Widget* widget_ = nullptr;
};

BEGIN_METADATA(TouchSelectionControllerImpl, EditingHandleView, View)
ADD_READONLY_PROPERTY_METADATA(gfx::SelectionBound::Type, SelectionBoundType)
ADD_PROPERTY_METADATA(bool, WidgetVisible)
ADD_PROPERTY_METADATA(bool, DrawInvisible)
END_METADATA

TouchSelectionControllerImpl::TouchSelectionControllerImpl(
    ui::TouchEditable* client_view)
    : client_view_(client_view),
      selection_handle_1_(
          new EditingHandleView(this, client_view->GetNativeView(), false)),
      selection_handle_2_(
          new EditingHandleView(this, client_view->GetNativeView(), false)),
      cursor_handle_(
          new EditingHandleView(this, client_view->GetNativeView(), true)) {
  selection_start_time_ = base::TimeTicks::Now();
  aura::Window* client_window = client_view_->GetNativeView();
  client_widget_ = Widget::GetTopLevelWidgetForNativeView(client_window);
  // Observe client widget moves and resizes to update the selection handles.
  if (client_widget_)
    client_widget_->AddObserver(this);

  // Observe certain event types sent to any event target, to hide this ui.
  aura::Env* env = aura::Env::GetInstance();
  std::set<ui::EventType> types = {ui::ET_MOUSE_PRESSED, ui::ET_MOUSE_MOVED,
                                   ui::ET_KEY_PRESSED, ui::ET_MOUSEWHEEL};
  env->AddEventObserver(this, env, types);
}

TouchSelectionControllerImpl::~TouchSelectionControllerImpl() {
  UMA_HISTOGRAM_BOOLEAN("Event.TouchSelection.EndedWithAction",
                        command_executed_);
  HideQuickMenu();
  aura::Env::GetInstance()->RemoveEventObserver(this);
  if (client_widget_)
    client_widget_->RemoveObserver(this);
  // Close the owning Widgets to clean up the EditingHandleViews.
  selection_handle_1_->CloseHandleWidget();
  selection_handle_2_->CloseHandleWidget();
  cursor_handle_->CloseHandleWidget();
  CHECK(!IsInObserverList());
}

void TouchSelectionControllerImpl::SelectionChanged() {
  gfx::SelectionBound anchor, focus;
  client_view_->GetSelectionEndPoints(&anchor, &focus);
  gfx::SelectionBound screen_bound_anchor =
      ConvertToScreen(client_view_, anchor);
  gfx::SelectionBound screen_bound_focus = ConvertToScreen(client_view_, focus);
  gfx::Rect client_bounds = client_view_->GetBounds();
  if (anchor.edge_start().y() < client_bounds.y()) {
    auto anchor_edge_start = gfx::PointF(anchor.edge_start_rounded());
    anchor_edge_start.set_y(client_bounds.y());
    anchor.SetEdgeStart(anchor_edge_start);
  }
  if (focus.edge_start().y() < client_bounds.y()) {
    auto focus_edge_start = gfx::PointF(focus.edge_start_rounded());
    focus_edge_start.set_y(client_bounds.y());
    focus.SetEdgeStart(focus_edge_start);
  }
  gfx::SelectionBound screen_bound_anchor_clipped =
      ConvertToScreen(client_view_, anchor);
  gfx::SelectionBound screen_bound_focus_clipped =
      ConvertToScreen(client_view_, focus);
  if (screen_bound_anchor_clipped == selection_bound_1_clipped_ &&
      screen_bound_focus_clipped == selection_bound_2_clipped_)
    return;

  selection_bound_1_ = screen_bound_anchor;
  selection_bound_2_ = screen_bound_focus;
  selection_bound_1_clipped_ = screen_bound_anchor_clipped;
  selection_bound_2_clipped_ = screen_bound_focus_clipped;

  if (client_view_->DrawsHandles()) {
    UpdateQuickMenu();
    return;
  }

  if (dragging_handle_) {
    // We need to reposition only the selection handle that is being dragged.
    // The other handle stays the same. Also, the selection handle being dragged
    // will always be at the end of selection, while the other handle will be at
    // the start.
    // If the new location of this handle is out of client view, its widget
    // should not get hidden, since it should still receive touch events.
    // Hence, we are not using |SetHandleBound()| method here.
    dragging_handle_->SetBoundInScreen(screen_bound_focus_clipped, true);

    // Temporary fix for selection handle going outside a window. On a webpage,
    // the page should scroll if the selection handle is dragged outside the
    // window. That does not happen currently. So we just hide the handle for
    // now.
    // TODO(varunjain): Fix this: crbug.com/269003
    dragging_handle_->SetDrawInvisible(!ShouldShowHandleFor(focus));

    if (dragging_handle_ != cursor_handle_) {
      // The non-dragging-handle might have recently become visible.
      EditingHandleView* non_dragging_handle = selection_handle_1_;
      if (dragging_handle_ == selection_handle_1_) {
        non_dragging_handle = selection_handle_2_;
        // if handle 1 is being dragged, it is corresponding to the end of
        // selection and the other handle to the start of selection.
        selection_bound_1_ = screen_bound_focus;
        selection_bound_2_ = screen_bound_anchor;
        selection_bound_1_clipped_ = screen_bound_focus_clipped;
        selection_bound_2_clipped_ = screen_bound_anchor_clipped;
      }
      SetHandleBound(non_dragging_handle, anchor, screen_bound_anchor_clipped);
    }
  } else {
    UpdateQuickMenu();

    // Check if there is any selection at all.
    if (screen_bound_anchor.edge_start() == screen_bound_focus.edge_start() &&
        screen_bound_anchor.edge_end() == screen_bound_focus.edge_end()) {
      selection_handle_1_->SetWidgetVisible(false);
      selection_handle_2_->SetWidgetVisible(false);
      SetHandleBound(cursor_handle_, anchor, screen_bound_anchor_clipped);
      return;
    }

    cursor_handle_->SetWidgetVisible(false);
    SetHandleBound(selection_handle_1_, anchor, screen_bound_anchor_clipped);
    SetHandleBound(selection_handle_2_, focus, screen_bound_focus_clipped);
  }
}

void TouchSelectionControllerImpl::ShowQuickMenuImmediatelyForTesting() {
  if (quick_menu_timer_.IsRunning()) {
    quick_menu_timer_.Stop();
    QuickMenuTimerFired();
  }
}

void TouchSelectionControllerImpl::OnDragBegin(EditingHandleView* handle) {
  dragging_handle_ = handle;
  HideQuickMenu();
  if (dragging_handle_ == cursor_handle_) {
    return;
  }

  DCHECK(dragging_handle_ == selection_handle_1_ ||
         dragging_handle_ == selection_handle_2_);

  // Find selection end points in client_view's coordinate system.
  gfx::Point base = selection_bound_1_.edge_start_rounded();
  base.Offset(0, selection_bound_1_.GetHeight() / 2);
  client_view_->ConvertPointFromScreen(&base);

  gfx::Point extent = selection_bound_2_.edge_start_rounded();
  extent.Offset(0, selection_bound_2_.GetHeight() / 2);
  client_view_->ConvertPointFromScreen(&extent);

  if (dragging_handle_ == selection_handle_1_) {
    std::swap(base, extent);
  }

  // When moving the handle we want to move only the extent point. Before
  // doing so we must make sure that the base point is set correctly.
  client_view_->SelectBetweenCoordinates(base, extent);
}

void TouchSelectionControllerImpl::OnDragUpdate(const gfx::Point& drag_pos) {
  DCHECK(dragging_handle_);
  gfx::Point drag_pos_in_client = drag_pos;
  ConvertPointToClientView(dragging_handle_, &drag_pos_in_client);

  if (dragging_handle_ == cursor_handle_) {
    client_view_->MoveCaret(drag_pos_in_client);
    return;
  }

  DCHECK(dragging_handle_ == selection_handle_1_ ||
         dragging_handle_ == selection_handle_2_);
  client_view_->MoveRangeSelectionExtent(drag_pos_in_client);
}

void TouchSelectionControllerImpl::OnDragEnd() {
  dragging_handle_ = nullptr;
  StartQuickMenuTimer();
}

void TouchSelectionControllerImpl::ConvertPointToClientView(
    EditingHandleView* source,
    gfx::Point* point) {
  View::ConvertPointToScreen(source, point);
  client_view_->ConvertPointFromScreen(point);
}

void TouchSelectionControllerImpl::SetHandleBound(
    EditingHandleView* handle,
    const gfx::SelectionBound& bound,
    const gfx::SelectionBound& bound_in_screen) {
  handle->SetWidgetVisible(ShouldShowHandleFor(bound));
  handle->SetBoundInScreen(bound_in_screen, handle->GetWidgetVisible());
}

bool TouchSelectionControllerImpl::ShouldShowHandleFor(
    const gfx::SelectionBound& bound) const {
  if (bound.GetHeight() < kSelectionHandleBarMinHeight)
    return false;
  gfx::Rect client_bounds = client_view_->GetBounds();
  client_bounds.Inset(
      gfx::Insets::TLBR(0, 0, -kSelectionHandleBarBottomAllowance, 0));
  return client_bounds.Contains(BoundToRect(bound));
}

bool TouchSelectionControllerImpl::IsCommandIdEnabled(int command_id) const {
  return client_view_->IsCommandIdEnabled(command_id);
}

void TouchSelectionControllerImpl::ExecuteCommand(int command_id,
                                                  int event_flags) {
  command_executed_ = true;
  base::TimeDelta duration = base::TimeTicks::Now() - selection_start_time_;
  // Note that we only log the duration stats for the 'successful' selections,
  // i.e. selections ending with the execution of a command.
  UMA_HISTOGRAM_CUSTOM_TIMES("Event.TouchSelection.Duration", duration,
                             base::Milliseconds(500), base::Seconds(60), 60);
  client_view_->ExecuteCommand(command_id, event_flags);
}

void TouchSelectionControllerImpl::RunContextMenu() {
  // Context menu should appear centered on top of the selected region.
  const gfx::Rect rect = GetQuickMenuAnchorRect();
  const gfx::Point anchor(rect.CenterPoint().x(), rect.y());
  client_view_->OpenContextMenu(anchor);
}

bool TouchSelectionControllerImpl::ShouldShowQuickMenu() {
  return false;
}

std::u16string TouchSelectionControllerImpl::GetSelectedText() {
  return std::u16string();
}

void TouchSelectionControllerImpl::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(client_widget_, widget);
  client_widget_->RemoveObserver(this);
  client_widget_ = nullptr;
}

void TouchSelectionControllerImpl::OnWidgetBoundsChanged(
    Widget* widget,
    const gfx::Rect& new_bounds) {
  DCHECK_EQ(client_widget_, widget);
  SelectionChanged();
}

void TouchSelectionControllerImpl::OnEvent(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    auto* cursor = aura::client::GetCursorClient(
        client_view_->GetNativeView()->GetRootWindow());
    if (cursor && !cursor->IsMouseEventsEnabled())
      return;

    // Windows OS unhandled WM_POINTER* may be redispatched as WM_MOUSE*.
    // Avoid adjusting the handles on synthesized events or events generated
    // from touch as this can clear an active selection generated by the pen.
    if ((event.flags() & (int{ui::EF_IS_SYNTHESIZED} | ui::EF_FROM_TOUCH)) ||
        event.AsMouseEvent()->pointer_details().pointer_type ==
            ui::EventPointerType::kPen) {
      return;
    }
  }

  client_view_->DestroyTouchSelection();
}

void TouchSelectionControllerImpl::QuickMenuTimerFired() {
  gfx::Rect menu_anchor = GetQuickMenuAnchorRect();
  if (menu_anchor == gfx::Rect())
    return;

  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      GetWeakPtr(), menu_anchor, GetMaxHandleImageSize(),
      client_view_->GetNativeView());
}

void TouchSelectionControllerImpl::StartQuickMenuTimer() {
  if (quick_menu_timer_.IsRunning())
    return;
  quick_menu_timer_.Start(FROM_HERE, base::Milliseconds(200), this,
                          &TouchSelectionControllerImpl::QuickMenuTimerFired);
}

void TouchSelectionControllerImpl::UpdateQuickMenu() {
  // Hide quick menu to be shown when the timer fires.
  HideQuickMenu();
  StartQuickMenuTimer();
}

void TouchSelectionControllerImpl::HideQuickMenu() {
  if (ui::TouchSelectionMenuRunner::GetInstance()->IsRunning())
    ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  quick_menu_timer_.Stop();
}

gfx::Rect TouchSelectionControllerImpl::GetQuickMenuAnchorRect() const {
  // Get selection end points in client_view's space.
  gfx::SelectionBound b1_in_screen = selection_bound_1_clipped_;
  gfx::SelectionBound b2_in_screen = cursor_handle_->GetWidgetVisible()
                                         ? b1_in_screen
                                         : selection_bound_2_clipped_;
  // Convert from screen to client.
  gfx::SelectionBound b1 = ConvertFromScreen(client_view_, b1_in_screen);
  gfx::SelectionBound b2 = ConvertFromScreen(client_view_, b2_in_screen);

  // if selection is completely inside the view, we display the quick menu in
  // the middle of the end points on the top. Else, we show it above the visible
  // handle. If no handle is visible, we do not show the menu.
  gfx::Rect menu_anchor;
  if (ShouldShowHandleFor(b1) && ShouldShowHandleFor(b2))
    menu_anchor = gfx::RectBetweenSelectionBounds(b1_in_screen, b2_in_screen);
  else if (ShouldShowHandleFor(b1))
    menu_anchor = BoundToRect(b1_in_screen);
  else if (ShouldShowHandleFor(b2))
    menu_anchor = BoundToRect(b2_in_screen);
  else
    return menu_anchor;

  // Enlarge the anchor rect so that the menu is offset from the text at least
  // by the same distance the handles are offset from the text.
  menu_anchor.Inset(gfx::Insets::VH(-kSelectionHandleVerticalVisualOffset, 0));

  return menu_anchor;
}

gfx::NativeView TouchSelectionControllerImpl::GetCursorHandleNativeView() {
  return cursor_handle_->GetWidget()->GetNativeView();
}

gfx::SelectionBound::Type
TouchSelectionControllerImpl::GetSelectionHandle1Type() {
  return selection_handle_1_->GetSelectionBoundType();
}

gfx::Rect TouchSelectionControllerImpl::GetSelectionHandle1Bounds() {
  return selection_handle_1_->GetBoundsInScreen();
}

gfx::Rect TouchSelectionControllerImpl::GetSelectionHandle2Bounds() {
  return selection_handle_2_->GetBoundsInScreen();
}

gfx::Rect TouchSelectionControllerImpl::GetCursorHandleBounds() {
  return cursor_handle_->GetBoundsInScreen();
}

bool TouchSelectionControllerImpl::IsSelectionHandle1Visible() {
  return selection_handle_1_->GetWidgetVisible();
}

bool TouchSelectionControllerImpl::IsSelectionHandle2Visible() {
  return selection_handle_2_->GetWidgetVisible();
}

bool TouchSelectionControllerImpl::IsCursorHandleVisible() {
  return cursor_handle_->GetWidgetVisible();
}

gfx::Rect TouchSelectionControllerImpl::GetExpectedHandleBounds(
    const gfx::SelectionBound& bound) {
  return GetSelectionWidgetBounds(bound);
}

View* TouchSelectionControllerImpl::GetHandle1View() {
  return selection_handle_1_;
}

View* TouchSelectionControllerImpl::GetHandle2View() {
  return selection_handle_2_;
}

}  // namespace views

DEFINE_ENUM_CONVERTERS(gfx::SelectionBound::Type,
                       {gfx::SelectionBound::Type::LEFT, u"LEFT"},
                       {gfx::SelectionBound::Type::RIGHT, u"RIGHT"},
                       {gfx::SelectionBound::Type::CENTER, u"CENTER"},
                       {gfx::SelectionBound::Type::EMPTY, u"EMPTY"})
