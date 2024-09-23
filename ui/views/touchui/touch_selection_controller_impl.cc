// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_controller_impl.h"

#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/touch_selection/touch_selection_magnifier_aura.h"
#include "ui/touch_selection/touch_selection_metrics.h"
#include "ui/touch_selection/vector_icons/vector_icons.h"
#include "ui/views/view_utils.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

// Constants defining the visual attributes of selection handles

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
// of the EventType::kGestureScrollUpdate event and the drag position reported
// to the client:
//                            ___________
//    Selection Highlight --->_____|__|<-|---- Drag position reported to client
//                              _  |  O  |
//            Bottom Padding __|   |   <-|---- EventType::kGestureScrollUpdate
//            position
//                             |_  |_____|<--- Editing handle widget
//
//                                 | |
//                                  T
//                          Horizontal Padding
//
constexpr int kSelectionHandleVerticalDragOffset = 5;

// Minimum height for selection handle bar. If the bar height is going to be
// less than this value, handle will not be shown.
constexpr int kSelectionHandleBarMinHeight = 5;
// Maximum amount that selection handle bar can stick out of client view's
// boundaries.
constexpr int kSelectionHandleBarBottomAllowance = 3;

// Opacity of the selection handle image.
constexpr float kSelectionHandleOpacity = 0.8f;

// Delay before showing the quick menu after it is requested, in milliseconds.
constexpr int kQuickMenuDelayInMs = 200;

// Vertical offset to apply from the bottom of the selection/text baseline to
// the top of the handle image.
constexpr int kSelectionHandleVerticalOffset = 2;
int GetSelectionHandleVerticalOffset() {
  return ::features::IsTouchTextEditingRedesignEnabled()
             ? 0
             : kSelectionHandleVerticalOffset;
}

// Padding to apply horizontally around and vertically below the handle image.
// This is included in the touch handle target area to make dragging the handle
// easier (see pic above).
constexpr int kSelectionHandleHorizontalPadding = 10;
constexpr int kSelectionHandleBottomPadding = 20;
constexpr int kSelectionHandlePadding = 6;
int GetSelectionHandleHorizontalPadding() {
  return ::features::IsTouchTextEditingRedesignEnabled()
             ? kSelectionHandlePadding
             : kSelectionHandleHorizontalPadding;
}
int GetSelectionHandleBottomPadding() {
  return ::features::IsTouchTextEditingRedesignEnabled()
             ? kSelectionHandlePadding
             : kSelectionHandleBottomPadding;
}

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
      NOTREACHED() << "Invalid touch handle bound type: " << bound_type;
  }
}

// Returns the appropriate handle vector icon based on the handle bound type.
ui::ImageModel GetHandleVectorIcon(gfx::SelectionBound::Type bound_type) {
  const gfx::VectorIcon* icon = nullptr;
  switch (bound_type) {
    case gfx::SelectionBound::LEFT:
      icon = &ui::kTextSelectionHandleLeftIcon;
      break;
    case gfx::SelectionBound::CENTER:
      icon = &ui::kTextSelectionHandleCenterIcon;
      break;
    case gfx::SelectionBound::RIGHT:
      icon = &ui::kTextSelectionHandleRightIcon;
      break;
    default:
      NOTREACHED() << "Invalid touch handle bound type: " << bound_type;
  }
  return ui::ImageModel::FromVectorIcon(*icon,
                                        /*color_id=*/ui::kColorSysPrimary);
}

// Returns the appropriate handle image model based on the handle bound type.
ui::ImageModel GetHandleImageModel(gfx::SelectionBound::Type bound_type) {
  return ::features::IsTouchTextEditingRedesignEnabled()
             ? GetHandleVectorIcon(bound_type)
             : ui::ImageModel::FromImage(*GetHandleImage(bound_type));
  ;
}

