// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/managed_display_info.h"

#include <stdio.h>

#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

#if defined(OS_WIN)
#include <windows.h>
#include "ui/display/win/dpi.h"
#endif

namespace display {
namespace {

// Use larger than max int to catch overflow early.
const int64_t kSynthesizedDisplayIdStart = 2200000000LL;

int64_t next_synthesized_display_id = kSynthesizedDisplayIdStart;

const float kDpi96 = 96.0;

// Check the content of |spec| and fill |bounds| and |device_scale_factor|.
// Returns true when |bounds| is found.
bool GetDisplayBounds(const std::string& spec,
                      gfx::Rect* bounds,
                      float* device_scale_factor) {
  int width = 0;
  int height = 0;
  int x = 0;
  int y = 0;
  if (sscanf(spec.c_str(), "%dx%d*%f", &width, &height, device_scale_factor) >=
          2 ||
      sscanf(spec.c_str(), "%d+%d-%dx%d*%f", &x, &y, &width, &height,
             device_scale_factor) >= 4) {
    bounds->SetRect(x, y, width, height);

    auto equals_within_epsilon = [device_scale_factor](float dsf) {
      return std::abs(*device_scale_factor - dsf) < 0.01f;
    };
    if (equals_within_epsilon(1.77f)) {
      *device_scale_factor = kDsf_1_777;
    } else if (equals_within_epsilon(2.25f)) {
      *device_scale_factor = kDsf_2_252;
    } else if (equals_within_epsilon(2.66)) {
      *device_scale_factor = kDsf_2_666;
    }
    return true;
  }
  return false;
}

// Display mode list is sorted by:
//  * the area in pixels in ascending order
//  * refresh rate in descending order
struct ManagedDisplayModeSorter {
  bool operator()(const ManagedDisplayMode& a, const ManagedDisplayMode& b) {
    gfx::Size size_a_dip = a.GetSizeInDIP();
    gfx::Size size_b_dip = b.GetSizeInDIP();
    if (size_a_dip.GetArea() == size_b_dip.GetArea())
      return (a.refresh_rate() > b.refresh_rate());
    return (size_a_dip.GetArea() < size_b_dip.GetArea());
  }
};

bool IsWithinEpsilon(float a, float b) {
  constexpr float kEpsilon = 0.0001f;
  return std::abs(a - b) < kEpsilon;
}

}  // namespace

ManagedDisplayMode::ManagedDisplayMode() {}

ManagedDisplayMode::ManagedDisplayMode(const gfx::Size& size) : size_(size) {}

ManagedDisplayMode::ManagedDisplayMode(const gfx::Size& size,
                                       float refresh_rate,
                                       bool is_interlaced,
                                       bool native)
    : size_(size),
      refresh_rate_(refresh_rate),
      is_interlaced_(is_interlaced),
      native_(native) {}

ManagedDisplayMode::ManagedDisplayMode(const gfx::Size& size,
                                       float refresh_rate,
                                       bool is_interlaced,
                                       bool native,
                                       float device_scale_factor)
    : size_(size),
      refresh_rate_(refresh_rate),
      is_interlaced_(is_interlaced),
      native_(native),
      device_scale_factor_(device_scale_factor) {}

ManagedDisplayMode::~ManagedDisplayMode() = default;

ManagedDisplayMode::ManagedDisplayMode(const ManagedDisplayMode& other) =
    default;

ManagedDisplayMode& ManagedDisplayMode::operator=(
    const ManagedDisplayMode& other) = default;

bool ManagedDisplayMode::operator==(const ManagedDisplayMode& other) const {
  return size_ == other.size_ && is_interlaced_ == other.is_interlaced_ &&
         native_ == other.native_ &&
         IsWithinEpsilon(refresh_rate_, other.refresh_rate_) &&
         IsWithinEpsilon(device_scale_factor_, other.device_scale_factor_);
}

gfx::Size ManagedDisplayMode::GetSizeInDIP() const {
  gfx::SizeF size_dip(size_);
  size_dip.Scale(1.0f / device_scale_factor_);
  return gfx::ToFlooredSize(size_dip);
}

bool ManagedDisplayMode::IsEquivalent(const ManagedDisplayMode& other) const {
  if (display::features::IsListAllDisplayModesEnabled())
    return *this == other;

  return size_ == other.size_ &&
         IsWithinEpsilon(device_scale_factor_, other.device_scale_factor_);
}

std::string ManagedDisplayMode::ToString() const {
  return base::StringPrintf(
      "DisplayMode{size: %s, refresh_rate: %f, interlaced:"
      " %d, native: %d, device_scale_factor: %f}",
      size_.ToString().c_str(), refresh_rate_, is_interlaced_, native_,
      device_scale_factor_);
}

// static
ManagedDisplayInfo ManagedDisplayInfo::CreateFromSpec(const std::string& spec) {
  return CreateFromSpecWithID(spec, kInvalidDisplayId);
}

// static
ManagedDisplayInfo ManagedDisplayInfo::CreateFromSpecWithID(
    const std::string& spec,
    int64_t id) {
#if defined(OS_WIN)
  gfx::Rect bounds_in_native(
      gfx::Size(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)));
#else
  // Default bounds for a display.
  const int kDefaultHostWindowX = 200;
  const int kDefaultHostWindowY = 200;
  const int kDefaultHostWindowWidth = 1366;
  const int kDefaultHostWindowHeight = 768;
  gfx::Rect bounds_in_native(kDefaultHostWindowX, kDefaultHostWindowY,
                             kDefaultHostWindowWidth, kDefaultHostWindowHeight);
#endif
  std::string main_spec = spec;

