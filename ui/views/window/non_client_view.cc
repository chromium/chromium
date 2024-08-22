// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/non_client_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#endif

namespace views {

NonClientFrameView::~NonClientFrameView() = default;

bool NonClientFrameView::ShouldPaintAsActive() const {
  return GetWidget()->ShouldPaintAsActive();
}

int NonClientFrameView::GetHTComponentForFrame(const gfx::Point& point,
                                               const gfx::Insets& resize_border,
                                               int top_resize_corner_height,
                                               int resize_corner_width,
                                               bool can_resize) {
  // If the point isnt within the resize boundaries, return nowhere.
  bool point_in_top = point.y() < resize_border.top();
  bool point_in_bottom = point.y() >= height() - resize_border.bottom();
  bool point_in_left = point.x() < resize_border.left();
  bool point_in_right = point.x() >= width() - resize_border.right();

  if (!point_in_left && !point_in_right && !point_in_top && !point_in_bottom)
    return HTNOWHERE;

  // If the window can't be resized, there are no resize boundaries, just
  // window borders.
  if (!can_resize) {
    return HTBORDER;
  }

  // Shrink the resize boundaries
  point_in_top |= point.y() < top_resize_corner_height;
  point_in_left |= point.x() < resize_corner_width;
  point_in_right |= point.x() >= width() - resize_corner_width;

  if (point_in_top) {
    if (point_in_left) {
      return HTTOPLEFT;
    } else if (point_in_right) {
      return HTTOPRIGHT;
    }
    return HTTOP;
  } else if (point_in_bottom) {
    if (point_in_left) {
      return HTBOTTOMLEFT;
    } else if (point_in_right) {
      return HTBOTTOMRIGHT;
    }
    return HTBOTTOM;
  } else if (point_in_left) {
    return HTLEFT;
  }
  CHECK(point_in_right);
  return HTRIGHT;
}

gfx::Rect NonClientFrameView::GetBoundsForClientView() const {
  return gfx::Rect();
}

gfx::Rect NonClientFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

bool NonClientFrameView::GetClientMask(const gfx::Size& size,
                                       SkPath* mask) const {
  return false;
}

bool NonClientFrameView::HasWindowTitle() const {
  return false;
}

bool NonClientFrameView::IsWindowTitleVisible() const {
  return false;
}

#if BUILDFLAG(IS_WIN)
gfx::Point NonClientFrameView::GetSystemMenuScreenPixelLocation() const {
  gfx::Point point(GetMirroredXInView(GetBoundsForClientView().x()),
                   GetSystemMenuY());
  View::ConvertPointToScreen(this, &point);
  point = display::win::ScreenWin::DIPToScreenPoint(point);
  // The native system menu seems to overlap the titlebar by 1 px.  Match that.
  return point - gfx::Vector2d(0, 1);
}
#endif

int NonClientFrameView::NonClientHitTest(const gfx::Point& point) {
  return HTNOWHERE;
}

void NonClientFrameView::OnThemeChanged() {
  View::OnThemeChanged();
  SchedulePaint();
}

void NonClientFrameView::Layout(PassKey) {
  if (GetLayoutManager())
    GetLayoutManager()->Layout(this);

  views::ClientView* client_view = GetWidget()->client_view();
  client_view->SetBoundsRect(GetBoundsForClientView());
  SkPath client_clip;
  if (GetClientMask(client_view->size(), &client_clip))
    client_view->SetClipPath(client_clip);
}

View::Views NonClientFrameView::GetChildrenInZOrder() {
  View::Views paint_order = View::GetChildrenInZOrder();
  views::ClientView* client_view =
      GetWidget() ? GetWidget()->client_view() : nullptr;

  // Move the client view to the beginning of the Z-order to ensure that the
  // other children of the frame view draw on top of it.
  if (client_view && std::erase(paint_order, client_view)) {
    paint_order.insert(paint_order.begin(), client_view);
  }

  return paint_order;
}

void NonClientFrameView::InsertClientView(ClientView* client_view) {
  AddChildView(client_view);
}

NonClientFrameView::NonClientFrameView() {
  GetViewAccessibility().SetRole(ax::mojom::Role::kClient);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

#if BUILDFLAG(IS_WIN)
int NonClientFrameView::GetSystemMenuY() const {
  return GetBoundsForClientView().y();
}
#endif

BEGIN_METADATA(NonClientFrameView)
END_METADATA

NonClientView::NonClientView(views::ClientView* client_view)
    : client_view_(client_view) {
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  // TODO(crbug.com/40866857): Should this be pruned from the accessibility
  // tree?
  GetViewAccessibility().SetRole(ax::mojom::Role::kClient);
}

NonClientView::~NonClientView() {
  // This value may have been reset before the window hierarchy shuts down,
  // so we need to manually remove it.
  RemoveChildView(frame_view_.get());
}

void NonClientView::SetFrameView(
    std::unique_ptr<NonClientFrameView> frame_view) {
  // If there is an existing frame view, ensure that the ClientView remains
  // attached to the Widget by moving the ClientView to the new frame before
  // removing the old frame from the view hierarchy.
  std::unique_ptr<NonClientFrameView> old_frame_view = std::move(frame_view_);
  frame_view_ = std::move(frame_view);
  if (parent()) {
    AddChildViewAt(frame_view_.get(), 0);
    frame_view_->InsertClientView(client_view_);
  }

  if (old_frame_view)
    RemoveChildView(old_frame_view.get());
}

void NonClientView::SetOverlayView(View* view) {
  if (overlay_view_)
    RemoveChildView(overlay_view_);

  if (!view)
    return;

  overlay_view_ = view;
  if (parent())
    AddChildView(overlay_view_.get());
}

CloseRequestResult NonClientView::OnWindowCloseRequested() {
  return client_view_->OnWindowCloseRequested();
}

void NonClientView::WindowClosing() {
  client_view_->WidgetClosing();
}

void NonClientView::UpdateFrame() {
  Widget* widget = GetWidget();
  SetFrameView(widget->CreateNonClientFrameView());
  widget->ThemeChanged();
  InvalidateLayout();
  SchedulePaint();
}

gfx::Rect NonClientView::GetWindowBoundsForClientBounds(
    const gfx::Rect client_bounds) const {
  return frame_view_->GetWindowBoundsForClientBounds(client_bounds);
}

int NonClientView::NonClientHitTest(const gfx::Point& point) {
  // The NonClientFrameView is responsible for also asking the ClientView.
  return frame_view_->NonClientHitTest(point);
}

void NonClientView::GetWindowMask(const gfx::Size& size, SkPath* window_mask) {
  frame_view_->GetWindowMask(size, window_mask);
}

void NonClientView::ResetWindowControls() {
  frame_view_->ResetWindowControls();
}

void NonClientView::UpdateWindowIcon() {
  frame_view_->UpdateWindowIcon();
}

void NonClientView::UpdateWindowTitle() {
  frame_view_->UpdateWindowTitle();
}

void NonClientView::SizeConstraintsChanged() {
  frame_view_->SizeConstraintsChanged();
}

bool NonClientView::HasWindowTitle() const {
  return frame_view_->HasWindowTitle();
}

bool NonClientView::IsWindowTitleVisible() const {
  return frame_view_->IsWindowTitleVisible();
}

gfx::Size NonClientView::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  // TODO(pkasting): This should probably be made to look similar to
  // GetMinimumSize() below.  This will require implementing GetPreferredSize()
  // better in the various frame views.
  gfx::Rect client_bounds(gfx::Point(),
                          client_view_->GetPreferredSize(available_size));
  return GetWindowBoundsForClientBounds(client_bounds).size();
}

gfx::Size NonClientView::GetMinimumSize() const {
  return frame_view_->GetMinimumSize();
}

gfx::Size NonClientView::GetMaximumSize() const {
  return frame_view_->GetMaximumSize();
}

void NonClientView::Layout(PassKey) {
  // TODO(pkasting): The frame view should have the client view as a child and
  // lay it out directly + set its clip path.  Done correctly, this should let
  // us use a FillLayout on this class that holds |frame_view_| and
  // |overlay_view_|, and eliminate CalculatePreferredSize()/GetMinimumSize()/
  // GetMaximumSize()/Layout().  The frame view and client view were originally
  // siblings because "many Views make the assumption they are only inserted
  // into a View hierarchy once" ( http://codereview.chromium.org/27317 ), but
  // where that is still the case it should simply be fixed.
  frame_view_->SetBoundsRect(GetLocalBounds());

  if (overlay_view_)
    overlay_view_->SetBoundsRect(GetLocalBounds());
}

View* NonClientView::GetTooltipHandlerForPoint(const gfx::Point& point) {
  // The same logic as for TargetForRect() applies here.
  if (frame_view_->parent() == this) {
    // During the reset of the frame_view_ it's possible to be in this code
    // after it's been removed from the view hierarchy but before it's been
    // removed from the NonClientView.
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToTarget(this, frame_view_.get(), &point_in_child_coords);
    View* handler =
        frame_view_->GetTooltipHandlerForPoint(point_in_child_coords);
    if (handler)
      return handler;
  }

  return View::GetTooltipHandlerForPoint(point);
}

void NonClientView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // Add our child views here as we are added to the Widget so that if we are
  // subsequently resized all the parent-child relationships are established.
  // TODO(pkasting): The above comment makes no sense to me.  Try to eliminate
  // the various setters, and create and add children directly in the
  // constructor.
  if (details.is_add && GetWidget() && details.child == this) {
    AddChildViewAt(frame_view_.get(), 0);
    frame_view_->InsertClientView(client_view_);
    if (overlay_view_)
      AddChildView(overlay_view_.get());
  }
}

