// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_snapshot.h"

#include <inttypes.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/strings/stringprintf.h"

namespace display {

namespace {

// The display serial number beginning byte position and its length in the
// EDID number as defined in the spec.
// https://en.wikipedia.org/wiki/Extended_Display_Identification_Data
constexpr size_t kSerialNumberBeginingByte = 12U;
constexpr size_t kSerialNumberLengthInBytes = 4U;

std::string ModeListString(
    const std::vector<std::unique_ptr<const DisplayMode>>& modes) {
  std::stringstream stream;
  bool first = true;
  for (auto& mode : modes) {
    if (!first)
      stream << ", ";
    stream << mode->ToString();
    first = false;
  }
  return stream.str();
}

std::string DisplayConnectionTypeString(DisplayConnectionType type) {
  switch (type) {
    case DISPLAY_CONNECTION_TYPE_NONE:
      return "none";
    case DISPLAY_CONNECTION_TYPE_UNKNOWN:
      return "unknown";
    case DISPLAY_CONNECTION_TYPE_INTERNAL:
      return "internal";
    case DISPLAY_CONNECTION_TYPE_VGA:
      return "vga";
    case DISPLAY_CONNECTION_TYPE_HDMI:
      return "hdmi";
    case DISPLAY_CONNECTION_TYPE_DVI:
      return "dvi";
    case DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      return "dp";
    case DISPLAY_CONNECTION_TYPE_NETWORK:
      return "network";
  }
  NOTREACHED();
  return "";
}

}  // namespace

DisplaySnapshot::DisplaySnapshot(int64_t display_id,
                                 const gfx::Point& origin,
                                 const gfx::Size& physical_size,
                                 DisplayConnectionType type,
                                 bool is_aspect_preserving_scaling,
                                 bool has_overscan,
                                 bool has_color_correction_matrix,
                                 bool color_correction_in_linear_space,
                                 const gfx::ColorSpace& color_space,
                                 uint32_t bits_per_channel,
                                 std::string display_name,
                                 const base::FilePath& sys_path,
                                 DisplayModeList modes,
                                 PanelOrientation panel_orientation,
                                 const std::vector<uint8_t>& edid,
                                 const DisplayMode* current_mode,
                                 const DisplayMode* native_mode,
                                 int64_t product_code,
                                 int32_t year_of_manufacture,
                                 const gfx::Size& maximum_cursor_size)
    : display_id_(display_id),
      origin_(origin),
      physical_size_(physical_size),
      type_(type),
      is_aspect_preserving_scaling_(is_aspect_preserving_scaling),
      has_overscan_(has_overscan),
      has_color_correction_matrix_(has_color_correction_matrix),
      color_correction_in_linear_space_(color_correction_in_linear_space),
      color_space_(color_space),
      bits_per_channel_(bits_per_channel),
      display_name_(display_name),
      sys_path_(sys_path),
      modes_(std::move(modes)),
      panel_orientation_(panel_orientation),
      edid_(edid),
      current_mode_(current_mode),
      native_mode_(native_mode),
      product_code_(product_code),
      year_of_manufacture_(year_of_manufacture),
      maximum_cursor_size_(maximum_cursor_size) {
  // We must explicitly clear out the bytes that represent the serial number.
  const size_t end =
      std::min(kSerialNumberBeginingByte + kSerialNumberLengthInBytes,
               edid_.size());
  for (size_t i = kSerialNumberBeginingByte; i < end; ++i)
    edid_[i] = 0;
}

DisplaySnapshot::~DisplaySnapshot() {}

std::unique_ptr<DisplaySnapshot> DisplaySnapshot::Clone() {
  DisplayModeList clone_modes;
  const DisplayMode* cloned_current_mode = nullptr;
  const DisplayMode* cloned_native_mode = nullptr;

  // Clone the display modes and find equivalent pointers to the native and
  // current mode.
  for (auto& mode : modes_) {
    clone_modes.push_back(mode->Clone());
    if (mode.get() == current_mode_)
      cloned_current_mode = clone_modes.back().get();
    if (mode.get() == native_mode_)
      cloned_native_mode = clone_modes.back().get();
  }

  return std::make_unique<DisplaySnapshot>(
      display_id_, origin_, physical_size_, type_,
      is_aspect_preserving_scaling_, has_overscan_,
      has_color_correction_matrix_, color_correction_in_linear_space_,
      color_space_, bits_per_channel_, display_name_, sys_path_,
      std::move(clone_modes), panel_orientation_, edid_, cloned_current_mode,
      cloned_native_mode, product_code_, year_of_manufacture_,
      maximum_cursor_size_);
}

std::string DisplaySnapshot::ToString() const {
  return base::StringPrintf(
      "id=%" PRId64
      " current_mode=%s native_mode=%s origin=%s"
      " panel_orientation=%d"
      " physical_size=%s, type=%s name=\"%s\" (year:%d) "
      "modes=(%s)",
      display_id_,
      current_mode_ ? current_mode_->ToString().c_str() : "nullptr",
      native_mode_ ? native_mode_->ToString().c_str() : "nullptr",
      origin_.ToString().c_str(), panel_orientation_,
      physical_size_.ToString().c_str(),
      DisplayConnectionTypeString(type_).c_str(), display_name_.c_str(),
      year_of_manufacture_, ModeListString(modes_).c_str());
}

// static
gfx::BufferFormat DisplaySnapshot::PrimaryFormat() {
  return gfx::BufferFormat::BGRA_8888;
}

}  // namespace display
