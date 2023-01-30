// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/test/display_manager_test_api.h"

#include <cstdarg>
#include <vector>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/util/display_util.h"

namespace display {
namespace test {
namespace {

// Indicates the default maximum of displays that chrome device can support.
constexpr size_t kDefaultMaxSupportDisplayTest = 10;

DisplayInfoList CreateDisplayInfoListFromString(
    const std::string specs,
    DisplayManager* display_manager) {
  DisplayInfoList display_info_list;
  std::vector<std::string> parts = base::SplitString(
      specs, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  size_t index = 0;

  Displays list = display_manager->IsInUnifiedMode()
                      ? display_manager->software_mirroring_display_list()
                      : display_manager->active_display_list();

  for (std::vector<std::string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter, ++index) {
    int64_t id = (index < list.size()) ? list[index].id() : kInvalidDisplayId;
    display_info_list.push_back(
        ManagedDisplayInfo::CreateFromSpecWithID(*iter, id));
  }
  return display_info_list;
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

DisplayManagerTestApi::~DisplayManagerTestApi() {}

void DisplayManagerTestApi::ResetMaximumDisplay() {
  maximum_support_display_ = kDefaultMaxSupportDisplayTest;
}

void DisplayManagerTestApi::UpdateDisplay(const std::string& display_specs) {
  DisplayInfoList display_info_list =
      CreateDisplayInfoListFromString(display_specs, display_manager_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (display_info_list.size() > maximum_support_display_) {
    display_manager_->configurator()->has_unassociated_display_ = true;
    while (display_info_list.size() > maximum_support_display_)
      display_info_list.pop_back();
  } else {
    display_manager_->configurator()->has_unassociated_display_ = false;
  }
#endif
  bool is_host_origin_set = false;
  for (const ManagedDisplayInfo& display_info : display_info_list) {
    if (display_info.bounds_in_native().origin() != gfx::Point(0, 0)) {
      is_host_origin_set = true;
      break;
    }
  }

  // Start from (1,1) so that windows won't overlap with native mouse cursor.
  // See |AshTestBase::SetUp()|.
  int next_y = 1;
  for (auto& info : display_info_list) {
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

    // Overscan and native resolution are excluded for now as they require
    // special handing (has_overscan flag. resolution change makes sense
    // only on external).
    display_manager_->RegisterDisplayProperty(
        info.id(), info.GetRotation(Display::RotationSource::USER),
        /*overscan_insets=*/nullptr,
        /*native_resolution=*/gfx::Size(), info.device_scale_factor(),
        info.zoom_factor(), info.refresh_rate(), info.is_interlaced());
  }

  display_manager_->OnNativeDisplaysChanged(display_info_list);
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

  DCHECK(primary_display_iter != display_manager_->active_display_list_.end());

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
    id = display::GetNextSynthesizedDisplayId(id);
    list.push_back(id);
  }
  SortDisplayIdList(&list);
  DCHECK_EQ(N, list.size());
  return list;
}

}  // namespace test
}  // namespace display