View* NonClientView::TargetForRect(View* root, const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!UsePointBasedTargeting(rect))
    return ViewTargeterDelegate::TargetForRect(root, rect);

  // Because of the z-ordering of our child views (the client view is positioned
  // over the non-client frame view, if the client view ever overlaps the frame
  // view visually (as it does for the browser window), then it will eat
  // events for the window controls. We override this method here so that we can
  // detect this condition and re-route the events to the non-client frame view.
  // The assumption is that the frame view's implementation of HitTest will only
  // return true for area not occupied by the client view.
  if (frame_view_->parent() == this) {
    // During the reset of the frame_view_ it's possible to be in this code
    // after it's been removed from the view hierarchy but before it's been
    // removed from the NonClientView.
    gfx::RectF rect_in_child_coords_f(rect);
    View::ConvertRectToTarget(this, frame_view_.get(), &rect_in_child_coords_f);
    gfx::Rect rect_in_child_coords =
        gfx::ToEnclosingRect(rect_in_child_coords_f);
    if (frame_view_->HitTestRect(rect_in_child_coords))
      return frame_view_->GetEventHandlerForRect(rect_in_child_coords);
  }

  return ViewTargeterDelegate::TargetForRect(root, rect);
}

BEGIN_METADATA(NonClientView)
END_METADATA

}  // namespace views