  float zoom_factor = 1.0f;
  std::vector<std::string> parts = base::SplitString(
      main_spec, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 2) {
    double scale_in_double = 0;
    if (base::StringToDouble(parts[1], &scale_in_double))
      zoom_factor = scale_in_double;
    main_spec = parts[0];
  }

  parts = base::SplitString(main_spec, "/", base::KEEP_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
  Display::Rotation rotation(Display::ROTATE_0);
  bool has_overscan = false;
  if (!parts.empty()) {
    main_spec = parts[0];
    if (parts.size() >= 2) {
      std::string options = parts[1];
      for (size_t i = 0; i < options.size(); ++i) {
        char c = options[i];
        switch (c) {
          case 'o':
            has_overscan = true;
            break;
          case 'r':  // rotate 90 degrees to 'right'.
            rotation = Display::ROTATE_90;
            break;
          case 'u':  // 180 degrees, 'u'pside-down.
            rotation = Display::ROTATE_180;
            break;
          case 'l':  // rotate 90 degrees to 'left'.
            rotation = Display::ROTATE_270;
            break;
        }
      }
    }
  }

  float device_scale_factor = 1.0f;
  if (!GetDisplayBounds(main_spec, &bounds_in_native, &device_scale_factor)) {
#if defined(OS_WIN)
    device_scale_factor = win::GetDPIScale();
#endif
  }

  ManagedDisplayModeList display_modes;
  parts = base::SplitString(main_spec, "#", base::KEEP_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 2) {
    size_t native_mode = 0;
    int largest_area = -1;
    float highest_refresh_rate = -1.0f;
    main_spec = parts[0];
    std::string resolution_list = parts[1];
    parts = base::SplitString(resolution_list, "|", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY);
    for (size_t i = 0; i < parts.size(); ++i) {
      gfx::Size size;
      float refresh_rate = 60.0f;
      bool is_interlaced = false;

      gfx::Rect mode_bounds;
      std::vector<std::string> resolution = base::SplitString(
          parts[i], "%", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (GetDisplayBounds(resolution[0], &mode_bounds, &device_scale_factor)) {
        size = mode_bounds.size();
        if (resolution.size() > 1)
          sscanf(resolution[1].c_str(), "%f", &refresh_rate);
        if (size.GetArea() >= largest_area &&
            refresh_rate > highest_refresh_rate) {
          // Use mode with largest area and highest refresh rate as native.
          largest_area = size.GetArea();
          highest_refresh_rate = refresh_rate;
          native_mode = i;
        }
        display_modes.push_back(ManagedDisplayMode(
            size, refresh_rate, is_interlaced, false, device_scale_factor));
      }
    }
    ManagedDisplayMode dm = display_modes[native_mode];
    display_modes[native_mode] =
        ManagedDisplayMode(dm.size(), dm.refresh_rate(), dm.is_interlaced(),
                           true, dm.device_scale_factor());
  }

  if (id == kInvalidDisplayId) {
    id = next_synthesized_display_id;
    next_synthesized_display_id = GetNextSynthesizedDisplayId(id);
  }
  ManagedDisplayInfo display_info(
      id, base::StringPrintf("Display-%d", static_cast<int>(id)), has_overscan);
  display_info.set_device_scale_factor(device_scale_factor);
  display_info.SetRotation(rotation, Display::RotationSource::ACTIVE);
  display_info.set_zoom_factor(zoom_factor);
  display_info.SetBounds(bounds_in_native);

  if (!display_modes.size()) {
    display_modes.push_back(ManagedDisplayMode(
        display_info.size_in_pixel(), 60.0f,
        /*interlace=*/false, /*native=*/true, device_scale_factor));
  }

  display_info.SetManagedDisplayModes(display_modes);

  // To test the overscan, it creates the default 5% overscan.
  if (has_overscan) {
    int width = bounds_in_native.width() / device_scale_factor / 40;
    int height = bounds_in_native.height() / device_scale_factor / 40;
    display_info.SetOverscanInsets(gfx::Insets(height, width, height, width));
    display_info.UpdateDisplaySize();
  }

  DVLOG(1) << "DisplayInfoFromSpec info=" << display_info.ToString()
           << ", spec=" << spec;
  return display_info;
}

ManagedDisplayInfo::ManagedDisplayInfo()
    : id_(kInvalidDisplayId),
      year_of_manufacture_(kInvalidYearOfManufacture),
      has_overscan_(false),
      active_rotation_source_(Display::RotationSource::UNKNOWN),
      touch_support_(Display::TouchSupport::UNKNOWN),
      device_scale_factor_(1.0f),
      device_dpi_(kDpi96),
      panel_orientation_(display::PanelOrientation::kNormal),
      overscan_insets_in_dip_(0, 0, 0, 0),
      zoom_factor_(1.f),
      refresh_rate_(60.f),
      is_interlaced_(false),
      from_native_platform_(false),
      native_(false),
      is_aspect_preserving_scaling_(false),
      clear_overscan_insets_(false),
      bits_per_channel_(0) {}

ManagedDisplayInfo::ManagedDisplayInfo(int64_t id,
                                       const std::string& name,
                                       bool has_overscan)
    : id_(id),
      name_(name),
      year_of_manufacture_(kInvalidYearOfManufacture),
      has_overscan_(has_overscan),
      active_rotation_source_(Display::RotationSource::UNKNOWN),
      touch_support_(Display::TouchSupport::UNKNOWN),
      device_scale_factor_(1.0f),
      device_dpi_(kDpi96),
      panel_orientation_(display::PanelOrientation::kNormal),
      overscan_insets_in_dip_(0, 0, 0, 0),
      zoom_factor_(1.f),
      refresh_rate_(60.f),
      is_interlaced_(false),
      from_native_platform_(false),
      native_(false),
      is_aspect_preserving_scaling_(false),
      clear_overscan_insets_(false),
      bits_per_channel_(0) {}

ManagedDisplayInfo::ManagedDisplayInfo(const ManagedDisplayInfo& other) =
    default;

ManagedDisplayInfo::~ManagedDisplayInfo() {}

void ManagedDisplayInfo::SetRotation(Display::Rotation rotation,
                                     Display::RotationSource source) {
  rotations_[source] = rotation;
  rotations_[Display::RotationSource::ACTIVE] = rotation;
  active_rotation_source_ = source;
}

Display::Rotation ManagedDisplayInfo::GetActiveRotation() const {
  return GetRotation(Display::RotationSource::ACTIVE);
}

Display::Rotation ManagedDisplayInfo::GetLogicalActiveRotation() const {
  return GetRotationWithPanelOrientation(
      GetRotation(Display::RotationSource::ACTIVE));
}

Display::Rotation ManagedDisplayInfo::GetRotation(
    Display::RotationSource source) const {
  if (rotations_.find(source) == rotations_.end())
    return Display::ROTATE_0;
  return rotations_.at(source);
}

void ManagedDisplayInfo::Copy(const ManagedDisplayInfo& native_info) {
  DCHECK(id_ == native_info.id_);
  manufacturer_id_ = native_info.manufacturer_id_;
  product_id_ = native_info.product_id_;
  year_of_manufacture_ = native_info.year_of_manufacture_;
  name_ = native_info.name_;
  has_overscan_ = native_info.has_overscan_;

  active_rotation_source_ = native_info.active_rotation_source_;
  touch_support_ = native_info.touch_support_;
  device_scale_factor_ = native_info.device_scale_factor_;
  DCHECK(!native_info.bounds_in_native_.IsEmpty());
  bounds_in_native_ = native_info.bounds_in_native_;
  device_dpi_ = native_info.device_dpi_;
  panel_orientation_ = native_info.panel_orientation_,
  size_in_pixel_ = native_info.size_in_pixel_;
  is_aspect_preserving_scaling_ = native_info.is_aspect_preserving_scaling_;
  display_modes_ = native_info.display_modes_;
  maximum_cursor_size_ = native_info.maximum_cursor_size_;
  display_color_spaces_ = native_info.display_color_spaces_;
  bits_per_channel_ = native_info.bits_per_channel_;
  refresh_rate_ = native_info.refresh_rate_;
  is_interlaced_ = native_info.is_interlaced_;
  native_ = native_info.native_;

  // Rotation, color_profile and overscan are given by preference,
  // or unit tests. Don't copy if this native_info came from
  // DisplayChangeObserver.
  if (native_info.from_native_platform())
    return;
  // Update the overscan_insets_in_dip_ either if the inset should be
  // cleared, or has non empty insts.
  if (native_info.clear_overscan_insets())
    overscan_insets_in_dip_.Set(0, 0, 0, 0);
  else if (!native_info.overscan_insets_in_dip_.IsEmpty())
    overscan_insets_in_dip_ = native_info.overscan_insets_in_dip_;

  rotations_ = native_info.rotations_;
  zoom_factor_ = native_info.zoom_factor_;
}

void ManagedDisplayInfo::SetBounds(const gfx::Rect& new_bounds_in_native) {
  bounds_in_native_ = new_bounds_in_native;
  size_in_pixel_ = new_bounds_in_native.size();
  UpdateDisplaySize();
}

float ManagedDisplayInfo::GetEffectiveDeviceScaleFactor() const {
  return device_scale_factor_ * zoom_factor_;
}

gfx::Size ManagedDisplayInfo::GetSizeInPixelWithPanelOrientation() const {
  gfx::Size size = bounds_in_native_.size();
  if (panel_orientation_ == display::PanelOrientation::kLeftUp ||
      panel_orientation_ == display::PanelOrientation::kRightUp) {
    return gfx::Size(size.height(), size.width());
  }
  return size;
}

void ManagedDisplayInfo::UpdateDisplaySize() {
  size_in_pixel_ = GetSizeInPixelWithPanelOrientation();

  if (!overscan_insets_in_dip_.IsEmpty()) {
    gfx::Insets insets_in_pixel = GetOverscanInsetsInPixel();
    size_in_pixel_.Enlarge(-insets_in_pixel.width(), -insets_in_pixel.height());
  } else {
    overscan_insets_in_dip_.Set(0, 0, 0, 0);
  }

  if (GetActiveRotation() == Display::ROTATE_90 ||
      GetActiveRotation() == Display::ROTATE_270) {
    size_in_pixel_.SetSize(size_in_pixel_.height(), size_in_pixel_.width());
  }
}

void ManagedDisplayInfo::SetOverscanInsets(const gfx::Insets& insets_in_dip) {
  overscan_insets_in_dip_ = insets_in_dip;
}

gfx::Insets ManagedDisplayInfo::GetOverscanInsetsInPixel() const {
  return gfx::ToFlooredInsets(gfx::ConvertInsetsToPixels(
      overscan_insets_in_dip_, device_scale_factor_));
}

void ManagedDisplayInfo::SetManagedDisplayModes(
    const ManagedDisplayModeList& display_modes) {
  display_modes_ = display_modes;
  std::sort(display_modes_.begin(), display_modes_.end(),
            ManagedDisplayModeSorter());
}

gfx::Size ManagedDisplayInfo::GetNativeModeSize() const {
  for (const ManagedDisplayMode& display_mode : display_modes_) {
    if (display_mode.native())
      return display_mode.size();
  }
  return gfx::Size();
}

std::string ManagedDisplayInfo::ToString() const {
  int rotation_degree = static_cast<int>(GetActiveRotation()) * 90;

  std::string result = base::StringPrintf(
      "ManagedDisplayInfo[%lld] native bounds=%s, size=%s, device-scale=%g, "
      "display-zoom=%g, overscan=%s, rotation=%d, touchscreen=%s",
      static_cast<long long int>(id_), bounds_in_native_.ToString().c_str(),
      size_in_pixel_.ToString().c_str(), device_scale_factor_, zoom_factor_,
      overscan_insets_in_dip_.ToString().c_str(), rotation_degree,
      touch_support_ == Display::TouchSupport::AVAILABLE
          ? "yes"
          : touch_support_ == Display::TouchSupport::UNAVAILABLE ? "no"
                                                                 : "unknown");

  return result;
}

std::string ManagedDisplayInfo::ToFullString() const {
  std::string display_modes_str;
  for (const ManagedDisplayMode& m : display_modes_) {
    if (!display_modes_str.empty())
      display_modes_str += ",";
    base::StringAppendF(&display_modes_str, "(%dx%d@%g%c%s %g)",
                        m.size().width(), m.size().height(), m.refresh_rate(),
                        m.is_interlaced() ? 'I' : 'P', m.native() ? "(N)" : "",
                        m.device_scale_factor());
  }
  return ToString() + ", display_modes==" + display_modes_str;
}

Display::Rotation ManagedDisplayInfo::GetRotationWithPanelOrientation(
    Display::Rotation rotation) const {
  int offset = 0;
  switch (panel_orientation_) {
    case PanelOrientation::kNormal:
      break;
    case PanelOrientation::kBottomUp:
      offset = 2;
      break;
    case PanelOrientation::kRightUp:
      offset = 1;
      break;
    case PanelOrientation::kLeftUp:
      offset = 3;
      break;
  }
  return static_cast<Display::Rotation>((static_cast<int>(rotation) + offset) %
                                        4);
}

void ResetDisplayIdForTest() {
  next_synthesized_display_id = kSynthesizedDisplayIdStart;
}

int64_t GetNextSynthesizedDisplayId(int64_t id) {
  int next_output_index = id & 0xFF;
  next_output_index++;
  DCHECK_GT(0x100, next_output_index);
  int64_t base = GetDisplayIdWithoutOutputIndex(id);
  if (id == kSynthesizedDisplayIdStart)
    return id + 0x100 + next_output_index;
  return base + next_output_index;
}

}  // namespace display
