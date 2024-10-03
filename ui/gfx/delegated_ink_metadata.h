// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DELEGATED_INK_METADATA_H_
#define UI_GFX_DELEGATED_INK_METADATA_H_

#include <string>

#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Maximum number of points that can be drawn. This is used to limit the total
// number of ink trail tokens that we will store, and the total number of points
// that we will store to provide to the Direct Composition APIs. It should match
// the exact number of points that the OS Compositor will store to draw as part
// of a trail.
inline constexpr int kMaximumNumberOfDelegatedInkPoints = 128;

// This class stores all the metadata that is gathered when the WebAPI
// updateInkTrailStartPoint is called. This metadata flows from blink,
// through cc, and into viz in order to produce a delegated ink trail on the
// end of what was already rendered.
//
// Explainer for the feature:
// https://github.com/WICG/ink-enhancement/blob/main/README.md
class GFX_EXPORT DelegatedInkMetadata {
 public:
  DelegatedInkMetadata() = default;
  bool operator==(const DelegatedInkMetadata& o) const = default;
  DelegatedInkMetadata(const PointF& pt,
                       double diameter,
                       SkColor color,
                       base::TimeTicks timestamp,
                       const RectF& area,
                       bool hovering)
      : point_(pt),
        diameter_(diameter),
        color_(color),
        timestamp_(timestamp),
        presentation_area_(area),
        is_hovering_(hovering) {}
  DelegatedInkMetadata(const PointF& pt,
                       double diameter,
                       SkColor color,
                       base::TimeTicks timestamp,
                       const RectF& area,
                       base::TimeTicks frame_time,
                       bool hovering,
                       std::uint64_t render_pass_id)
      : point_(pt),
        diameter_(diameter),
        color_(color),
        timestamp_(timestamp),
        presentation_area_(area),
        frame_time_(frame_time),
        is_hovering_(hovering),
        render_pass_id_(render_pass_id) {}
  DelegatedInkMetadata(const DelegatedInkMetadata& other) = default;
  DelegatedInkMetadata& operator=(const DelegatedInkMetadata& other) = default;

  const PointF& point() const { return point_; }
  double diameter() const { return diameter_; }
  SkColor color() const { return color_; }
  base::TimeTicks timestamp() const { return timestamp_; }
  const RectF& presentation_area() const { return presentation_area_; }
  base::TimeTicks frame_time() const { return frame_time_; }
  std::uint64_t render_pass_id() const { return render_pass_id_; }
  bool is_hovering() const { return is_hovering_; }
  void set_render_pass_id(std::uint64_t render_pass_id) {
    render_pass_id_ = render_pass_id;
  }
  void set_frame_time(base::TimeTicks frame_time) { frame_time_ = frame_time; }
  uint64_t trace_id() const {
    // Use mask to distinguish from DelegatedInkPoint::trace_id().
    // Using microseconds provides uniqueness of trace_id per
    // DelegatedInkMetadata.
    return static_cast<uint64_t>(timestamp_.since_origin().InMicroseconds()) |
           (uint64_t{1} << 63);
  }
  std::string ToString() const;

 private:
  // Location of the pointerevent relative to the root frame.
  PointF point_;

  // Width of the trail, in physical pixels.
  double diameter_ = 0;

  // Color to draw the ink trail.
  SkColor color_ = 0;

  // Timestamp from the pointerevent for the ink point.
  base::TimeTicks timestamp_;

  // The rect to clip the ink trail to, defaults to the containing viewport.
  RectF presentation_area_;

  // Frame time of the layer tree that this metadata is on.
  base::TimeTicks frame_time_;

  // True if the left mouse button is up or if a stylus with hovering
  // capabilities is hovering over the screen when updateInkTrailStartPoint is
  // called.
  bool is_hovering_ = false;

  // Id of the render pass that will have the delegated ink drawn on top of.
  // This is used when delegated ink is rendered by Skia. Only used within the
  // gpu process - not serialized over mojo.
  std::uint64_t render_pass_id_ = 0;
};

}  // namespace gfx

#endif  // UI_GFX_DELEGATED_INK_METADATA_H_
