// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SELECTION_BOUND_H_
#define UI_GFX_SELECTION_BOUND_H_

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class Rect;
class RectF;

// Bound of a selection end-point.
class GFX_EXPORT SelectionBound {
 public:
  enum Type { LEFT, RIGHT, CENTER, EMPTY, LAST = EMPTY };

  SelectionBound();
  SelectionBound(const SelectionBound& other);
  ~SelectionBound();

  Type type() const { return type_; }
  void set_type(Type value) { type_ = value; }

  const gfx::PointF& edge_start() const { return edge_start_; }
  const gfx::PointF& visible_edge_start() const { return visible_edge_start_; }
  const gfx::Point& edge_start_rounded() const { return edge_start_rounded_; }
  void SetEdgeStart(const gfx::PointF& value);
  void SetVisibleEdgeStart(const gfx::PointF& value);

  const gfx::PointF& edge_end() const { return edge_end_; }
  const gfx::PointF& visible_edge_end() const { return visible_edge_end_; }
  const gfx::Point& edge_end_rounded() const { return edge_end_rounded_; }
  void SetEdgeEnd(const gfx::PointF& value);
  void SetVisibleEdgeEnd(const gfx::PointF& value);

  void SetEdge(const gfx::PointF& start, const gfx::PointF& end);
  void SetVisibleEdge(const gfx::PointF& start, const gfx::PointF& end);

  bool visible() const { return visible_; }
  void set_visible(bool value) { visible_ = value; }

  // Returns the vertical difference between rounded start and end.
  int GetHeight() const;

  std::string ToString() const;

 private:
  Type type_;
  // The actual bounds of a selection end-point mgiht be invisible for
  // occlusion.
  gfx::PointF edge_start_;
  gfx::PointF edge_end_;
  // The visible bounds of a selection, which are equal to the above, when there
  // is no occlusion.
  gfx::PointF visible_edge_start_;
  gfx::PointF visible_edge_end_;
  gfx::Point edge_start_rounded_;
  gfx::Point edge_end_rounded_;
  bool visible_;
};

GFX_EXPORT bool operator==(const SelectionBound& lhs,
                           const SelectionBound& rhs);
GFX_EXPORT bool operator!=(const SelectionBound& lhs,
                           const SelectionBound& rhs);

GFX_EXPORT gfx::Rect RectBetweenSelectionBounds(const SelectionBound& b1,
                                                const SelectionBound& b2);

GFX_EXPORT gfx::RectF RectFBetweenSelectionBounds(const SelectionBound& b1,
                                                  const SelectionBound& b2);

GFX_EXPORT gfx::RectF RectFBetweenVisibleSelectionBounds(
    const SelectionBound& b1,
    const SelectionBound& b2);
}  // namespace ui

#endif  // UI_GFX_SELECTION_BOUND_H_
