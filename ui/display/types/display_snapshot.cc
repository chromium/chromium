// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_snapshot.h"

#include <inttypes.h>

#include <sstream>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace display {

namespace {

// The display serial number beginning byte position and its length in the
// EDID number as defined in the spec.
// https://en.wikipedia.org/wiki/Extended_Display_Identification_Data
constexpr size_t kSerialNumberBeginningByte = 12U;
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
    bool has_content_protection_key,
    const ColorInfo& color_info,
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
      has_content_protection_key_(has_content_protection_key),
      color_info_(color_info),
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
      drm_formats_and_modifiers_(drm_formats_and_modifiers) {
  // We must explicitly clear out the bytes that represent the serial number.
  const size_t end = std::min(
      kSerialNumberBeginningByte + kSerialNumberLengthInBytes, edid_.size());
  for (size_t i = kSerialNumberBeginningByte; i < end; ++i) {
    edid_[i] = 0;
  }
}

DisplaySnapshot::~DisplaySnapshot() = default;

std::unique_ptr<DisplaySnapshot> DisplaySnapshot::Clone() const {
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

  auto clone = std::make_unique<DisplaySnapshot>(
      display_id_, port_display_id_, edid_display_id_, connector_index_,
      origin_, physical_size_, type_, base_connector_id_, path_topology_,
      is_aspect_preserving_scaling_, has_overscan_, privacy_screen_state_,
      has_content_protection_key_, color_info_, display_name_, sys_path_,
      std::move(clone_modes), panel_orientation_, edid_, cloned_current_mode,
      cloned_native_mode, product_code_, year_of_manufacture_,
      maximum_cursor_size_, variable_refresh_rate_state_,
      drm_formats_and_modifiers_);
  // Set current mode in case it is non-native (because non-native modes are not
  // cloned).
  clone->set_current_mode(current_mode_);

  return clone;
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

bool DisplaySnapshot::IsVrrCapable() const {
  return variable_refresh_rate_state_ !=
         VariableRefreshRateState::kVrrNotCapable;
}

bool DisplaySnapshot::IsVrrEnabled() const {
  return variable_refresh_rate_state_ == VariableRefreshRateState::kVrrEnabled;
}

void DisplaySnapshot::set_current_mode(const DisplayMode* mode) {
  if (current_mode_ == mode ||
      (current_mode_ && mode && *current_mode_ == *mode)) {
    return;
  }

  if (!mode) {
    current_mode_ = nullptr;
    return;
  }

  for (const auto& owned_mode : modes_) {
    if (*owned_mode == *mode) {
      current_mode_ = owned_mode.get();
      return;
    }
  }

  for (const auto& owned_mode : nonnative_modes_) {
    if (*owned_mode == *mode) {
      current_mode_ = owned_mode.get();
      return;
    }
  }

  // Unowned modes can occur due to panel fitting or virtual modes.
  VLOG(3) << "Encountered mode which does not natively belong to display: "
          << mode->ToString();
  // The clone will persist as an owned non-native mode until the snapshot is
  // destructed. Do not attempt to delete it earlier in case it has been
  // accessed elsewhere.
  nonnative_modes_.push_back(mode->Clone());
  current_mode_ = nonnative_modes_.back().get();
}

}  // namespace display
