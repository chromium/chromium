// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_properties_parser.h"

#include <optional>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace display {
namespace {

constexpr char kDisplayRadiiKeyName[] = "rounded-corners";
constexpr char kDisplayUpperLeftRadiusKeyName[] = "top-left";
constexpr char kDisplayUpperRightRadiusKeyName[] = "top-right";
constexpr char kDisplayLowerLeftRadiusKeyName[] = "bottom-left";
constexpr char kDisplayLowerRightRadiusKeyName[] = "bottom-right";

std::optional<gfx::RoundedCornersF> ParsePanelRadii(
    const base::Value* json_value) {
  if (!json_value || !json_value->is_dict()) {
    return std::nullopt;
  }

  const auto& radii_value = json_value->GetDict();
  if (radii_value.size() != 4u) {
    return std::nullopt;
  }

  gfx::RoundedCornersF panel_radii;
  for (const auto value : radii_value) {
    if (!value.second.is_int() || value.second.GetInt() < 0) {
      return std::nullopt;
    }

    const auto& key = value.first;
    const int radius = value.second.GetInt();

    if (key == kDisplayUpperLeftRadiusKeyName) {
      panel_radii.set_upper_left(radius);
    } else if (key == kDisplayUpperRightRadiusKeyName) {
      panel_radii.set_upper_right(radius);
    } else if (key == kDisplayLowerLeftRadiusKeyName) {
      panel_radii.set_lower_left(radius);
    } else if (key == kDisplayLowerRightRadiusKeyName) {
      panel_radii.set_lower_right(radius);
    }
  }

  return panel_radii;
}

}  // namespace

std::optional<gfx::RoundedCornersF> ParseDisplayPanelRadii(
    const base::Value* json_value) {
  if (!json_value->is_list()) {
    return std::nullopt;
  }
  const auto& display_infos = json_value->GetList();

  if (display_infos.size() > 1) {
    LOG(WARNING) << "Currently rounded-display property is only supported for "
                    "the internal display";
    return std::nullopt;
  }

  if (!display_infos.back().is_dict()) {
    return std::nullopt;
  }

  const auto& display_info = display_infos.back().GetDict();

  std::optional<gfx::RoundedCornersF> panel_radii =
      ParsePanelRadii(display_info.Find(kDisplayRadiiKeyName));

  if (!panel_radii.has_value()) {
    LOG(ERROR) << base::StringPrintf(
        "Invalid format of `%s` specified through display-properties "
        "switch",
        kDisplayRadiiKeyName);

    return std::nullopt;
  }

  return panel_radii;
}

}  // namespace display
