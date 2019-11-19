// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/selection_bound.h"

namespace gfx {

SelectionBound::SelectionBound() : type_(EMPTY), visible_(false) {}

SelectionBound::SelectionBound(const SelectionBound& other) = default;

SelectionBound::~SelectionBound() {}

void SelectionBound::SetEdgeStart(const gfx::PointF& value) {
  edge_start_ = value;
  edge_start_rounded_ = gfx::ToRoundedPoint(value);
}

void SelectionBound::SetVisibleEdgeStart(const gfx::PointF& value) {
  visible_edge_start_ = value;
}

void SelectionBound::SetEdgeEnd(const gfx::PointF& value) {
  edge_end_ = value;
  edge_end_rounded_ = gfx::ToRoundedPoint(value);
}

void SelectionBound::SetVisibleEdgeEnd(const gfx::PointF& value) {
  visible_edge_end_ = value;
}

void SelectionBound::SetEdge(const gfx::PointF& start, const gfx::PointF& end) {
  SetEdgeStart(start);
  SetEdgeEnd(end);
}

void SelectionBound::SetVisibleEdge(const gfx::PointF& start,
                                    const gfx::PointF& end) {
  SetVisibleEdgeStart(start);
  SetVisibleEdgeEnd(end);
}

int SelectionBound::GetHeight() const {
  return edge_end_rounded_.y() - edge_start_rounded_.y();
}

std::string SelectionBound::ToString() const {
  return base::StringPrintf(
      "SelectionBound(%s, %s, %s, %s, %d)", edge_start_.ToString().c_str(),
      edge_end_.ToString().c_str(), edge_start_rounded_.ToString().c_str(),
      edge_end_rounded_.ToString().c_str(), visible_);
}

bool operator==(const SelectionBound& lhs, const SelectionBound& rhs) {
  return lhs.type() == rhs.type() && lhs.visible() == rhs.visible() &&
         lhs.edge_start() == rhs.edge_start() &&
         lhs.edge_end() == rhs.edge_end() &&
         lhs.visible_edge_start() == rhs.visible_edge_start() &&
         lhs.visible_edge_end() == rhs.visible_edge_end();
}

bool operator!=(const SelectionBound& lhs, const SelectionBound& rhs) {
  return !(lhs == rhs);
}

gfx::Rect RectBetweenSelectionBounds(const SelectionBound& b1,
                                     const SelectionBound& b2) {
  gfx::Point top_left(b1.edge_start_rounded());
  top_left.SetToMin(b1.edge_end_rounded());
  top_left.SetToMin(b2.edge_start_rounded());
  top_left.SetToMin(b2.edge_end_rounded());

  gfx::Point bottom_right(b1.edge_start_rounded());
  bottom_right.SetToMax(b1.edge_end_rounded());
  bottom_right.SetToMax(b2.edge_start_rounded());
  bottom_right.SetToMax(b2.edge_end_rounded());

  gfx::Vector2d diff = bottom_right - top_left;
  return gfx::Rect(top_left, gfx::Size(diff.x(), diff.y()));
}

gfx::RectF RectFBetweenSelectionBounds(const SelectionBound& b1,
                                       const SelectionBound& b2) {
  gfx::PointF top_left(b1.edge_start());
  top_left.SetToMin(b1.edge_end());
  top_left.SetToMin(b2.edge_start());
  top_left.SetToMin(b2.edge_end());

  gfx::PointF bottom_right(b1.edge_start());
  bottom_right.SetToMax(b1.edge_end());
  bottom_right.SetToMax(b2.edge_start());
  bottom_right.SetToMax(b2.edge_end());

  gfx::Vector2dF diff = bottom_right - top_left;
  return gfx::RectF(top_left, gfx::SizeF(diff.x(), diff.y()));
}

gfx::RectF RectFBetweenVisibleSelectionBounds(const SelectionBound& b1,
                                              const SelectionBound& b2) {
  // The selection bound is determined by the |start| and |end| points. For the
  // writing-mode is vertical-*, the bounds are horizontal, the |end| might
  // be on the left side of the |start|.
  gfx::PointF top_left(b1.visible_edge_start());
  top_left.SetToMin(b1.visible_edge_end());
  top_left.SetToMin(b2.visible_edge_start());
  top_left.SetToMin(b2.visible_edge_end());

  gfx::PointF bottom_right(b1.visible_edge_start());
  bottom_right.SetToMax(b1.visible_edge_end());
  bottom_right.SetToMax(b2.visible_edge_start());
  bottom_right.SetToMax(b2.visible_edge_end());

  gfx::Vector2dF diff = bottom_right - top_left;
  return gfx::RectF(top_left, gfx::SizeF(diff.x(), diff.y()));
}

}  // namespace gfx
