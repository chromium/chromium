// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_PAINT_INFO_H_
#define UI_VIEWS_PAINT_INFO_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/views_export.h"

namespace views {

// This class manages the context required during View::Paint(). It is
// responsible for setting the paint recording size and the paint recording
// scale factors for an individual View.
// Each PaintInfo instance has paint recording offset relative to a root
// PaintInfo.
// All coordinates are in paint recording space. If pixel canvas is enabled this
// essentially becomes pixel coordinate space.
class VIEWS_EXPORT PaintInfo {
 public:
  enum class ScaleType {
    // Scale the recordings by the default device scale factor while maintaining
    // the aspect ratio. Use this when a view contains an image or icon that
    // should not get distorted due to scaling.
    kUniformScaling = 0,

    // Scale the recordings based on the device scale factor but snap to the
    // parent's bottom or right edge whenever possible. This may lead to minor
    // distortion and is not recommended to be used with views that contain
    // images.
    kScaleWithEdgeSnapping
  };

  // Instantiates a root PaintInfo. This should only be initialized at the Paint
  // root, ie., a layer or the root of a widget.
  static PaintInfo CreateRootPaintInfo(const ui::PaintContext& root_context,
                                       const gfx::Size& size);

  // Instantiate a child PaintInfo instance. All bounds for this object are
  // relative to its root PaintInfo.
  static PaintInfo CreateChildPaintInfo(const PaintInfo& parent_paint_info,
                                        const gfx::Rect& bounds,
                                        const gfx::Size& parent_size,
                                        ScaleType scale_type,
                                        bool is_layer,
                                        bool needs_paint = false);

  PaintInfo(const PaintInfo& other);
  ~PaintInfo();

  // Returns true if all paint commands are recorded at pixel size.
  bool IsPixelCanvas() const;

  // Returns true if the View should be painted based on whether per-view
  // invalidation is enabled or not.
  bool ShouldPaint() const;

  const ui::PaintContext& context() const {
    return root_context_ ? *root_context_ : context_;
  }

  gfx::Vector2d offset_from_root() const {
    return paint_recording_bounds_.OffsetFromOrigin();
  }

  const gfx::Vector2d& offset_from_parent() const {
    return offset_from_parent_;
  }

  float paint_recording_scale_x() const { return paint_recording_scale_x_; }

  float paint_recording_scale_y() const { return paint_recording_scale_y_; }

  const gfx::Size& paint_recording_size() const {
    return paint_recording_bounds_.size();
  }

  const gfx::Rect& paint_recording_bounds() const {
    return paint_recording_bounds_;
  }

 private:
  friend class PaintInfoTest;
  FRIEND_TEST_ALL_PREFIXES(PaintInfoTest, LayerPaintInfo);

  PaintInfo(const ui::PaintContext& root_context, const gfx::Size& size);
  PaintInfo(const PaintInfo& parent_paint_info,
            const gfx::Rect& bounds,
            const gfx::Size& parent_size,
            ScaleType scale_type,
            bool is_layer,
            bool needs_paint = false);

  // Scales the |child_bounds| to its recording bounds based on the
  // |context.device_scale_factor()|. The recording bounds are snapped to the
  // parent's right and/or bottom edge if required.
  // If pixel canvas is disabled, this function returns |child_bounds| as is.
  gfx::Rect GetSnappedRecordingBounds(const gfx::Size& parent_size,
                                      const gfx::Rect& child_bounds) const;

  // The scale at which the paint commands are recorded at. Due to the decimal
  // rounding and snapping to edges during the scale operation, the effective
  // paint recording scale may end up being slightly different between the x and
  // y axis.
  float paint_recording_scale_x_;
  float paint_recording_scale_y_;

  // Paint Recording bounds of the view. The offset is relative to the root
  // PaintInfo.
  const gfx::Rect paint_recording_bounds_;

  // Offset relative to the parent view's paint recording bounds. Returns 0
  // offset if this is the root.
  gfx::Vector2d offset_from_parent_;

  // Compositor PaintContext associated with the view this object belongs to.
  ui::PaintContext context_;
  raw_ptr<const ui::PaintContext> root_context_;

  // True if the individual View has been marked invalid for paint (i.e.
  // SchedulePaint() was invoked on the View).
  bool needs_paint_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_PAINT_INFO_H_