// Calculates the bounds of the widget containing the selection handle based
// on the SelectionBound's type and location.
gfx::Rect GetSelectionWidgetBounds(const gfx::SelectionBound& bound) {
  const gfx::Size image_size = GetHandleImageModel(bound.type()).Size();
  const int widget_width =
      image_size.width() + 2 * GetSelectionHandleHorizontalPadding();
  // Extend the widget height to handle touch events below the painted image.
  const int widget_height = bound.GetHeight() + image_size.height() +
                            GetSelectionHandleVerticalOffset() +
                            GetSelectionHandleBottomPadding();

  // Due to the shape of the handle images, the widget is aligned differently to
  // the selection bound depending on the type of the bound.
  int widget_left = 0;
  switch (bound.type()) {
    case gfx::SelectionBound::LEFT:
      widget_left = bound.edge_start_rounded().x() - image_size.width() -
                    GetSelectionHandleHorizontalPadding();
      break;
    case gfx::SelectionBound::RIGHT:
      widget_left = bound.edge_start_rounded().x() -
                    GetSelectionHandleHorizontalPadding();
      break;
    case gfx::SelectionBound::CENTER:
      widget_left = bound.edge_start_rounded().x() - widget_width / 2;
      break;
    default:
      NOTREACHED() << "Undefined bound type.";
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

std::unique_ptr<views::Widget> CreateHandleWidget(gfx::NativeView parent) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.parent = parent;

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->GetNativeWindow()->SetEventTargeter(
      std::make_unique<aura::WindowTargeter>());
  if (::features::IsTouchTextEditingRedesignEnabled()) {
    // Disable visibility change animations so that the handle's opacity is not
    // overridden by fade effects.
    widget->SetVisibilityChangedAnimationsEnabled(false);
    widget->SetOpacity(kSelectionHandleOpacity);
  }

  return widget;
}

}  // namespace

namespace views {

using EditingHandleView = TouchSelectionControllerImpl::EditingHandleView;

// A View that displays the text selection handle.
class TouchSelectionControllerImpl::EditingHandleView : public View {
  METADATA_HEADER(EditingHandleView, View)

 public:
  EditingHandleView(TouchSelectionControllerImpl* controller,
                    bool is_cursor_handle)
      : controller_(controller),
        handle_image_(ui::ImageModel::FromImage(*GetCenterHandleImage())),
        is_cursor_handle_(is_cursor_handle) {}

  EditingHandleView(const EditingHandleView&) = delete;
  EditingHandleView& operator=(const EditingHandleView&) = delete;
  ~EditingHandleView() override = default;

  gfx::SelectionBound::Type GetSelectionBoundType() const {
    return selection_bound_.type();
  }

