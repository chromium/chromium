// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager_utilities.h"

#include <algorithm>
#include <set>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

#if defined(OS_CHROMEOS)
#include "chromeos/system/statistics_provider.h"
#endif

namespace display {

ManagedDisplayInfo::ManagedDisplayModeList CreateInternalManagedDisplayModeList(
    const ManagedDisplayMode& native_mode) {
  ManagedDisplayMode mode(native_mode.size(), native_mode.refresh_rate(),
                          native_mode.is_interlaced(), true,
                          native_mode.device_scale_factor());
  return ManagedDisplayInfo::ManagedDisplayModeList{mode};
}

UnifiedDisplayModeParam::UnifiedDisplayModeParam(float dsf,
                                                 float scale,
                                                 bool is_default)
    : device_scale_factor(dsf),
      display_bounds_scale(scale),
      is_default_mode(is_default) {}

ManagedDisplayInfo::ManagedDisplayModeList CreateUnifiedManagedDisplayModeList(
    const ManagedDisplayMode& native_mode,
    const std::vector<UnifiedDisplayModeParam>& modes_param_list) {
  ManagedDisplayInfo::ManagedDisplayModeList display_mode_list;
  display_mode_list.reserve(modes_param_list.size());

  for (auto& param : modes_param_list) {
    gfx::SizeF scaled_size(native_mode.size());
    scaled_size.Scale(param.display_bounds_scale);
    display_mode_list.emplace_back(
        gfx::ToFlooredSize(scaled_size), native_mode.refresh_rate(),
        native_mode.is_interlaced(),
        param.is_default_mode ? true : false /* native */,
        param.device_scale_factor);
  }
  // Sort the mode by the size in DIP.
  std::sort(display_mode_list.begin(), display_mode_list.end(),
            [](const ManagedDisplayMode& a, const ManagedDisplayMode& b) {
              return a.GetSizeInDIP(false).GetArea() <
                     b.GetSizeInDIP(false).GetArea();
            });
  return display_mode_list;
}

bool ForceFirstDisplayInternal() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool ret = command_line->HasSwitch(::switches::kUseFirstDisplayAsInternal);
#if defined(OS_CHROMEOS)
  // Touch view mode is only available to internal display. We force the
  // display as internal for emulator to test touch view mode.
  ret = ret ||
        chromeos::system::StatisticsProvider::GetInstance()->IsRunningOnVm();
#endif
  return ret;
}

bool ComputeBoundary(const Display& a_display,
                     const Display& b_display,
                     gfx::Rect* a_edge_in_screen,
                     gfx::Rect* b_edge_in_screen) {
  const gfx::Rect& a_bounds = a_display.bounds();
  const gfx::Rect& b_bounds = b_display.bounds();

  // Find touching side.
  int rx = std::max(a_bounds.x(), b_bounds.x());
  int ry = std::max(a_bounds.y(), b_bounds.y());
  int rr = std::min(a_bounds.right(), b_bounds.right());
  int rb = std::min(a_bounds.bottom(), b_bounds.bottom());

  DisplayPlacement::Position position;
  if (rb == ry) {
    // top bottom
    if (rr <= rx) {
      // Top and bottom align, but no edges are shared.
      return false;
    }

    if (a_bounds.bottom() == b_bounds.y()) {
      position = DisplayPlacement::BOTTOM;
    } else if (a_bounds.y() == b_bounds.bottom()) {
      position = DisplayPlacement::TOP;
    } else {
      return false;
    }
  } else if (rr == rx) {
    // left right
    if (rb <= ry) {
      // Left and right align, but no edges are shared.
      return false;
    }

    if (a_bounds.right() == b_bounds.x()) {
      position = DisplayPlacement::RIGHT;
    } else if (a_bounds.x() == b_bounds.right()) {
      position = DisplayPlacement::LEFT;
    } else {
      return false;
    }
  } else {
    return false;
  }

  switch (position) {
    case DisplayPlacement::TOP:
    case DisplayPlacement::BOTTOM: {
      int left = std::max(a_bounds.x(), b_bounds.x());
      int right = std::min(a_bounds.right(), b_bounds.right());
      if (position == DisplayPlacement::TOP) {
        a_edge_in_screen->SetRect(left, a_bounds.y(), right - left, 1);
        b_edge_in_screen->SetRect(left, b_bounds.bottom() - 1, right - left, 1);
      } else {
        a_edge_in_screen->SetRect(left, a_bounds.bottom() - 1, right - left, 1);
        b_edge_in_screen->SetRect(left, b_bounds.y(), right - left, 1);
      }
      break;
    }
    case DisplayPlacement::LEFT:
    case DisplayPlacement::RIGHT: {
      int top = std::max(a_bounds.y(), b_bounds.y());
      int bottom = std::min(a_bounds.bottom(), b_bounds.bottom());
      if (position == DisplayPlacement::LEFT) {
        a_edge_in_screen->SetRect(a_bounds.x(), top, 1, bottom - top);
        b_edge_in_screen->SetRect(b_bounds.right() - 1, top, 1, bottom - top);
      } else {
        a_edge_in_screen->SetRect(a_bounds.right() - 1, top, 1, bottom - top);
        b_edge_in_screen->SetRect(b_bounds.x(), top, 1, bottom - top);
      }
      break;
    }
  }
  return true;
}

