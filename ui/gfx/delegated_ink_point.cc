// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/delegated_ink_point.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/delegated_ink_metadata.h"

namespace gfx {

bool DelegatedInkPoint::MatchesDelegatedInkMetadata(
    const DelegatedInkMetadata* metadata) const {
  if (!metadata) {
    return false;
  }
  // The maximum difference allowed when comparing a DelegatedInkPoint |point_|
  // to the |point_| on a DelegatedInkMetadata. Some precision loss can occur
  // when moving between coordinate spaces in the browser and renderer,
  // particularly when the device scale factor is not a whole number. This can
  // result in a DelegatedInkMetadata and DelegatedInkPoint having been created
  // from the same point, but having a very small difference. When this occurs,
  // we can safely ignore that they are slightly different and use the point for
  // a delegated ink trail anyway, since it is a very small difference and will
  // only be visible for a single frame. The allowed difference is the square
  // root of 2 - which is derived from the maximum allowed difference of 1px in
  // each direction.
  // TODO(crbug.com/338250110) Consider removing this tolerance.
  constexpr float kEpsilon = 1.4143f;
  TRACE_EVENT_INSTANT2("delegated_ink_trails",
                       "DelegatedInkPoint::MatchesDelegatedInkMetadata",
                       TRACE_EVENT_SCOPE_THREAD, "metadata",
                       metadata->ToString(), "point", ToString());
  return timestamp_ == metadata->timestamp() &&
         point_.IsWithinDistance(metadata->point(), kEpsilon);
}

std::string DelegatedInkPoint::ToString() const {
  return base::StringPrintf("point: %s, timestamp: %" PRId64 ", pointer_id: %d",
                            point_.ToString().c_str(),
                            timestamp_.since_origin().InMicroseconds(),
                            pointer_id_);
}

}  // namespace gfx