  // View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->DrawImageInt(
        handle_image_.Rasterize(GetColorProvider()),
        GetSelectionHandleHorizontalPadding(),
        selection_bound_.GetHeight() + GetSelectionHandleVerticalOffset());
  }

  void OnThemeChanged() override {
    View::OnThemeChanged();
    if (handle_image_.IsVectorIcon()) {
      SchedulePaint();
    }
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    event->SetHandled();
    switch (event->type()) {
      case ui::EventType::kGestureTap:
        if (is_cursor_handle_) {
          controller_->ToggleQuickMenu();
        }
        break;
      case ui::EventType::kGestureScrollBegin: {
        // Can only drag one handle at a time.
        DCHECK(!controller_->GetDraggingHandle());
        is_dragging_ = true;
        GetWidget()->SetCapture(this);
        controller_->OnDragBegin(this);
        // Distance from the point which is |kSelectionHandleVerticalDragOffset|
        // pixels above the bottom of the selection bound edge to the event
        // location (aka the touch-drag point).
        drag_offset_ = selection_bound_.edge_end_rounded() -
                       gfx::Vector2d(0, kSelectionHandleVerticalDragOffset) -
                       event->location();
        break;
      }
      case ui::EventType::kGestureScrollUpdate: {
        DCHECK(is_dragging_);
        controller_->OnDragUpdate(this, event->location() + drag_offset_);
        break;
      }
      case ui::EventType::kGestureScrollEnd:
      case ui::EventType::kScrollFlingStart: {
        is_dragging_ = false;
        GetWidget()->ReleaseCapture();
        controller_->OnDragEnd();
        ui::RecordTouchSelectionDrag(
            is_cursor_handle_
                ? ui::TouchSelectionDragType::kCursorHandleDrag
                : ui::TouchSelectionDragType::kSelectionHandleDrag);
        break;
      }
      default:
        break;
    }
  }

  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override {
    // This function will be called during widget initialization, i.e. before
    // SetBoundInScreen has been called. No-op in that case.
    if (!selection_bound_.HasHandle()) {
      return gfx::Size();
    }
    return GetSelectionWidgetBounds(selection_bound_).size();
  }

  bool GetWidgetVisible() const { return GetWidget()->IsVisible(); }

  void SetWidgetVisible(bool visible) {
    Widget* widget = GetWidget();
    if (widget->IsVisible() == visible) {
      return;
    }
    if (visible) {
      widget->Show();
    } else {
      widget->Hide();
    }
  }

  // If |is_visible| is true, this will update the widget and trigger a repaint
  // if necessary. Otherwise this will only update the internal state:
  // |selection_bound_| and |handle_image_|, so that the state is valid for the
  // time this becomes visible.
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
      handle_image_ = GetHandleImageModel(bound.type());
      if (is_visible)
        SchedulePaint();
    }

    if (is_visible) {
      selection_bound_.SetEdge(bound.edge_start(), bound.edge_end());

      GetWidget()->SetBounds(GetSelectionWidgetBounds(selection_bound_));

      aura::Window* window = GetWidget()->GetNativeView();
      gfx::Point edge_start = selection_bound_.edge_start_rounded();
      gfx::Point edge_end = selection_bound_.edge_end_rounded();
      wm::ConvertPointFromScreen(window, &edge_start);
      wm::ConvertPointFromScreen(window, &edge_end);
      selection_bound_.SetEdge(gfx::PointF(edge_start), gfx::PointF(edge_end));
    }

    const auto insets = gfx::Insets::TLBR(
        selection_bound_.GetHeight() + GetSelectionHandleVerticalOffset(), 0, 0,
        0);

    // Shifts the hit-test target below the apparent bounds to make dragging
    // easier.
    GetWidget()->GetNativeWindow()->targeter()->SetInsets(insets, insets);
  }

  bool GetIsDragging() const { return is_dragging_; }

  gfx::Size GetHandleImageSize() const { return handle_image_.Size(); }

 private:
  raw_ptr<TouchSelectionControllerImpl> controller_;

  // In local coordinates
  gfx::SelectionBound selection_bound_;
  ui::ImageModel handle_image_;

  // If true, this is a handle corresponding to the single cursor, otherwise it
  // is a handle corresponding to one of the two selection bounds.
  bool is_cursor_handle_;

  // Offset applied to the scroll events location when calling
  // TouchSelectionControllerImpl::OnDragUpdate while dragging the handle.
  gfx::Vector2d drag_offset_;

  // Whether the handle is currently being dragged.
  bool is_dragging_ = false;
};

BEGIN_METADATA(TouchSelectionControllerImpl, EditingHandleView)
ADD_READONLY_PROPERTY_METADATA(gfx::SelectionBound::Type, SelectionBoundType)
ADD_READONLY_PROPERTY_METADATA(bool, IsDragging)
ADD_READONLY_PROPERTY_METADATA(gfx::Size, HandleImageSize)
END_METADATA

TouchSelectionControllerImpl::TouchSelectionControllerImpl(
    ui::TouchEditable* client_view)
    : client_view_(client_view) {
  DCHECK(client_view_);
  CreateHandleWidgets();
  aura::Window* client_window = client_view_->GetNativeView();
  client_widget_ = Widget::GetTopLevelWidgetForNativeView(client_window);
  // Observe client widget moves and resizes to update the selection handles.
  if (client_widget_)
    client_widget_->AddObserver(this);

  // Observe certain event types sent to any event target, to hide this ui.
  aura::Env* env = aura::Env::GetInstance();
  std::set<ui::EventType> types = {
      ui::EventType::kMousePressed, ui::EventType::kMouseMoved,
      ui::EventType::kKeyPressed, ui::EventType::kMousewheel};
  env->AddEventObserver(this, env, types);

  toggle_menu_enabled_ = ::features::IsTouchTextEditingRedesignEnabled();
}

TouchSelectionControllerImpl::~TouchSelectionControllerImpl() {
  HideQuickMenu();
  HideMagnifier();
  aura::Env::GetInstance()->RemoveEventObserver(this);
  if (client_widget_) {
    client_widget_->RemoveObserver(this);
  }
  // Close the handle widgets to clean up the EditingHandleViews. We do this
  // here to ensure that the EditingHandleViews aren't left with a pointer to a
  // deleted TouchSelectionControllerImpl.
  selection_handle_1_widget_->CloseNow();
  selection_handle_2_widget_->CloseNow();
  cursor_handle_widget_->CloseNow();

  CHECK(!IsInObserverList());
}

