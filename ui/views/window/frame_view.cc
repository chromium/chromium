// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/frame_view.h"

#include <memory>

#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom-data-view.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#endif

namespace views {

FrameView::~FrameView() = default;

bool FrameView::ShouldPaintAsActive() const {
  return GetWidget()->ShouldPaintAsActive();
}

int FrameView::GetHTComponentForFrame(const gfx::Point& point,
                                      const gfx::Insets& resize_border,
                                      int top_resize_corner_height,
                                      int resize_corner_width,
                                      bool can_resize) {
  // If the point isnt within the resize boundaries, return nowhere.
  bool point_in_top = point.y() < resize_border.top();
  bool point_in_bottom = point.y() >= height() - resize_border.bottom();
  bool point_in_left = point.x() < resize_border.left();
  bool point_in_right = point.x() >= width() - resize_border.right();

  if (!point_in_left && !point_in_right && !point_in_top && !point_in_bottom) {
    return HTNOWHERE;
  }

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

gfx::Rect FrameView::GetBoundsForClientView() const {
  return gfx::Rect();
}

gfx::Rect FrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

bool FrameView::GetClientMask(const gfx::Size& size, SkPath* mask) const {
  return false;
}

bool FrameView::HasWindowTitle() const {
  return false;
}

bool FrameView::IsWindowTitleVisible() const {
  return false;
}

#if BUILDFLAG(IS_WIN)
gfx::Point FrameView::GetSystemMenuScreenPixelLocation() const {
  gfx::Point point(GetMirroredXInView(GetBoundsForClientView().x()),
                   GetSystemMenuY());
  View::ConvertPointToScreen(this, &point);
  point = display::win::GetScreenWin()->DIPToScreenPoint(point);
  // The native system menu seems to overlap the titlebar by 1 px.  Match that.
  return point - gfx::Vector2d(0, 1);
}
#endif

int FrameView::NonClientHitTest(const gfx::Point& point) {
  return HTNOWHERE;
}

void FrameView::OnThemeChanged() {
  View::OnThemeChanged();
  SchedulePaint();
}

void FrameView::Layout(PassKey) {
  if (GetLayoutManager()) {
    GetLayoutManager()->Layout(this);
  }

  views::ClientView* client_view = GetWidget()->client_view();
  client_view->SetBoundsRect(GetBoundsForClientView());
  SkPath client_clip;
  if (GetClientMask(client_view->size(), &client_clip)) {
    client_view->SetClipPath(client_clip);
  }
}

View::Views FrameView::GetChildrenInZOrder() {
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

void FrameView::InsertClientView(ClientView* client_view) {
  AddChildViewRaw(client_view);
}

FrameView::FrameView() {
  GetViewAccessibility().SetRole(ax::mojom::Role::kClient);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

#if BUILDFLAG(IS_WIN)
int FrameView::GetSystemMenuY() const {
  return GetBoundsForClientView().y();
}
#endif

BEGIN_METADATA(FrameView)
END_METADATA

}  // namespace views
