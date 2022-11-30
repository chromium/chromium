// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/delegated_ink_point.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "ui/gfx/delegated_ink_metadata.h"

namespace gfx {

bool DelegatedInkPoint::MatchesDelegatedInkMetadata(
    const DelegatedInkMetadata* metadata) const {
  // The maximum difference allowed when comparing a DelegatedInkPoint |point_|
  // to the |point_| on a DelegatedInkMetadata. Some precision loss can occur
  // when moving between coordinate spaces in the browser and renderer,
  // particularly when the device scale factor is not a whole number. This can
  // result in a DelegatedInkMetadata and DelegatedInkPoint having been created
  // from the same point, but having a very small difference. When this occurs,
  // we can safely ignore that they are slightly different and use the point for
  // a delegated ink trail anyway, since it is a very small difference and will
  // only be visible for a single frame.
  constexpr float kEpsilon = 0.05f;

  return metadata && timestamp_ == metadata->timestamp() &&
         point_.IsWithinDistance(metadata->point(), kEpsilon);
}

std::string DelegatedInkPoint::ToString() const {
  return base::StringPrintf("point: %s, timestamp: %" PRId64 ", pointer_id: %d",
                            point_.ToString().c_str(),
                            timestamp_.since_origin().InMicroseconds(),
                            pointer_id_);
}

}  // namespace gfx
