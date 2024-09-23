// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DELEGATED_INK_POINT_H_
#define UI_GFX_DELEGATED_INK_POINT_H_

#include <limits>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class DelegatedInkMetadata;
namespace mojom {
class DelegatedInkPointDataView;
}  // namespace mojom

// This class stores the information required to draw a single point of a
// delegated ink trail. When the WebAPI |updateInkTrailStartPoint| is called,
// the renderer requests that the browser begin sending these to viz. Viz
// will collect them, and then during |DrawAndSwap| will use the
// DelegatedInkPoints that have arrived from the browser along with the
// DelegatedInkMetadata that the renderer sent to draw a delegated ink trail on
// the screen, connected to the end of the already rendered ink stroke.
//
// Explainer for the feature:
// https://github.com/WICG/ink-enhancement/blob/main/README.md
class GFX_EXPORT DelegatedInkPoint {
 public:
  DelegatedInkPoint() = default;
  bool operator==(const DelegatedInkPoint& o) const = default;
  DelegatedInkPoint(const PointF& pt,
                    base::TimeTicks timestamp,
                    int32_t pointer_id = std::numeric_limits<int32_t>::min())
      : point_(pt), timestamp_(timestamp), pointer_id_(pointer_id) {}

  const PointF& point() const { return point_; }
  base::TimeTicks timestamp() const { return timestamp_; }
  std::optional<base::TimeTicks> paint_timestamp() const {
    return paint_timestamp_;
  }
  void set_paint_timestamp(base::TimeTicks ts) { paint_timestamp_ = ts; }
  int32_t pointer_id() const { return pointer_id_; }
  std::string ToString() const;

  bool MatchesDelegatedInkMetadata(const DelegatedInkMetadata* metadata) const;
  uint64_t trace_id() const {
    // Use mask to distinguish from DelegatedInkMetadata::trace_id().
    // Using microseconds provides uniqueness of trace_id per
    // DelegatedInkPoint.
    return timestamp_.since_origin().InMicroseconds() & 0x7fffffffffffffff;
  }

 private:
  friend struct mojo::StructTraits<mojom::DelegatedInkPointDataView,
                                   DelegatedInkPoint>;

  // Location of the input event relative to the root window in device pixels.
  // Scale is device scale factor at time of input.
  PointF point_;

  // Timestamp from the input event.
  base::TimeTicks timestamp_;

  // Timestamp from the the first time the point is painted.
  std::optional<base::TimeTicks> paint_timestamp_;

  // Pointer ID from the input event. Used to store all DelegatedInkPoints from
  // the same source together in viz so that they are all candidates for a
  // single delegated ink trail and DelegatedInkPoints from other sources are
  // not.
  int32_t pointer_id_;
};

}  // namespace gfx

#endif  // UI_GFX_DELEGATED_INK_POINT_H_
