// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_snapshot.h"

#include <inttypes.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/notreached.h"
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

DisplaySnapshot::DisplaySnapshot(
    int64_t display_id,
    int64_t port_display_id,
    int64_t edid_display_id,
    uint16_t connector_index,
    const gfx::Point& origin,
    const gfx::Size& physical_size,
    DisplayConnectionType type,
    uint64_t base_connector_id,
    const std::vector<uint64_t>& path_topology,
    bool is_aspect_preserving_scaling,
    bool has_overscan,
    PrivacyScreenState privacy_screen_state,
    bool has_color_correction_matrix,
    bool color_correction_in_linear_space,
    const gfx::ColorSpace& color_space,
    uint32_t bits_per_channel,
    const absl::optional<gfx::HDRStaticMetadata>& hdr_static_metadata,
    std::string display_name,
    const base::FilePath& sys_path,
    DisplayModeList modes,
    PanelOrientation panel_orientation,
    const std::vector<uint8_t>& edid,
    const DisplayMode* current_mode,
    const DisplayMode* native_mode,
    int64_t product_code,
    int32_t year_of_manufacture,
    const gfx::Size& maximum_cursor_size,
    VariableRefreshRateState variable_refresh_rate_state,
    const absl::optional<gfx::Range>& vertical_display_range_limits,
    const DrmFormatsAndModifiers& drm_formats_and_modifiers)
    : display_id_(display_id),
      port_display_id_(port_display_id),
      edid_display_id_(edid_display_id),
      connector_index_(connector_index),
      origin_(origin),
      physical_size_(physical_size),
      type_(type),
      base_connector_id_(base_connector_id),
      path_topology_(path_topology),
      is_aspect_preserving_scaling_(is_aspect_preserving_scaling),
      has_overscan_(has_overscan),
      privacy_screen_state_(privacy_screen_state),
      has_color_correction_matrix_(has_color_correction_matrix),
      color_correction_in_linear_space_(color_correction_in_linear_space),
      color_space_(color_space),
      bits_per_channel_(bits_per_channel),
      hdr_static_metadata_(hdr_static_metadata),
      display_name_(display_name),
      sys_path_(sys_path),
      modes_(std::move(modes)),
      panel_orientation_(panel_orientation),
      edid_(edid),
      current_mode_(current_mode),
      native_mode_(native_mode),
      product_code_(product_code),
      year_of_manufacture_(year_of_manufacture),
      maximum_cursor_size_(maximum_cursor_size),
      variable_refresh_rate_state_(variable_refresh_rate_state),
      vertical_display_range_limits_(vertical_display_range_limits),
      drm_formats_and_modifiers_(drm_formats_and_modifiers) {
  // We must explicitly clear out the bytes that represent the serial number.
  const size_t end = std::min(
      kSerialNumberBeginingByte + kSerialNumberLengthInBytes, edid_.size());
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
      display_id_, port_display_id_, edid_display_id_, connector_index_,
      origin_, physical_size_, type_, base_connector_id_, path_topology_,
      is_aspect_preserving_scaling_, has_overscan_, privacy_screen_state_,
      has_color_correction_matrix_, color_correction_in_linear_space_,
      color_space_, bits_per_channel_, hdr_static_metadata_, display_name_,
      sys_path_, std::move(clone_modes), panel_orientation_, edid_,
      cloned_current_mode, cloned_native_mode, product_code_,
      year_of_manufacture_, maximum_cursor_size_, variable_refresh_rate_state_,
      vertical_display_range_limits_, drm_formats_and_modifiers_);
}

std::string DisplaySnapshot::ToString() const {
  std::string sharing_connector;
  if (base_connector_id_) {
    sharing_connector = path_topology_.empty() ? "NO" : "YES";
  } else {
    sharing_connector = "parsing_error";
  }

  return base::StringPrintf(
      "id=%" PRId64
      " current_mode=%s native_mode=%s origin=%s"
      " panel_orientation=%d"
      " physical_size=%s, type=%s sharing_base_connector=%s name=\"%s\" "
      "(year:%d) modes=(%s)",
      display_id(),
      current_mode_ ? current_mode_->ToString().c_str() : "nullptr",
      native_mode_ ? native_mode_->ToString().c_str() : "nullptr",
      origin_.ToString().c_str(), panel_orientation_,
      physical_size_.ToString().c_str(),
      DisplayConnectionTypeString(type_).c_str(), sharing_connector.c_str(),
      display_name_.c_str(), year_of_manufacture_,
      ModeListString(modes_).c_str());
}

// static
gfx::BufferFormat DisplaySnapshot::PrimaryFormat() {
  return gfx::BufferFormat::BGRA_8888;
}

void DisplaySnapshot::AddIndexToDisplayId() {
  // The EDID-based display ID occupies the first 32 bits of |edid_display_id_|.
  edid_display_id_ |= static_cast<int64_t>(connector_index_) << 32;
}

}  // namespace display
