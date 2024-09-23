// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_mode.h"

#include <ostream>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"

namespace display {

DisplayMode::DisplayMode(const gfx::Size& size,
                         bool interlaced,
                         float refresh_rate,
                         const std::optional<float>& vsync_rate_min)
    : size_(size),
      refresh_rate_(refresh_rate),
      is_interlaced_(interlaced),
      vsync_rate_min_(vsync_rate_min) {}

DisplayMode::DisplayMode(const gfx::Size& size,
                         bool interlaced,
                         float refresh_rate)
    : DisplayMode(size,
                  interlaced,
                  refresh_rate,
                  /*vsync_rate_min=*/std::nullopt) {}

DisplayMode::~DisplayMode() = default;

std::unique_ptr<DisplayMode> DisplayMode::Clone() const {
  return base::WrapUnique(
      new DisplayMode(size_, is_interlaced_, refresh_rate_, vsync_rate_min_));
}

std::unique_ptr<DisplayMode> DisplayMode::CopyWithSize(
    const gfx::Size& size) const {
  return std::make_unique<DisplayMode>(size, is_interlaced_, refresh_rate_,
                                       vsync_rate_min_);
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
         refresh_rate_ == other.refresh_rate_ &&
         vsync_rate_min_ == other.vsync_rate_min_;
}

std::string DisplayMode::ToString() const {
  return base::StringPrintf("[size=%s %srate=%f vsync_min=%f]",
                            size_.ToString().c_str(),
                            is_interlaced_ ? "interlaced " : "", refresh_rate_,
                            vsync_rate_min_.value_or(0));
}

std::string DisplayMode::ToStringForTest() const {
  return base::StringPrintf("[size=%s %srate=%f]", size_.ToString().c_str(),
                            is_interlaced_ ? "interlaced " : "", refresh_rate_);
}

void PrintTo(const DisplayMode& mode, std::ostream* os) {
  *os << mode.ToString();
}

}  // namespace display
