// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/test/display_manager_test_api.h"

#include <cstdarg>
#include <iterator>
#include <vector>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/util/display_manager_test_util.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_test_util.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace test {
namespace {

// Indicates the default maximum of displays that chrome device can support.
constexpr size_t kDefaultMaxSupportDisplayTest = 10;

DisplayInfoList CreateDisplayInfoListFromString(const std::string& specs,
                                                DisplayManager* display_manager,
                                                bool generate_new_ids) {
  Displays list = display_manager->IsInUnifiedMode()
                      ? display_manager->software_mirroring_display_list()
                      : display_manager->active_display_list();

  return CreateDisplayInfoListFromSpecs(specs, list, generate_new_ids);
}

// Gets the display |mode| for |resolution|. Returns false if no display
// mode matches the resolution, or the display is an internal display.
bool GetDisplayModeForResolution(const ManagedDisplayInfo& info,
                                 const gfx::Size& resolution,
                                 ManagedDisplayMode* mode) {
  if (IsInternalDisplayId(info.id()))
    return false;

  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  DCHECK_NE(0u, modes.size());
  auto iter = base::ranges::find(modes, resolution, &ManagedDisplayMode::size);
  if (iter == modes.end()) {
    DLOG(WARNING) << "Unsupported resolution was requested:"
                  << resolution.ToString();
    return false;
  }
  *mode = *iter;
  return true;
}

}  // namespace

size_t DisplayManagerTestApi::maximum_support_display_ =
    kDefaultMaxSupportDisplayTest;

DisplayManagerTestApi::DisplayManagerTestApi(DisplayManager* display_manager)
    : display_manager_(display_manager) {
  DCHECK(display_manager);
}

DisplayManagerTestApi::~DisplayManagerTestApi() = default;

void DisplayManagerTestApi::ResetMaximumDisplay() {
  maximum_support_display_ = kDefaultMaxSupportDisplayTest;
}

int64_t DisplayManagerTestApi::AddDisplay(const DisplayParams& display_params) {
  const Displays& current_displays = display_manager_->active_display_list();
  if (current_displays.size() >= maximum_support_display_) {
    LOG(ERROR) << "Display limit exceeded.";
    return kInvalidDisplayId;
  }
  int64_t new_display_id = GetASynthesizedDisplayId();
  std::vector<ManagedDisplayInfo> current_display_infos;
  for (const Display& display : current_displays) {
    ManagedDisplayInfo display_info =
        GetInternalManagedDisplayInfo(display.id());
    gfx::Rect bounds = display_info.bounds_in_native();
    // Reset the bounds so that UpdateDisplayWithDisplayInfoList automatically
    // arranges them.
    bounds.set_origin(gfx::Point());
    display_info.SetBounds(bounds);
    current_display_infos.push_back(display_info);
  }
  ManagedDisplayInfo new_display;
  new_display.set_display_id(new_display_id);
  new_display.SetBounds(gfx::Rect(display_params.resolution));
  ManagedDisplayInfo::ManagedDisplayModeList display_modes;
  display_modes.emplace_back(display_params.resolution, /*refresh_rate=*/60,
                             /*is_interlaced=*/false, /*native=*/true,
                             /*device_scale_factor=*/1);
  new_display.SetManagedDisplayModes(display_modes);
  current_display_infos.push_back(new_display);
  UpdateDisplayWithDisplayInfoList(current_display_infos,
                                   /*from_native_platform=*/false);
  return new_display.id();
}

void DisplayManagerTestApi::RemoveDisplay(int64_t display_id) {
  const Displays& active_displays = display_manager_->active_display_list();
  std::vector<ManagedDisplayInfo> desired_display_infos;
  for (const Display& display : active_displays) {
    if (display.id() == display_id) {
      continue;
    }
    desired_display_infos.push_back(
        GetInternalManagedDisplayInfo(display.id()));
  }
  if (desired_display_infos.size() == active_displays.size()) {
    LOG(ERROR) << "Display with ID " << display_id << " not found.";
    return;
  }

  UpdateDisplayWithDisplayInfoList(desired_display_infos,
                                   /*from_native_platform=*/false);
}

void DisplayManagerTestApi::ResetDisplays() {
  display_manager_->InitDefaultDisplay();
}

void DisplayManagerTestApi::UpdateDisplay(const std::string& display_specs,
                                          bool from_native_platform,
                                          bool generate_new_ids) {
  DisplayInfoList display_info_list = CreateDisplayInfoListFromString(
      display_specs, display_manager_, generate_new_ids);
  UpdateDisplayWithDisplayInfoList(display_info_list, from_native_platform);
}