void TouchSelectionControllerImpl::SelectionChanged() {
  EditingHandleView* selection_handle_1 = GetSelectionHandle1();
  EditingHandleView* selection_handle_2 = GetSelectionHandle2();
  EditingHandleView* cursor_handle = GetCursorHandle();
  if (!selection_handle_1 || !selection_handle_2 || !cursor_handle) {
    return;
  }

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
  const bool is_client_selection_dragging = client_view_->IsSelectionDragging();
  if (is_client_selection_dragging == is_client_selection_dragging_ &&
      screen_bound_anchor_clipped == selection_bound_1_clipped_ &&
      screen_bound_focus_clipped == selection_bound_2_clipped_) {
    return;
  }

  is_client_selection_dragging_ = is_client_selection_dragging;
  selection_bound_1_ = screen_bound_anchor;
  selection_bound_2_ = screen_bound_focus;
  selection_bound_1_clipped_ = screen_bound_anchor_clipped;
  selection_bound_2_clipped_ = screen_bound_focus_clipped;

  if (is_client_selection_dragging) {
    selection_handle_1->SetWidgetVisible(false);
    selection_handle_2->SetWidgetVisible(false);
    cursor_handle->SetWidgetVisible(false);
    UpdateQuickMenu();
    ShowMagnifier(screen_bound_focus);
  } else if (EditingHandleView* dragging_handle = GetDraggingHandle()) {
    // We need to reposition only the selection handle that is being dragged.
    // The other handle stays the same. Also, the selection handle being dragged
    // will always be at the end of selection, while the other handle will be at
    // the start.
    // If the new location of this handle is out of client view, its widget
    // should not get hidden, since it should still receive touch events.
    // Hence, we are not using |SetHandleBound()| method here.
    dragging_handle->SetBoundInScreen(screen_bound_focus_clipped, true);
    ShowMagnifier(screen_bound_focus);

    if (dragging_handle != cursor_handle) {
      // The non-dragging-handle might have recently become visible.
      EditingHandleView* non_dragging_handle = selection_handle_1;
      if (dragging_handle == selection_handle_1) {
        non_dragging_handle = selection_handle_2;
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
    if (screen_bound_anchor.edge_start() == screen_bound_focus.edge_start() &&
        screen_bound_anchor.edge_end() == screen_bound_focus.edge_end()) {
      // Empty selection, show cursor handle.
      selection_handle_1->SetWidgetVisible(false);
      selection_handle_2->SetWidgetVisible(false);
      SetHandleBound(cursor_handle, anchor, screen_bound_anchor_clipped);
      quick_menu_requested_ = !toggle_menu_enabled_;
    } else {
      // Non-empty selection, show selection handles.
      cursor_handle->SetWidgetVisible(false);
      SetHandleBound(selection_handle_1, anchor, screen_bound_anchor_clipped);
      SetHandleBound(selection_handle_2, focus, screen_bound_focus_clipped);
      quick_menu_requested_ = true;
    }
    UpdateQuickMenu();
    HideMagnifier();
  }
}

void TouchSelectionControllerImpl::ToggleQuickMenu() {
  if (toggle_menu_enabled_) {
    quick_menu_requested_ = !quick_menu_requested_;
    UpdateQuickMenu();
  }
}

void TouchSelectionControllerImpl::ShowQuickMenuImmediatelyForTesting() {
  if (quick_menu_timer_.IsRunning()) {
    quick_menu_timer_.Stop();
    QuickMenuTimerFired();
  }
}

void TouchSelectionControllerImpl::OnDragBegin(EditingHandleView* handle) {
  DCHECK_EQ(handle, GetDraggingHandle());
  UpdateQuickMenu();
  if (handle == GetCursorHandle()) {
    return;
  }

  DCHECK(handle == GetSelectionHandle1() || handle == GetSelectionHandle2());

  // Find selection end points in client_view's coordinate system.
  gfx::Point base = selection_bound_1_.edge_start_rounded();
  base.Offset(0, selection_bound_1_.GetHeight() / 2);
  client_view_->ConvertPointFromScreen(&base);

  gfx::Point extent = selection_bound_2_.edge_start_rounded();
  extent.Offset(0, selection_bound_2_.GetHeight() / 2);
  client_view_->ConvertPointFromScreen(&extent);

  if (handle == GetSelectionHandle1()) {
    std::swap(base, extent);
  }

  // When moving the handle we want to move only the extent point. Before
  // doing so we must make sure that the base point is set correctly.
  client_view_->SelectBetweenCoordinates(base, extent);
}

void TouchSelectionControllerImpl::OnDragUpdate(EditingHandleView* handle,
                                                const gfx::Point& drag_pos) {
  DCHECK_EQ(handle, GetDraggingHandle());
  gfx::Point drag_pos_in_client = drag_pos;
  ConvertPointToClientView(handle, &drag_pos_in_client);

  if (handle == GetCursorHandle()) {
    client_view_->MoveCaret(drag_pos_in_client);
    return;
  }

  DCHECK(handle == GetSelectionHandle1() || handle == GetSelectionHandle2());
  client_view_->MoveRangeSelectionExtent(drag_pos_in_client);
}

void TouchSelectionControllerImpl::OnDragEnd() {
  DCHECK(!GetDraggingHandle());
  UpdateQuickMenu();
  HideMagnifier();
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
  client_view_ = nullptr;
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
  auto* menu_runner = ui::TouchSelectionMenuRunner::GetInstance();
  if (!menu_runner) {
    return;
  }

  gfx::Rect menu_anchor = GetQuickMenuAnchorRect();
  if (menu_anchor == gfx::Rect())
    return;

  gfx::Size handle_image_size;
  if (::features::IsTouchTextEditingRedesignEnabled()) {
    if (selection_handle_1_widget_->IsClosed() ||
        selection_handle_2_widget_->IsClosed() ||
        cursor_handle_widget_->IsClosed()) {
      return;
    }
    handle_image_size = cursor_handle_widget_->IsVisible()
                            ? GetCursorHandle()->GetHandleImageSize()
                            : GetSelectionHandle1()->GetHandleImageSize();
  } else {
    handle_image_size = GetMaxHandleImageSize();
  }

  menu_runner->OpenMenu(GetWeakPtr(), menu_anchor, handle_image_size,
                        client_view_->GetNativeView());
}

void TouchSelectionControllerImpl::StartQuickMenuTimer() {
  if (quick_menu_timer_.IsRunning())
    return;
  quick_menu_timer_.Start(FROM_HERE, base::Milliseconds(kQuickMenuDelayInMs),
                          this,
                          &TouchSelectionControllerImpl::QuickMenuTimerFired);
}

void TouchSelectionControllerImpl::UpdateQuickMenu() {
  HideQuickMenu();
  if (quick_menu_requested_ && GetDraggingHandle() == nullptr &&
      !client_view_->IsSelectionDragging()) {
    StartQuickMenuTimer();
  }
}

void TouchSelectionControllerImpl::HideQuickMenu() {
  auto* menu_runner = ui::TouchSelectionMenuRunner::GetInstance();
  if (menu_runner && menu_runner->IsRunning()) {
    menu_runner->CloseMenu();
  }
  quick_menu_timer_.Stop();
}

gfx::Rect TouchSelectionControllerImpl::GetQuickMenuAnchorRect() const {
  // Get selection end points in client_view's space.
  gfx::SelectionBound b1_in_screen = selection_bound_1_clipped_;
  gfx::SelectionBound b2_in_screen =
      cursor_handle_widget_ && cursor_handle_widget_->IsVisible()
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
  menu_anchor.Outset(gfx::Outsets::VH(GetSelectionHandleVerticalOffset(), 0));

  return menu_anchor;
}

void TouchSelectionControllerImpl::ShowMagnifier(
    const gfx::SelectionBound& focus_bound_in_screen) {
  if (!::features::IsTouchTextEditingRedesignEnabled()) {
    return;
  }

  aura::Window* root_window = client_view_->GetNativeView()->GetRootWindow();
  DCHECK(root_window);
  if (!touch_selection_magnifier_) {
    touch_selection_magnifier_ =
        std::make_unique<ui::TouchSelectionMagnifierAura>();
  }

  // Convert focus bound to root window coordinates.
  gfx::Point focus_start = focus_bound_in_screen.edge_start_rounded();
  gfx::Point focus_end = focus_bound_in_screen.edge_end_rounded();
  wm::ConvertPointFromScreen(root_window, &focus_start);
  wm::ConvertPointFromScreen(root_window, &focus_end);
  touch_selection_magnifier_->ShowFocusBound(root_window->layer(), focus_start,
                                             focus_end);
}

void TouchSelectionControllerImpl::HideMagnifier() {
  touch_selection_magnifier_ = nullptr;
}

void TouchSelectionControllerImpl::CreateHandleWidgets() {
  DCHECK(client_view_);

  selection_handle_1_widget_ =
      CreateHandleWidget(client_view_->GetNativeView());
  selection_handle_1_widget_->SetContentsView(
      std::make_unique<EditingHandleView>(this, false));

  selection_handle_2_widget_ =
      CreateHandleWidget(client_view_->GetNativeView());
  selection_handle_2_widget_->SetContentsView(
      std::make_unique<EditingHandleView>(this, false));

  cursor_handle_widget_ = CreateHandleWidget(client_view_->GetNativeView());
  cursor_handle_widget_->SetContentsView(
      std::make_unique<EditingHandleView>(this, true));
}

EditingHandleView* TouchSelectionControllerImpl::GetSelectionHandle1() {
  return selection_handle_1_widget_->IsClosed()
             ? nullptr
             : AsViewClass<EditingHandleView>(
                   selection_handle_1_widget_->GetContentsView());
}

EditingHandleView* TouchSelectionControllerImpl::GetSelectionHandle2() {
  return selection_handle_1_widget_->IsClosed()
             ? nullptr
             : AsViewClass<EditingHandleView>(
                   selection_handle_2_widget_->GetContentsView());
}

EditingHandleView* TouchSelectionControllerImpl::GetCursorHandle() {
  return selection_handle_1_widget_->IsClosed()
             ? nullptr
             : AsViewClass<EditingHandleView>(
                   cursor_handle_widget_->GetContentsView());
}

EditingHandleView* TouchSelectionControllerImpl::GetDraggingHandle() {
  EditingHandleView* selection_handle_1 = GetSelectionHandle1();
  EditingHandleView* selection_handle_2 = GetSelectionHandle2();
  EditingHandleView* cursor_handle = GetCursorHandle();

  if (selection_handle_1->GetIsDragging()) {
    return selection_handle_1;
  } else if (selection_handle_2->GetIsDragging()) {
    return selection_handle_2;
  } else if (cursor_handle->GetIsDragging()) {
    return cursor_handle;
  }
  return nullptr;
}

gfx::NativeView TouchSelectionControllerImpl::GetCursorHandleNativeView() {
  return cursor_handle_widget_->GetNativeView();
}

gfx::SelectionBound::Type
TouchSelectionControllerImpl::GetSelectionHandle1Type() {
  return GetSelectionHandle1()->GetSelectionBoundType();
}

gfx::Rect TouchSelectionControllerImpl::GetSelectionHandle1Bounds() {
  return GetSelectionHandle1()->GetBoundsInScreen();
}

gfx::Rect TouchSelectionControllerImpl::GetSelectionHandle2Bounds() {
  return GetSelectionHandle2()->GetBoundsInScreen();
}

gfx::Rect TouchSelectionControllerImpl::GetCursorHandleBounds() {
  return GetCursorHandle()->GetBoundsInScreen();
}

bool TouchSelectionControllerImpl::IsSelectionHandle1Visible() {
  return GetSelectionHandle1()->GetWidgetVisible();
}

bool TouchSelectionControllerImpl::IsSelectionHandle2Visible() {
  return GetSelectionHandle2()->GetWidgetVisible();
}

bool TouchSelectionControllerImpl::IsCursorHandleVisible() {
  return GetCursorHandle()->GetWidgetVisible();
}

gfx::Rect TouchSelectionControllerImpl::GetExpectedHandleBounds(
    const gfx::SelectionBound& bound) {
  return GetSelectionWidgetBounds(bound);
}

View* TouchSelectionControllerImpl::GetHandle1View() {
  return selection_handle_1_widget_->GetContentsView();
}

View* TouchSelectionControllerImpl::GetHandle2View() {
  return selection_handle_2_widget_->GetContentsView();
}

}  // namespace views

DEFINE_ENUM_CONVERTERS(gfx::SelectionBound::Type,
                       {gfx::SelectionBound::Type::LEFT, u"LEFT"},
                       {gfx::SelectionBound::Type::RIGHT, u"RIGHT"},
                       {gfx::SelectionBound::Type::CENTER, u"CENTER"},
                       {gfx::SelectionBound::Type::EMPTY, u"EMPTY"})