DisplayIdList CreateDisplayIdList(const Displays& list) {
  return GenerateDisplayIdList(
      list.begin(), list.end(),
      [](const Display& display) { return display.id(); });
}

void SortDisplayIdList(DisplayIdList* ids) {
  std::sort(ids->begin(), ids->end(),
            [](int64_t a, int64_t b) { return CompareDisplayIds(a, b); });
}

std::string DisplayIdListToString(const DisplayIdList& list) {
  std::stringstream s;
  const char* sep = "";
  for (int64_t id : list) {
    s << sep << id;
    sep = ",";
  }
  return s.str();
}

display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                              const gfx::Rect& bounds) {
  display::ManagedDisplayInfo info(id, "x-" + base::NumberToString(id), false);
  info.SetBounds(bounds);
  return info;
}

int64_t GetDisplayIdWithoutOutputIndex(int64_t id) {
  constexpr uint64_t kMask = ~static_cast<uint64_t>(0xFF);
  return static_cast<int64_t>(kMask & id);
}

MixedMirrorModeParams::MixedMirrorModeParams(int64_t src_id,
                                             const DisplayIdList& dst_ids)
    : source_id(src_id), destination_ids(dst_ids) {}

MixedMirrorModeParams::MixedMirrorModeParams(
    const MixedMirrorModeParams& mixed_params) = default;

MixedMirrorModeParams::~MixedMirrorModeParams() = default;

MixedMirrorModeParamsErrors ValidateParamsForMixedMirrorMode(
    const DisplayIdList& connected_display_ids,
    const MixedMirrorModeParams& mixed_params) {
  if (connected_display_ids.size() <= 1)
    return MixedMirrorModeParamsErrors::kErrorSingleDisplay;

  std::set<int64_t> all_display_ids;
  for (auto& id : connected_display_ids)
    all_display_ids.insert(id);
  if (!all_display_ids.count(mixed_params.source_id))
    return MixedMirrorModeParamsErrors::kErrorSourceIdNotFound;

  // This set is used to check duplicate id.
  std::set<int64_t> specified_display_ids;
  specified_display_ids.insert(mixed_params.source_id);

  if (mixed_params.destination_ids.empty())
    return MixedMirrorModeParamsErrors::kErrorDestinationIdsEmpty;

  for (auto& id : mixed_params.destination_ids) {
    if (!all_display_ids.count(id))
      return MixedMirrorModeParamsErrors::kErrorDestinationIdNotFound;
    if (!specified_display_ids.insert(id).second)
      return MixedMirrorModeParamsErrors::kErrorDuplicateId;
  }
  return MixedMirrorModeParamsErrors::kSuccess;
}

}  // namespace display
