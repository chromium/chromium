// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_mode.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"

namespace display {

DisplayMode::DisplayMode(const gfx::Size& size,
                         bool interlaced,
                         float refresh_rate,
                         int htotal,
                         int vtotal,
                         int clock)
    : size_(size),
      refresh_rate_(refresh_rate),
      is_interlaced_(interlaced),
      htotal_(htotal),
      vtotal_(vtotal),
      clock_(clock) {}

DisplayMode::~DisplayMode() {}

std::unique_ptr<DisplayMode> DisplayMode::Clone() const {
  return base::WrapUnique(new DisplayMode(size_, is_interlaced_, refresh_rate_,
                                          htotal_, vtotal_, clock_));
}

bool DisplayMode::operator<(const DisplayMode& other) const {
  if (size_.GetArea() < other.size_.GetArea())
    return true;
  if (size_.GetArea() > other.size_.GetArea())
    return false;
  if (size_.width() < other.size_.width())
    return true;
  if (size_.width() > other.size_.width())
    return false;
  return refresh_rate_ < other.refresh_rate_;
}

bool DisplayMode::operator>(const DisplayMode& other) const {
  return other < *this;
}

bool DisplayMode::operator==(const DisplayMode& other) const {
  return size_ == other.size_ && is_interlaced_ == other.is_interlaced_ &&
         refresh_rate_ == other.refresh_rate_ && htotal_ == other.htotal_ &&
         vtotal_ == other.vtotal_ && clock_ == other.clock_;
}

float DisplayMode::GetVSyncRateMin(int vsync_rate_min_from_edid) const {
  if (!htotal_) {
    return vsync_rate_min_from_edid;
  }

  float clock_hz = clock_ * 1000.0f;
  float htotal = htotal_;

  // Calculate the vtotal from the imprecise min vsync rate.
  float vtotal_extended = clock_hz / (htotal * vsync_rate_min_from_edid);
  // Clamp the calculated vtotal and determine the precise min vsync rate.
  return clock_hz / (htotal * std::floor(vtotal_extended));
}

std::string DisplayMode::ToString() const {
  return base::StringPrintf("[size=%s htot=%d vtot=%d clock=%d %srate=%f]",
                            size_.ToString().c_str(), htotal_, vtotal_, clock_,
                            is_interlaced_ ? "interlaced " : "", refresh_rate_);
}

void PrintTo(const DisplayMode& mode, std::ostream* os) {
  *os << mode.ToString();
}

}  // namespace display
