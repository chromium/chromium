// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_PAINT_CONTEXT_H_
#define UI_COMPOSITOR_PAINT_CONTEXT_H_

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class DisplayItemList;
}

namespace ui {
class ClipRecorder;
class PaintRecorder;
class TransformRecorder;

class COMPOSITOR_EXPORT PaintContext {
 public:
  // Construct a PaintContext that may only re-paint the area in the
  // |invalidation|.
  PaintContext(cc::DisplayItemList* list,
               float device_scale_factor,
               const gfx::Rect& invalidation,
               bool is_pixel_canvas);

  // Clone a PaintContext with an additional |offset|.
  PaintContext(const PaintContext& other, const gfx::Vector2d& offset);

  // Clone a PaintContext that has no consideration for invalidation.
  enum CloneWithoutInvalidation {
    CLONE_WITHOUT_INVALIDATION,
  };
  PaintContext(const PaintContext& other, CloneWithoutInvalidation c);

  PaintContext(const PaintContext&) = delete;
  PaintContext& operator=(const PaintContext&) = delete;

  ~PaintContext();

  // When true, IsRectInvalid() can be called, otherwise its result would be
  // invalid.
  bool CanCheckInvalid() const { return !invalidation_.IsEmpty(); }

  // The device scale of the frame being painted.
  float device_scale_factor() const { return device_scale_factor_; }

  // Returns true if the paint commands are recorded at pixel size instead of
  // DIP.
  bool is_pixel_canvas() const { return is_pixel_canvas_; }

  // When true, the |bounds| touches an invalidated area, so should be
  // re-painted. When false, re-painting can be skipped. Bounds should be in
  // the local space with offsets up to the painting root in the PaintContext.
  bool IsRectInvalid(const gfx::Rect& bounds) const {
    DCHECK(CanCheckInvalid());
    return invalidation_.Intersects(bounds + offset_);
  }

#if DCHECK_IS_ON()
  void Visited(void* visited) const {
    if (!root_visited_)
      root_visited_ = visited;
  }
  void* RootVisited() const { return root_visited_; }
  const gfx::Vector2d& PaintOffset() const { return offset_; }
#endif

  const gfx::Rect& InvalidationForTesting() const { return invalidation_; }

 private:
  // The Recorder classes need access to the internal canvas and friends, but we
  // don't want to expose them on this class so that people must go through the
  // recorders to access them.
  friend class ClipRecorder;
  friend class PaintRecorder;
  friend class TransformRecorder;
  // The Cache class also needs to access the DisplayItemList to append its
  // cache contents.
  friend class PaintCache;

  // Returns a rect with the given size in the space of the context's
  // containing layer.
  gfx::Rect ToLayerSpaceBounds(const gfx::Size& size_in_context) const;

  // Returns the given rect translated by the layer space offset.
  gfx::Rect ToLayerSpaceRect(const gfx::Rect& rect) const;

  raw_ptr<cc::DisplayItemList> list_;
  // The device scale of the frame being painted. Used to determine which bitmap
  // resources to use in the frame.
  float device_scale_factor_;
  // Invalidation in the space of the paint root (ie the space of the layer
  // backing the paint taking place).
  gfx::Rect invalidation_;
  // Offset from the PaintContext to the space of the paint root and the
  // |invalidation_|.
  gfx::Vector2d offset_;
  // If enabled, the paint commands are recorded at pixel size.
  const bool is_pixel_canvas_;

#if DCHECK_IS_ON()
  // Used to verify that the |invalidation_| is only used to compare against
  // rects in the same space.
  mutable raw_ptr<void> root_visited_;
  // Used to verify that paint recorders are not nested. True while a paint
  // recorder is active.
  mutable bool inside_paint_recorder_;
#endif
};

}  // namespace ui

#endif  // UI_COMPOSITOR_PAINT_CONTEXT_H_
