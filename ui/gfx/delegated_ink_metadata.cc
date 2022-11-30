// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/delegated_ink_metadata.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"

namespace gfx {

std::string DelegatedInkMetadata::ToString() const {
  std::string str = base::StringPrintf(
      "point: %s, diameter: %f, color: %u, timestamp: %" PRId64
      ", presentation_area: %s, frame_time: %" PRId64 ", is_hovering: %d",
      point_.ToString().c_str(), diameter_, color_,
      timestamp_.since_origin().InMicroseconds(),
      presentation_area_.ToString().c_str(),
      frame_time_.since_origin().InMicroseconds(), is_hovering_);
  return str;
}

}  // namespace gfx