void DisplayManagerTestApi::UpdateDisplayWithDisplayInfoList(
    const std::vector<ManagedDisplayInfo>& display_info_list,
    bool from_native_platform) {
  std::vector<ManagedDisplayInfo> display_list_copy = display_info_list;
  if (display_list_copy.size() > maximum_support_display_) {
    display_manager_->configurator()->has_unassociated_display_ = true;
    while (display_list_copy.size() > maximum_support_display_) {
      display_list_copy.pop_back();
    }
  } else {
    display_manager_->configurator()->has_unassociated_display_ = false;
  }
  bool is_host_origin_set = false;
  for (const ManagedDisplayInfo& display_info : display_list_copy) {
    if (display_info.bounds_in_native().origin() != gfx::Point(0, 0)) {
      is_host_origin_set = true;
      break;
    }
  }

  // Start from (1,1) so that windows won't overlap with native mouse cursor.
  // See |AshTestBase::SetUp()|.
  int next_y = 1;
  for (auto& info : display_list_copy) {
    // On non-testing environment, when a secondary display is connected, a new
    // native (i.e. X) window for the display is always created below the
    // previous one for GPU performance reasons. Try to emulate the behavior
    // unless host origins are explicitly set.
    if (!is_host_origin_set) {
      gfx::Rect bounds(info.bounds_in_native().size());
      bounds.set_x(1);
      bounds.set_y(next_y);
      next_y += bounds.height();
      info.SetBounds(bounds);
    }
    info.set_from_native_platform(from_native_platform);

    // Overscan and native resolution are excluded for now as they require
    // special handing (has_overscan flag. resolution change makes sense
    // only on external).
    if (!from_native_platform) {
      display_manager_->RegisterDisplayProperty(
          info.id(), info.GetRotation(Display::RotationSource::USER),
          /*overscan_insets=*/nullptr,
          /*resolution_in_pixels=*/gfx::Size(), info.device_scale_factor(),
          info.zoom_factor(), info.zoom_factor_map(), info.refresh_rate(),
          info.is_interlaced(), info.variable_refresh_rate_state(),
          info.vsync_rate_min());
    }
  }

  display_manager_->OnNativeDisplaysChanged(display_list_copy);
  display_manager_->UpdateInternalManagedDisplayModeListForTest();
  display_manager_->RunPendingTasksForTest();
}

int64_t DisplayManagerTestApi::SetFirstDisplayAsInternalDisplay() {
  const Display& internal = display_manager_->active_display_list_[0];
  SetInternalDisplayIds({internal.id()});
  return Display::InternalDisplayId();
}

void DisplayManagerTestApi::SetInternalDisplayId(int64_t id) {
  SetInternalDisplayIds({id});
  display_manager_->UpdateInternalManagedDisplayModeListForTest();
}

void DisplayManagerTestApi::DisableChangeDisplayUponHostResize() {
  display_manager_->set_change_display_upon_host_resize(false);
}

const ManagedDisplayInfo& DisplayManagerTestApi::GetInternalManagedDisplayInfo(
    int64_t display_id) {
  return display_manager_->display_info_[display_id];
}

void DisplayManagerTestApi::SetTouchSupport(
    int64_t display_id,
    Display::TouchSupport touch_support) {
  display_manager_->FindDisplayForId(display_id)
      ->set_touch_support(touch_support);
}

const Display& DisplayManagerTestApi::GetSecondaryDisplay() const {
  CHECK_GE(display_manager_->GetNumDisplays(), 2U);

  const int64_t primary_display_id =
      Screen::GetScreen()->GetPrimaryDisplay().id();

  auto primary_display_iter = base::ranges::find(
      display_manager_->active_display_list_, primary_display_id, &Display::id);

  CHECK(primary_display_iter != display_manager_->active_display_list_.end());

  ++primary_display_iter;

  // If we've reach the end of |active_display_list_|, wrap back around to the
  // front.
  if (primary_display_iter == display_manager_->active_display_list_.end())
    return *display_manager_->active_display_list_.begin();

  return *primary_display_iter;
}

ScopedSetInternalDisplayId::ScopedSetInternalDisplayId(
    DisplayManager* display_manager,
    int64_t id) {
  DisplayManagerTestApi(display_manager).SetInternalDisplayId(id);
}

ScopedSetInternalDisplayId::~ScopedSetInternalDisplayId() {
  SetInternalDisplayIds({});
}

bool SetDisplayResolution(DisplayManager* display_manager,
                          int64_t display_id,
                          const gfx::Size& resolution) {
  const ManagedDisplayInfo& info = display_manager->GetDisplayInfo(display_id);
  ManagedDisplayMode mode;
  if (!GetDisplayModeForResolution(info, resolution, &mode))
    return false;
  return display_manager->SetDisplayMode(display_id, mode);
}

std::unique_ptr<DisplayLayout> CreateDisplayLayout(
    DisplayManager* display_manager,
    DisplayPlacement::Position position,
    int offset) {
  DisplayLayoutBuilder builder(Screen::GetScreen()->GetPrimaryDisplay().id());
  builder.SetSecondaryPlacement(
      DisplayManagerTestApi(display_manager).GetSecondaryDisplay().id(),
      position, offset);
  return builder.Build();
}

DisplayIdList CreateDisplayIdList2(int64_t id1, int64_t id2) {
  DisplayIdList list;
  list.push_back(id1);
  list.push_back(id2);
  SortDisplayIdList(&list);
  return list;
}

DisplayIdList CreateDisplayIdListN(int64_t start_id, size_t count) {
  DisplayIdList list;
  list.push_back(start_id);
  int64_t id = start_id;
  size_t N = count;
  while (count-- > 1) {
    id = display::SynthesizeDisplayIdFromSeed(id);
    list.push_back(id);
  }
  SortDisplayIdList(&list);
  DCHECK_EQ(N, list.size());
  return list;
}

}  // namespace test
}  // namespace display
