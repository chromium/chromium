// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager.h"

#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ash/components/system/devicemode.h"
#include "chromeos/ui/base/display_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager_util.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/strings/grit/ui_strings.h"

namespace display {

namespace {

// The number of pixels to overlap between the primary and secondary displays,
// in case that the offset value is too large.
const int kMinimumOverlapForInvalidOffset = 100;

// The UMA histogram that logs the types of mirror mode.
const char kMirrorModeTypesHistogram[] = "DisplayManager.MirrorModeTypes";

// The UMA histogram that logs the range in which the number of connected
// displays in mirror mode can reside.
const char kMirroringDisplayCountRangesHistogram[] =
    "DisplayManager.MirroringDisplayCountRanges";

// The UMA histogram that logs whether mirroring is done in hardware or
// software.
const char kMirroringImplementationHistogram[] =
    "DisplayManager.MirroringImplementation";

// The UMA historgram that logs the zoom percentage level of the intenral
// display.
constexpr char kInternalDisplayZoomPercentageHistogram[] =
    "DisplayManager.InternalDisplayZoomPercentage";

// Timeout in seconds after which we consider the change to the display zoom
// is not temporary.
constexpr int kDisplayZoomModifyTimeoutSec = 15;

struct DisplaySortFunctor {
  bool operator()(const Display& a, const Display& b) {
    return CompareDisplayIds(a.id(), b.id());
  }
};

struct DisplayInfoSortFunctor {
  bool operator()(const ManagedDisplayInfo& a, const ManagedDisplayInfo& b) {
    return CompareDisplayIds(a.id(), b.id());
  }
};

Display& GetInvalidDisplay() {
  static Display* invalid_display = new Display();
  return *invalid_display;
}

ManagedDisplayInfo::ManagedDisplayModeList::const_iterator FindDisplayMode(
    const ManagedDisplayInfo& info,
    const ManagedDisplayMode& target_mode) {
  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  return base::ranges::find_if(modes,
                               [target_mode](const ManagedDisplayMode& mode) {
                                 return target_mode.IsEquivalent(mode);
                               });
}

void SetInternalManagedDisplayModeList(ManagedDisplayInfo* info) {
  ManagedDisplayMode native_mode(info->bounds_in_native().size(),
                                 0.0 /* refresh_rate */, false /* interlaced */,
                                 true /* native_mode */,
                                 info->device_scale_factor());
  info->SetManagedDisplayModes(
      CreateInternalManagedDisplayModeList(native_mode));
}

void MaybeInitInternalDisplay(ManagedDisplayInfo* info) {
  int64_t id = info->id();
  if (ForceFirstDisplayInternal()) {
    display::SetInternalDisplayIds({id});
    SetInternalManagedDisplayModeList(info);
  }
}

gfx::Size GetMaxNativeSize(const ManagedDisplayInfo& info) {
  gfx::Size size;
  for (auto& mode : info.display_modes()) {
    if (mode.size().GetArea() > size.GetArea())
      size = mode.size();
  }
  return size;
}

bool ContainsDisplayWithId(const std::vector<Display>& displays,
                           int64_t display_id) {
  for (auto& display : displays) {
    if (display.id() == display_id)
      return true;
  }
  return false;
}

// Gets the next mode in |modes| in the direction marked by |up|. If trying to
// move past either end of |modes|, returns the same.
const ManagedDisplayMode* FindNextMode(
    const ManagedDisplayInfo::ManagedDisplayModeList& modes,
    size_t index,
    bool up) {
  DCHECK_LT(index, modes.size());
  size_t new_index = index;
  if (up && (index + 1 < modes.size()))
    ++new_index;
  else if (!up && index != 0)
    --new_index;
  return &modes[new_index];
}

// Gets the display |mode| for the next valid resolution. Returns false if the
// display is an internal display or if the DIP size cannot be found in |info|.
bool GetDisplayModeForNextResolution(const ManagedDisplayInfo& info,
                                     bool up,
                                     ManagedDisplayMode* mode) {
  DCHECK(!IsInternalDisplayId(info.id()));

  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  ManagedDisplayMode tmp(info.size_in_pixel(), 0.0, false, false,
                         info.device_scale_factor());
  const gfx::Size resolution = tmp.GetSizeInDIP();

  auto iter =
      base::ranges::find(modes, resolution, &ManagedDisplayMode::GetSizeInDIP);
  if (iter == modes.end())
    return false;
  *mode = *FindNextMode(modes, iter - modes.begin(), up);
  return true;
}

// Returns a pointer to the ManagedDisplayInfo of the display with |id|, nullptr
// if the corresponding info was not found.
const ManagedDisplayInfo* FindInfoById(const DisplayInfoList& display_info_list,
                                       int64_t id) {
  const auto iter =
      base::ranges::find(display_info_list, id, &ManagedDisplayInfo::id);

  if (iter == display_info_list.end())
    return nullptr;

  return &(*iter);
}

// Validates that:
// - All display IDs in the |matrix| are included in the |display_info_list|,
// - All IDs in |display_info_list| exist in the |matrix|,
// - All IDs in the matrix are unique (no repeated IDs).
bool ValidateMatrixForDisplayInfoList(
    const DisplayInfoList& display_info_list,
    const UnifiedDesktopLayoutMatrix& matrix) {
  std::set<int64_t> matrix_ids;
  for (const auto& row : matrix) {
    for (const auto& id : row) {
      if (!matrix_ids.emplace(id).second) {
        LOG(ERROR) << "Matrix has a repeated ID: " << id;
        return false;
      }

      if (!FindInfoById(display_info_list, id)) {
        LOG(ERROR) << "Matrix has ID: " << id << " with no corresponding info "
                   << "in the display info list.";
        return false;
      }
    }
  }

  for (const auto& info : display_info_list) {
    if (!matrix_ids.count(info.id())) {
      LOG(ERROR) << "Display info with ID: " << info.id() << " doesn't exist "
                 << "in the layout matrix.";
      return false;
    }
  }

  return true;
}

// Defines the ranges in which the number of displays can reside as reported by
// UMA in the case of Unified Desktop mode or mirror mode.
//
// WARNING: These values are persisted to logs. Entries should not be
//          renumbered and numeric values should never be reused.
enum class DisplayCountRange {
  // Exactly 2 displays.
  k2Displays = 0,
  // Range (2 : 4] displays.
  kUpTo4Displays = 1,
  // Range (4 : 6] displays.
  kUpTo6Displays = 2,
  // Range (6 : 8] displays.
  kUpTo8Displays = 3,
  // Greater than 8 displays.
  kGreaterThan8Displays = 4,

  // Always keep this the last item.
  kCount,
};

// Returns the display count range bucket in which |display_count| resides.
DisplayCountRange GetDisplayCountRange(int display_count) {
  // Note that Unified Mode and mirror mode cannot be enabled with a single
  // display.
  DCHECK_GE(display_count, 2);

  if (display_count <= 2)
    return DisplayCountRange::k2Displays;

  if (display_count <= 4)
    return DisplayCountRange::kUpTo4Displays;

  if (display_count <= 6)
    return DisplayCountRange::kUpTo6Displays;

  if (display_count <= 8)
    return DisplayCountRange::kUpTo8Displays;

  return DisplayCountRange::kGreaterThan8Displays;
}

// Describes the way mirror mode is implemented as reported by UMA.
//
// WARNING: These values are persisted to logs. Entries should not be renumbered
//          and numeric values should never be reused.
enum class MirroringImplementation {
  // Software mirroring, where the same content is rendered for each display
  // independently.
  kSoftware = 0,
  // Hardware mirroring, where a display is rendered once and shared across
  // multiple displays.
  kHardware = 1,

  kMaxValue = kHardware,
};

// Defines the types of mirror mode in which the displays connected to the
// device are in as reported by UMA.
//
// WARNING: These values are persisted to logs. Entries should not be renumbered
//          and numeric values should never be reused.
enum class MirrorModeTypes {
  // Normal mirror mode.
  kNormal = 0,
  // Mixed mirror mode.
  kMixed = 1,

  // Always keep this the last item.
  kCount,
};

void OnInternalDisplayZoomChanged(float zoom_factor) {
  constexpr static int kMaxValue = 300;
  constexpr static int kBucketSize = 5;
  constexpr static int kNumBuckets = kMaxValue / kBucketSize + 1;

  base::LinearHistogram::FactoryGet(
      kInternalDisplayZoomPercentageHistogram, kBucketSize, kMaxValue,
      kNumBuckets, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(std::round(zoom_factor * 100));
}

}  // namespace

DisplayManager::BeginEndNotifier::BeginEndNotifier(
    DisplayManager* display_manager)
    : display_manager_(display_manager) {
  if (display_manager_->notify_depth_++ == 0) {
    for (auto& observer : display_manager_->observers_)
      observer.OnWillProcessDisplayChanges();
  }
}

DisplayManager::BeginEndNotifier::~BeginEndNotifier() {
  if (--display_manager_->notify_depth_ == 0) {
    for (auto& observer : display_manager_->observers_)
      observer.OnDidProcessDisplayChanges();
  }
}

DisplayManager::DisplayManager(std::unique_ptr<Screen> screen)
    : screen_(std::move(screen)), layout_store_(new DisplayLayoutStore) {
  SetConfigureDisplays(chromeos::IsRunningAsSystemCompositor());
  change_display_upon_host_resize_ = !configure_displays_;
  unified_desktop_enabled_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableUnifiedDesktop);
  touch_device_manager_ = std::make_unique<TouchDeviceManager>();
}

DisplayManager::~DisplayManager() {
  // Reset the font params.
  gfx::SetFontRenderParamsDeviceScaleFactor(1.0f);
  on_display_zoom_modify_timeout_.Cancel();
}

void DisplayManager::SetConfigureDisplays(bool configure_displays) {
  configure_displays_ = configure_displays;
  if (display_configurator_) {
    display_configurator_->SetConfigureDisplays(configure_displays);
  }
}

bool DisplayManager::InitFromCommandLine() {
  DisplayInfoList info_list;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kHostWindowBounds))
    return false;
  const std::string size_str =
      command_line->GetSwitchValueASCII(::switches::kHostWindowBounds);
  for (const std::string& part : base::SplitString(
           size_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    info_list.push_back(ManagedDisplayInfo::CreateFromSpec(part));
    info_list.back().set_native(true);
    info_list.back().set_from_native_platform(true);
  }
  MaybeInitInternalDisplay(&info_list[0]);
  OnNativeDisplaysChanged(info_list);
  return true;
}

void DisplayManager::InitDefaultDisplay() {
  DisplayInfoList info_list;
  info_list.push_back(ManagedDisplayInfo::CreateFromSpec(std::string()));
  info_list.back().set_native(true);
  MaybeInitInternalDisplay(&info_list[0]);
  OnNativeDisplaysChanged(info_list);
}

void DisplayManager::UpdateInternalDisplay(
    const ManagedDisplayInfo& display_info) {
  DCHECK(HasInternalDisplay());
  InsertAndUpdateDisplayInfo(display_info);
}

void DisplayManager::RefreshFontParams() {
  gfx::SetFontRenderParamsDeviceScaleFactor(
      chromeos::GetRepresentativeDeviceScaleFactor(active_display_list_));
}

const DisplayLayout& DisplayManager::GetCurrentDisplayLayout() const {
  DCHECK_LE(2U, num_connected_displays());
  if (num_connected_displays() > 1) {
    DisplayIdList list = GetConnectedDisplayIdList();
    return layout_store_->GetRegisteredDisplayLayout(list);
  }
  DLOG(ERROR) << "DisplayLayout is requested for single display";
  // On release build, just fallback to default instead of blowing up.
  static base::NoDestructor<DisplayLayout> layout;
  layout->primary_id = active_display_list_[0].id();
  return *layout;
}

const DisplayLayout& DisplayManager::GetCurrentResolvedDisplayLayout() const {
  return current_resolved_layout_ ? *current_resolved_layout_
                                  : GetCurrentDisplayLayout();
}

DisplayIdList DisplayManager::GetConnectedDisplayIdList() const {
  return connected_display_id_list_;
}

bool DisplayManager::IsConnectedDisplayIdListInSyncWithCurrentState(
    const DisplayIdList& display_id_list) const {
#if DCHECK_IS_ON()
  DisplayIdList connected_display_id_list = display_id_list;
  if (IsInUnifiedMode()) {
    // A display for unified desktop is virtual.
    DCHECK_EQ(1u, display_id_list.size());
    DCHECK_EQ(display_id_list[0], kUnifiedDisplayId);
    connected_display_id_list.clear();
  }

  DisplayIdList software_mirroring_display_id_list =
      CreateDisplayIdList(software_mirroring_display_list_);
  connected_display_id_list.insert(connected_display_id_list.end(),
                                   software_mirroring_display_id_list.begin(),
                                   software_mirroring_display_id_list.end());
  connected_display_id_list.insert(connected_display_id_list.end(),
                                   hardware_mirroring_display_id_list_.begin(),
                                   hardware_mirroring_display_id_list_.end());
  SortDisplayIdList(&connected_display_id_list);
  return connected_display_id_list_ == connected_display_id_list;
#else
  return true;
#endif
}

void DisplayManager::SetLayoutForCurrentDisplays(
    std::unique_ptr<DisplayLayout> layout) {
  if (GetNumDisplays() == 1)
    return;
  BeginEndNotifier notifier(this);

  const DisplayIdList list = GetConnectedDisplayIdList();

  DCHECK(DisplayLayout::Validate(list, *layout));

  const DisplayLayout& current_layout =
      layout_store_->GetRegisteredDisplayLayout(list);

  if (layout->HasSamePlacementList(current_layout))
    return;

  layout_store_->RegisterLayoutForDisplayIdList(list, std::move(layout));
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);

  // TODO(oshima): Call UpdateDisplays instead.
  std::vector<int64_t> updated_ids;
  current_resolved_layout_ = GetCurrentDisplayLayout().Copy();
  ApplyDisplayLayout(current_resolved_layout_.get(), &active_display_list_,
                     &updated_ids);
  for (int64_t id : updated_ids) {
    NotifyMetricsChanged(GetDisplayForId(id),
                         DisplayObserver::DISPLAY_METRIC_BOUNDS |
                             DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  }

  if (delegate_)
    delegate_->PostDisplayConfigurationChange();
}

const Display& DisplayManager::GetDisplayForId(int64_t display_id) const {
  auto* display =
      const_cast<DisplayManager*>(this)->FindDisplayForId(display_id);
  // TODO(oshima): This happens when windows in unified desktop have
  // been moved to a normal window. Fix this.
  if (!display && display_id != kUnifiedDisplayId)
    DLOG(ERROR) << "Could not find display:" << display_id;
  return display ? *display : GetInvalidDisplay();
}

bool DisplayManager::IsDisplayIdValid(int64_t display_id) const {
  Display* display =
      const_cast<DisplayManager*>(this)->FindDisplayForId(display_id);
  return !!display;
}

const Display& DisplayManager::FindDisplayContainingPoint(
    const gfx::Point& point_in_screen) const {
  const Displays& active_only_displays = active_only_display_list();
  auto iter = display::FindDisplayContainingPoint(active_only_displays,
                                                  point_in_screen);
  return iter == active_only_displays.end() ? GetInvalidDisplay() : *iter;
}

bool DisplayManager::UpdateWorkAreaOfDisplay(int64_t display_id,
                                             const gfx::Insets& insets) {
  BeginEndNotifier notifier(this);
  Display* display = FindDisplayForId(display_id);
  DCHECK(display);
  gfx::Rect old_work_area = display->work_area();
  display->UpdateWorkAreaFromInsets(insets);
  bool workarea_changed = old_work_area != display->work_area();
  if (workarea_changed)
    NotifyMetricsChanged(*display, DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  return workarea_changed;
}

void DisplayManager::SetOverscanInsets(int64_t display_id,
                                       const gfx::Insets& insets_in_dip) {
  bool update = false;
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      if (insets_in_dip.IsEmpty()) {
        info.set_clear_overscan_insets(true);
      } else {
        info.set_clear_overscan_insets(false);
        info.SetOverscanInsets(insets_in_dip);
      }
      update = true;
    }
    display_info_list.push_back(info);
  }
  if (update) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  } else {
    display_info_[display_id].SetOverscanInsets(insets_in_dip);
  }
}

void DisplayManager::SetDisplayRotation(int64_t display_id,
                                        Display::Rotation rotation,
                                        Display::RotationSource source) {
  if (IsInUnifiedMode() && display_id == kUnifiedDisplayId)
    return;

  DisplayInfoList display_info_list;
  bool is_active = false;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      if (info.GetRotation(source) == rotation &&
          info.GetActiveRotation() == rotation) {
        return;
      }
      info.SetRotation(rotation, source);
      is_active = true;
    }
    display_info_list.push_back(info);
  }
  if (is_active) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  } else if (display_info_.find(display_id) != display_info_.end()) {
    // Inactive displays can reactivate, ensure they have been updated.
    display_info_[display_id].SetRotation(rotation, source);
  }
}

bool DisplayManager::SetDisplayMode(int64_t display_id,
                                    const ManagedDisplayMode& display_mode) {
  DisplayInfoList display_info_list;
  bool display_property_changed = false;
  bool resolution_changed = false;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      auto iter = FindDisplayMode(info, display_mode);
      if (iter == info.display_modes().end()) {
        DLOG(WARNING) << "Unsupported display mode was requested:"
                      << "size=" << display_mode.size().ToString()
                      << ", scale factor="
                      << display_mode.device_scale_factor();
        return false;
      }

      display_modes_[display_id] = *iter;
      if (info.bounds_in_native().size() != display_mode.size()) {
        // If resolution changes, then we can break right here. No need to
        // continue to fill |display_info_list|, since we won't be
        // synchronously updating the displays here.
        resolution_changed = true;

        // Different resolutions allow different zoom factors to be set in the
        // UI. To avoid confusion in the UI, reset the zoom factor to 1.0.
        display_info_[display_id].set_zoom_factor(1.f);
        break;
      }
      if (info.device_scale_factor() != display_mode.device_scale_factor()) {
        info.set_device_scale_factor(display_mode.device_scale_factor());
        display_property_changed = true;
      }

      if (features::IsListAllDisplayModesEnabled()) {
        if (info.refresh_rate() != display_mode.refresh_rate()) {
          info.set_refresh_rate(display_mode.refresh_rate());
          resolution_changed = true;
        }
        if (info.is_interlaced() != display_mode.is_interlaced()) {
          info.set_is_interlaced(display_mode.is_interlaced());
          resolution_changed = true;
        }
      }
    }
    display_info_list.emplace_back(info);
  }

  if (display_property_changed && !resolution_changed) {
    // We shouldn't synchronously update the displays here if the resolution
    // changed. This should happen asynchronously when configuration is
    // triggered.
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  }

  if (resolution_changed && IsInUnifiedMode())
    ReconfigureDisplays();
  else if (resolution_changed && configure_displays_)
    display_configurator_->OnConfigurationChanged();

  return resolution_changed || display_property_changed;
}

void DisplayManager::RegisterDisplayProperty(
    int64_t display_id,
    Display::Rotation rotation,
    const gfx::Insets* overscan_insets,
    const gfx::Size& resolution_in_pixels,
    float device_scale_factor,
    float display_zoom_factor,
    float refresh_rate,
    bool is_interlaced) {
  if (display_info_.find(display_id) == display_info_.end())
    display_info_[display_id] =
        ManagedDisplayInfo(display_id, std::string(), false);

  // Do not allow rotation in unified desktop mode.
  if (display_id == kUnifiedDisplayId)
    rotation = Display::ROTATE_0;

  ManagedDisplayInfo& info = display_info_[display_id];
  info.SetRotation(rotation, Display::RotationSource::USER);
  info.SetRotation(rotation, Display::RotationSource::ACTIVE);

  info.set_zoom_factor(display_zoom_factor);

  if (overscan_insets)
    info.SetOverscanInsets(*overscan_insets);

  info.set_refresh_rate(refresh_rate);
  info.set_is_interlaced(is_interlaced);

  if (!resolution_in_pixels.IsEmpty()) {
    DCHECK(!IsInternalDisplayId(display_id));
    ManagedDisplayMode mode(resolution_in_pixels, refresh_rate, is_interlaced,
                            false, device_scale_factor);
    display_modes_[display_id] = mode;
  }
}

bool DisplayManager::GetActiveModeForDisplayId(int64_t display_id,
                                               ManagedDisplayMode* mode) const {
  ManagedDisplayMode selected_mode;
  if (GetSelectedModeForDisplayId(display_id, &selected_mode)) {
    *mode = selected_mode;
    return true;
  }

  // If 'selected' mode is empty, it should return the default mode. This means
  // the native mode for the external display, and the first one for internal.
  const ManagedDisplayInfo& info = GetDisplayInfo(display_id);
  const ManagedDisplayInfo::ManagedDisplayModeList& display_modes =
      info.display_modes();

  for (const auto& display_mode : display_modes) {
    if (display::IsInternalDisplayId(display_id)) {
      if (display_modes.size() == 1) {
        *mode = display_mode;
        return true;
      }
    } else if (display_mode.native()) {
      *mode = display_mode;
      return true;
    }
  }
  return false;
}

void DisplayManager::RegisterDisplayRotationProperties(
    bool rotation_lock,
    Display::Rotation rotation) {
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);
  registered_internal_display_rotation_lock_ = rotation_lock;
  registered_internal_display_rotation_ = rotation;
  if (delegate_)
    delegate_->PostDisplayConfigurationChange();
}

bool DisplayManager::GetSelectedModeForDisplayId(
    int64_t display_id,
    ManagedDisplayMode* mode) const {
  auto iter = display_modes_.find(display_id);
  if (iter == display_modes_.end())
    return false;
  *mode = iter->second;
  return true;
}

void DisplayManager::SetSelectedModeForDisplayId(
    int64_t display_id,
    const ManagedDisplayMode& display_mode) {
  ManagedDisplayInfo info = GetDisplayInfo(display_id);
  auto iter = FindDisplayMode(info, display_mode);
  if (iter == info.display_modes().end()) {
    DLOG(WARNING) << "Unsupported display mode was requested:"
                  << "size=" << display_mode.size().ToString()
                  << ", scale factor=" << display_mode.device_scale_factor();
  }

  display_modes_[display_id] = *iter;
}

gfx::Insets DisplayManager::GetOverscanInsets(int64_t display_id) const {
  auto it = display_info_.find(display_id);
  return (it != display_info_.end()) ? it->second.overscan_insets_in_dip()
                                     : gfx::Insets();
}

void DisplayManager::OnNativeDisplaysChanged(
    const DisplayInfoList& updated_displays) {
  if (updated_displays.empty()) {
    VLOG(1) << __func__
            << "(0): # of current displays=" << active_display_list_.size();
    // If the device is booted without display, or chrome is started
    // without --ash-host-window-bounds on linux desktop, use the
    // default display.
    if (active_display_list_.empty()) {
      DisplayInfoList init_displays;
      init_displays.push_back(
          ManagedDisplayInfo::CreateFromSpec(std::string()));
      MaybeInitInternalDisplay(&init_displays[0]);
      OnNativeDisplaysChanged(init_displays);
    } else {
      // Otherwise don't update the displays when all displays are disconnected.
      // This happens when:
      // - the device is idle and powerd requested to turn off all displays.
      // - the device is suspended. (kernel turns off all displays)
      // - the internal display's brightness is set to 0 and no external
      //   display is connected.
      // - the internal display's brightness is 0 and external display is
      //   disconnected.
      // The display will be updated when one of displays is turned on, and the
      // display list will be updated correctly.
    }
    return;
  }
  VLOG_IF(1, updated_displays.size() == 1)
      << __func__ << "(1):" << updated_displays[0].ToString();
  VLOG_IF(1, updated_displays.size() > 1)
      << __func__ << "(" << updated_displays.size()
      << ") [0]=" << updated_displays[0].ToString()
      << ", [1]=" << updated_displays[1].ToString();

  first_display_id_ = updated_displays[0].id();
  std::map<gfx::Point, int64_t> origins;

  bool internal_display_connected = false;
  DisplayIdList hardware_mirroring_display_id_list;
  int64_t mirroring_source_id = kInvalidDisplayId;
  DisplayInfoList new_display_info_list;
  for (const auto& display_info : updated_displays) {
    if (!internal_display_connected)
      internal_display_connected = IsInternalDisplayId(display_info.id());
    // Mirrored monitors have the same origins.
    gfx::Point origin = display_info.bounds_in_native().origin();
    const auto iter = origins.find(origin);
    if (iter != origins.end()) {
      InsertAndUpdateDisplayInfo(display_info);
      if (hardware_mirroring_display_id_list.empty()) {
        // Unlike software mirroring, hardware mirroring has no source and
        // target. All mirroring displays scan the same frame buffer. But for
        // convenience, we treat the first mirroring display as source.
        mirroring_source_id = iter->second;
      }
      // Only keep the first hardware mirroring display in
      // |new_display_info_list| because hardware mirroring is not visible for
      // display manager and all hardware mirroring displays should be treated
      // as one single display from this point.
      hardware_mirroring_display_id_list.emplace_back(display_info.id());
    } else {
      origins.emplace(origin, display_info.id());
      new_display_info_list.emplace_back(display_info);
    }

    ManagedDisplayMode new_mode(
        display_info.bounds_in_native().size(), display_info.refresh_rate(),
        display_info.is_interlaced(), display_info.native(),
        display_info.device_scale_factor());

    const ManagedDisplayInfo::ManagedDisplayModeList& display_modes =
        display_info.display_modes();
    // This is empty the displays are initialized from InitFromCommandLine.
    if (display_modes.empty())
      continue;
    auto display_modes_iter = FindDisplayMode(display_info, new_mode);
    // Update the actual resolution selected as the resolution request may fail.
    if (display_modes_iter == display_modes.end())
      display_modes_.erase(display_info.id());
    else if (display_modes_.find(display_info.id()) != display_modes_.end())
      display_modes_[display_info.id()] = *display_modes_iter;
  }
  if (HasInternalDisplay() && !internal_display_connected) {
    if (display_info_.find(Display::InternalDisplayId()) ==
        display_info_.end()) {
      // Create a dummy internal display if the chrome restarted
      // in docked mode.
      ManagedDisplayInfo internal_display_info(
          Display::InternalDisplayId(),
          l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_INTERNAL),
          false /*Internal display must not have overscan */);
      internal_display_info.SetBounds(gfx::Rect(0, 0, 800, 600));
      display_info_[Display::InternalDisplayId()] = internal_display_info;
    } else {
      // Internal display is no longer active. Reset its rotation to user
      // preference, so that it is restored when the internal display becomes
      // active again.
      Display::Rotation user_rotation =
          display_info_[Display::InternalDisplayId()].GetRotation(
              Display::RotationSource::USER);
      display_info_[Display::InternalDisplayId()].SetRotation(
          user_rotation, Display::RotationSource::USER);
    }
  }

  if (!configure_displays_ && new_display_info_list.size() > 1 &&
      hardware_mirroring_display_id_list.empty()) {
    DisplayIdList list = CreateDisplayIdList(new_display_info_list);
    // Mirror mode is set by DisplayConfigurator on the device. Emulate it when
    // running on linux desktop.  Carry over HW mirroring state only in unified
    // desktop so that it can switch to software mirroring to avoid exiting
    // unified desktop.
    // Note that this is only for testing.
    bool should_enable_software_mirroring =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kEnableSoftwareMirroring) ||
        ShouldSetMirrorModeOn(list, unified_desktop_enabled_);
    SetSoftwareMirroring(should_enable_software_mirroring);
  }

  // Do not clear current mirror state before calling ShouldSetMirrorModeOn()
  // as it depends on the state.
  ClearMirroringSourceAndDestination();
  hardware_mirroring_display_id_list_ = hardware_mirroring_display_id_list;
  mirroring_source_id_ = mirroring_source_id;
  connected_display_id_list_ = CreateDisplayIdList(updated_displays);

  UpdateDisplaysWith(new_display_info_list);
}

void DisplayManager::UpdateDisplays() {
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_)
    display_info_list.push_back(GetDisplayInfo(display.id()));
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplaysWith(display_info_list);
}

void DisplayManager::UpdateDisplaysWith(
    const DisplayInfoList& updated_display_info_list) {
  BeginEndNotifier notifier(this);

  DisplayInfoList new_display_info_list = updated_display_info_list;
  std::sort(active_display_list_.begin(), active_display_list_.end(),
            DisplaySortFunctor());
  std::sort(new_display_info_list.begin(), new_display_info_list.end(),
            DisplayInfoSortFunctor());

  DisplayIdList new_display_id_list =
      CreateDisplayIdList(new_display_info_list);

  if (num_connected_displays() > 1) {
    DisplayIdList connected_display_id_list = GetConnectedDisplayIdList();
    DCHECK(IsConnectedDisplayIdListInSyncWithCurrentState(new_display_id_list));
    const DisplayLayout& layout =
        layout_store_->GetOrCreateRegisteredDisplayLayout(
            connected_display_id_list);
    current_default_multi_display_mode_ =
        (layout.default_unified && unified_desktop_enabled_) ? UNIFIED
                                                             : EXTENDED;
  }

  if (multi_display_mode_ != MIRRORING ||
      (mixed_mirror_mode_params_ &&
       ValidateParamsForMixedMirrorMode(new_display_id_list,
                                        *mixed_mirror_mode_params_) !=
           MixedMirrorModeParamsErrors::kSuccess)) {
    // Set default display mode if mixed mirror mode is requested but the
    // request is invalid. (e.g, This may happen when a mirroring source or
    // destination display is removed.)
    multi_display_mode_ = current_default_multi_display_mode_;
  }

  UMA_HISTOGRAM_ENUMERATION("DisplayManager.MultiDisplayMode",
                            multi_display_mode_, MULTI_DISPLAY_MODE_LAST + 1);

  CreateSoftwareMirroringDisplayInfo(&new_display_info_list);

  // Close the mirroring window if any here to avoid creating two compositor on
  // one display.
  if (delegate_)
    delegate_->CloseMirroringDisplayIfNotNecessary();

  Displays new_displays;
  Displays removed_displays;
  std::map<size_t, uint32_t> display_changes;
  std::vector<size_t> added_display_indices;

  auto curr_iter = active_display_list_.begin();
  DisplayInfoList::const_iterator new_info_iter = new_display_info_list.begin();

  while (curr_iter != active_display_list_.end() ||
         new_info_iter != new_display_info_list.end()) {
    if (curr_iter == active_display_list_.end()) {
      // more displays in new list.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    } else if (new_info_iter == new_display_info_list.end()) {
      // more displays in current list.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else if (curr_iter->id() == new_info_iter->id()) {
      const Display& current_display = *curr_iter;
      // Copy the info because |InsertAndUpdateDisplayInfo| updates the
      // instance.
      const ManagedDisplayInfo current_display_info =
          GetDisplayInfo(current_display.id());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      Display new_display =
          CreateDisplayFromDisplayInfoById(new_info_iter->id());
      const ManagedDisplayInfo& new_display_info =
          GetDisplayInfo(new_display.id());

      uint32_t metrics = DisplayObserver::DISPLAY_METRIC_NONE;

      // At that point the new Display objects we have are not entirely updated,
      // they are missing the translation related to the Display disposition in
      // the layout.
      // Using display.bounds() and display.work_area() would fail most of the
      // time.
      if (force_bounds_changed_ ||
          (current_display_info.bounds_in_native() !=
           new_display_info.bounds_in_native()) ||
          (current_display_info.GetOverscanInsetsInPixel() !=
           new_display_info.GetOverscanInsetsInPixel()) ||
          current_display.size() != new_display.size()) {
        metrics |= DisplayObserver::DISPLAY_METRIC_BOUNDS |
                   DisplayObserver::DISPLAY_METRIC_WORK_AREA;
      }

      if (current_display.device_scale_factor() !=
          new_display.device_scale_factor()) {
        metrics |= DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
      }

      if (current_display.rotation() != new_display.rotation())
        metrics |= DisplayObserver::DISPLAY_METRIC_ROTATION;

      if (!WithinEpsilon(current_display.display_frequency(),
                         new_display.display_frequency())) {
        metrics |= DisplayObserver::DISPLAY_METRIC_REFRESH_RATE;
      }

      if (current_display_info.is_interlaced() !=
          new_display_info.is_interlaced()) {
        metrics |= DisplayObserver::DISPLAY_METRIC_INTERLACED;
      }

      if (current_display.label() != new_display.label())
        metrics |= DisplayObserver::DISPLAY_METRIC_LABEL;

      if (metrics != DisplayObserver::DISPLAY_METRIC_NONE) {
        display_changes.insert(
            std::pair<size_t, uint32_t>(new_displays.size(), metrics));
      }

      new_display.UpdateWorkAreaFromInsets(current_display.GetWorkAreaInsets());
      new_displays.push_back(new_display);
      ++curr_iter;
      ++new_info_iter;
    } else if (CompareDisplayIds(curr_iter->id(), new_info_iter->id())) {
      // more displays in current list between ids, which means it is deleted.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else {
      // more displays in new list between ids, which means it is added.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    }
  }

  Display old_primary;
  if (delegate_) {
    // Get old primary from current resolved layout, because we could be in the
    // middle of updating the primary display, so screen_->GetPrimaryDisplay()
    // may already point to the new primary.
    if (current_resolved_layout_) {
      Display* primary = FindDisplayForId(current_resolved_layout_->primary_id);
      if (primary)
        old_primary = *primary;
    }
    if (!old_primary.is_valid())
      old_primary = screen_->GetPrimaryDisplay();
  }

  // Clear focus if the display has been removed, but don't clear focus if
  // the destkop has been moved from one display to another
  // (mirror -> docked, docked -> single internal).
  bool clear_focus =
      !removed_displays.empty() &&
      !(removed_displays.size() == 1 && added_display_indices.size() == 1);
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(clear_focus);

  std::vector<size_t> updated_indices;
  UpdateNonPrimaryDisplayBoundsForLayout(&new_displays, &updated_indices);
  for (size_t updated_index : updated_indices) {
    if (!base::Contains(added_display_indices, updated_index)) {
      uint32_t metrics = DisplayObserver::DISPLAY_METRIC_BOUNDS |
                         DisplayObserver::DISPLAY_METRIC_WORK_AREA;
      if (display_changes.find(updated_index) != display_changes.end())
        metrics |= display_changes[updated_index];

      display_changes[updated_index] = metrics;
    }
  }

  active_display_list_ = new_displays;
  active_only_display_list_ = active_display_list_;

  RefreshFontParams();
  base::AutoReset<bool> resetter(&change_display_upon_host_resize_, false);

  size_t active_display_list_size = active_display_list_.size();
  is_updating_display_list_ = true;
  // Temporarily add displays to be removed because display object
  // being removed are accessed during shutting down the root.
  active_display_list_.insert(active_display_list_.end(),
                              removed_displays.begin(), removed_displays.end());

  for (const auto& display : removed_displays)
    NotifyDisplayRemoved(display);

  for (size_t index : added_display_indices)
    NotifyDisplayAdded(active_display_list_[index]);

  active_display_list_.resize(active_display_list_size);
  is_updating_display_list_ = false;

  // OnDidRemoveDisplays is called after the displays have been removed,
  // in comparison to NotifyDisplayRemoved/OnDisplayRemoved which on Ash
  // is called before.
  if (!removed_displays.empty()) {
    for (auto& observer : observers_)
      observer.OnDidRemoveDisplays();
  }

  UpdatePrimaryDisplayIdIfNecessary();

  bool notify_primary_change =
      delegate_ ? old_primary.id() != screen_->GetPrimaryDisplay().id() : false;

  for (auto iter = display_changes.begin(); iter != display_changes.end();
       ++iter) {
    uint32_t metrics = iter->second;
    const Display& updated_display = active_display_list_[iter->first];

    if (notify_primary_change &&
        updated_display.id() == screen_->GetPrimaryDisplay().id()) {
      metrics |= DisplayObserver::DISPLAY_METRIC_PRIMARY;
      notify_primary_change = false;
    }
    NotifyMetricsChanged(updated_display, metrics);
  }

  uint32_t primary_metrics = 0;

  if (notify_primary_change) {
    // This happens when a primary display has moved to anther display without
    // bounds change.
    const Display& primary = screen_->GetPrimaryDisplay();
    if (primary.id() != old_primary.id()) {
      primary_metrics = DisplayObserver::DISPLAY_METRIC_PRIMARY;
      if (primary.size() != old_primary.size()) {
        primary_metrics |= (DisplayObserver::DISPLAY_METRIC_BOUNDS |
                            DisplayObserver::DISPLAY_METRIC_WORK_AREA);
      }
      if (primary.device_scale_factor() != old_primary.device_scale_factor())
        primary_metrics |= DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
    }
  }

  bool mirror_mode = IsInMirrorMode();
  if (mirror_mode != mirror_mode_for_metrics_) {
    primary_metrics |= DisplayObserver::DISPLAY_METRIC_MIRROR_STATE;
    mirror_mode_for_metrics_ = mirror_mode;
  }

  if (delegate_ && primary_metrics)
    NotifyMetricsChanged(screen_->GetPrimaryDisplay(), primary_metrics);

  UpdateInfoForRestoringMirrorMode();

  if (delegate_)
    delegate_->PostDisplayConfigurationChange();

  if (mirror_mode) {
    UMA_HISTOGRAM_ENUMERATION(
        kMirroringDisplayCountRangesHistogram,
        GetDisplayCountRange(GetMirroringDestinationDisplayIdList().size() + 1),
        DisplayCountRange::kCount);
    UMA_HISTOGRAM_ENUMERATION(kMirroringImplementationHistogram,
                              IsInSoftwareMirrorMode()
                                  ? MirroringImplementation::kSoftware
                                  : MirroringImplementation::kHardware);
    UMA_HISTOGRAM_ENUMERATION(kMirrorModeTypesHistogram,
                              mixed_mirror_mode_params_
                                  ? MirrorModeTypes::kMixed
                                  : MirrorModeTypes::kNormal,
                              MirrorModeTypes::kCount);
  }

  // Create the mirroring window asynchronously after all displays
  // are added so that it can mirror the display newly added. This can
  // happen when switching from dock mode to software mirror mode.
  CreateMirrorWindowAsyncIfAny();
}

const Display& DisplayManager::GetDisplayAt(size_t index) const {
  DCHECK_LT(index, active_display_list_.size());
  return active_display_list_[index];
}

const Display& DisplayManager::GetPrimaryDisplayCandidate() const {
  if (GetNumDisplays() != 2)
    return active_display_list_[0];
  const DisplayLayout& layout =
      layout_store_->GetRegisteredDisplayLayout(GetConnectedDisplayIdList());
  return GetDisplayForId(layout.primary_id);
}

// static
const Display& DisplayManager::GetFakePrimaryDisplay() {
  static Display* fake_display = nullptr;
  if (!fake_display) {
    fake_display = new Display(Display::GetDefaultDisplay());
    // Note that if an inappropriate gfx::BufferFormat is specified in the
    // gfx::DisplayColorSpaces of the fake display, this can sometimes
    // propagate to allocation code and cause errors.
    // https://crbug.com/1057501
    gfx::DisplayColorSpaces display_color_spaces(
        gfx::ColorSpace::CreateSRGB(), DisplaySnapshot::PrimaryFormat());
    fake_display->set_color_spaces(display_color_spaces);
  }
  return *fake_display;
}

size_t DisplayManager::GetNumDisplays() const {
  return active_display_list_.size();
}

bool DisplayManager::IsActiveDisplayId(int64_t display_id) const {
  return ContainsDisplayWithId(active_display_list_, display_id);
}

bool DisplayManager::IsInMirrorMode() const {
  // Either software or hardware mirror mode can be active at the same time.
  DCHECK(!IsInSoftwareMirrorMode() || !IsInHardwareMirrorMode());
  return IsInSoftwareMirrorMode() || IsInHardwareMirrorMode();
}

bool DisplayManager::IsInSoftwareMirrorMode() const {
  if (multi_display_mode_ != MIRRORING ||
      software_mirroring_display_list_.empty()) {
    return false;
  }

  // Software mirroring cannot coexist with hardware mirroring.
  DCHECK(hardware_mirroring_display_id_list_.empty());
  return true;
}

bool DisplayManager::IsInHardwareMirrorMode() const {
  if (hardware_mirroring_display_id_list_.empty())
    return false;

  // Hardware mirroring is not visible to the display manager, the display mode
  // should be EXTENDED.
  DCHECK(multi_display_mode_ == EXTENDED);

  // Hardware mirroring cannot coexist with software mirroring.
  DCHECK(software_mirroring_display_list_.empty());
  return true;
}

DisplayIdList DisplayManager::GetMirroringDestinationDisplayIdList() const {
  if (IsInSoftwareMirrorMode())
    return CreateDisplayIdList(software_mirroring_display_list_);
  if (IsInHardwareMirrorMode())
    return hardware_mirroring_display_id_list_;
  return DisplayIdList();
}

void DisplayManager::ClearMirroringSourceAndDestination() {
  mirroring_source_id_ = kInvalidDisplayId;
  hardware_mirroring_display_id_list_.clear();
  software_mirroring_display_list_.clear();
}

void DisplayManager::SetUnifiedDesktopEnabled(bool enable) {
  unified_desktop_enabled_ = enable;
  // There is no need to update the displays in mirror mode. Doing
  // this in hardware mirroring mode can cause crash because display
  // info in hardware mirroring comes from DisplayConfigurator.
  if (!IsInMirrorMode())
    ReconfigureDisplays();
}

bool DisplayManager::IsInUnifiedMode() const {
  return multi_display_mode_ == UNIFIED &&
         !software_mirroring_display_list_.empty();
}

void DisplayManager::SetUnifiedDesktopMatrix(
    const UnifiedDesktopLayoutMatrix& matrix) {
  current_unified_desktop_matrix_ = matrix;
  SetDefaultMultiDisplayModeForCurrentDisplays(UNIFIED);
}

Display DisplayManager::GetMirroringDisplayForUnifiedDesktop(
    DisplayPositionInUnifiedMatrix cell_position) const {
  if (!IsInUnifiedMode())
    return Display();

  DCHECK(!current_unified_desktop_matrix_.empty());

  const size_t rows = current_unified_desktop_matrix_.size();
  const size_t columns = current_unified_desktop_matrix_[0].size();

  int64_t display_id = kInvalidDisplayId;
  switch (cell_position) {
    case DisplayPositionInUnifiedMatrix::kTopLeft:
      display_id = current_unified_desktop_matrix_[0][0];
      break;

    case DisplayPositionInUnifiedMatrix::kTopRight:
      display_id = current_unified_desktop_matrix_[0][columns - 1];
      break;

    case DisplayPositionInUnifiedMatrix::kBottomLeft:
      display_id = current_unified_desktop_matrix_[rows - 1][0];
      break;
  }

  DCHECK_NE(display_id, kInvalidDisplayId);

  for (auto& display : software_mirroring_display_list_) {
    if (display.id() == display_id)
      return display;
  }

  NOTREACHED();
  return Display();
}

int DisplayManager::GetMirroringDisplayRowIndexInUnifiedMatrix(
    int64_t display_id) const {
  DCHECK(IsInUnifiedMode());

  return mirroring_display_id_to_unified_matrix_row_.at(display_id);
}

int DisplayManager::GetUnifiedDesktopRowMaxHeight(int row_index) const {
  DCHECK(IsInUnifiedMode());

  return unified_display_rows_heights_.at(row_index);
}

const ManagedDisplayInfo& DisplayManager::GetDisplayInfo(
    int64_t display_id) const {
  DCHECK_NE(kInvalidDisplayId, display_id);

  auto iter = display_info_.find(display_id);
  CHECK(iter != display_info_.end()) << display_id;
  return iter->second;
}

const Display DisplayManager::GetMirroringDisplayById(
    int64_t display_id) const {
  auto iter = base::ranges::find(software_mirroring_display_list_, display_id,
                                 &Display::id);
  return iter == software_mirroring_display_list_.end() ? Display() : *iter;
}

std::string DisplayManager::GetDisplayNameForId(int64_t id) const {
  if (id == kInvalidDisplayId)
    return l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_UNKNOWN);

  auto iter = display_info_.find(id);
  if (iter != display_info_.end() && !iter->second.name().empty())
    return iter->second.name();

  return base::StringPrintf("Display %d", static_cast<int>(id));
}

bool DisplayManager::ShouldSetMirrorModeOn(
    const DisplayIdList& new_id_list,
    bool should_check_hardware_mirroring) {
  DCHECK(new_id_list.size() > 1);
  if (layout_store_->forced_mirror_mode_for_tablet())
    return true;

  if (disable_restoring_mirror_mode_for_test_)
    return false;

  if (mixed_mirror_mode_params_) {
    // Mixed mirror mode should be restored.
    return true;
  }

  if (should_restore_mirror_mode_from_display_prefs_ ||
      num_connected_displays() <= 1) {
    // The ChromeOS just boots up, the display prefs have just been loaded, or
    // we only have one display. Restore mirror mode based on the external
    // displays' mirror info stored in the preferences. Mirror mode should be on
    // if one of the external displays was in mirror mode before.
    should_restore_mirror_mode_from_display_prefs_ = false;

    for (int64_t id : new_id_list) {
      if (external_display_mirror_info_.count(
              GetDisplayIdWithoutOutputIndex(id))) {
        return true;
      }
    }
  }
  // Mirror mode should remain unchanged as long as there are more than one
  // connected displays.
  return IsInSoftwareMirrorMode() ||
         (should_check_hardware_mirroring && IsInHardwareMirrorMode());
}

void DisplayManager::SetMirrorMode(
    MirrorMode mode,
    const absl::optional<MixedMirrorModeParams>& mixed_params) {
  if (num_connected_displays() < 2)
    return;

  // If the user turned off mirror mode, disable
  // `forced_mirror_mode_for_tablet`.
  if (mode != MirrorMode::kNormal &&
      layout_store_->forced_mirror_mode_for_tablet()) {
    layout_store_->set_forced_mirror_mode_for_tablet(false);
  }

  if (mode == MirrorMode::kMixed) {
    // Set mixed mirror mode parameters. This will be used to do two things:
    // 1. Set the specified source and destination displays in mirror mode
    // configuration (We call this mode mixed mirror mode).
    // 2. Restore the mixed mirror mode when display configuration changes.
    mixed_mirror_mode_params_ = mixed_params;
  } else {
    DCHECK(mixed_params == absl::nullopt);
    // Clear mixed mirror mode parameters here to avoid restoring the mode after
    // display configuration changes.
    mixed_mirror_mode_params_ = absl::nullopt;
  }

  const bool enabled = mode != MirrorMode::kOff;
  if (configure_displays_) {
    MultipleDisplayState new_state =
        enabled ? MULTIPLE_DISPLAY_STATE_MULTI_MIRROR
                : MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED;
    display_configurator_->SetDisplayMode(new_state);
    return;
  }
  multi_display_mode_ =
      enabled ? MIRRORING : current_default_multi_display_mode_;
  ReconfigureDisplays();
}

void DisplayManager::AddRemoveDisplay(
    ManagedDisplayInfo::ManagedDisplayModeList display_modes) {
  DCHECK(!active_display_list_.empty());

  DisplayInfoList new_display_info_list;
  const ManagedDisplayInfo& first_display =
      IsInUnifiedMode()
          ? GetDisplayInfo(software_mirroring_display_list_[0].id())
          : GetDisplayInfo(active_display_list_[0].id());
  new_display_info_list.push_back(first_display);
  // Add if there is only one display connected.
  if (num_connected_displays() == 1) {
    gfx::Rect host_bounds = first_display.bounds_in_native();
    if (display_modes.empty()) {
      display_modes.emplace_back(
          gfx::Size(host_bounds.height() + 100 /* width */,
                    host_bounds.height()),
          60.0, /* refresh_rate */
          false /* is_interlaced */, true /* native */);
    }

    // Find native display mode (or just the first one if there is no
    // mode marked as native) and create a display with this mode as a default
    const ManagedDisplayMode* native_display_mode = &(display_modes[0]);
    for (const ManagedDisplayMode& display_mode : display_modes) {
      if (display_mode.native()) {
        native_display_mode = &display_mode;
        break;
      }
    }

    const int kVerticalOffsetPx = 100;
    // Layout the 2nd display below the primary as with the real device.
    ManagedDisplayInfo display = ManagedDisplayInfo::CreateFromSpecWithID(
        base::StringPrintf("%d+%d-%dx%d", host_bounds.x(),
                           host_bounds.bottom() + kVerticalOffsetPx,
                           native_display_mode->size().width(),
                           native_display_mode->size().height()),
        first_display.id() + 1);

    display.SetManagedDisplayModes(std::move(display_modes));
    new_display_info_list.push_back(std::move(display));
  }
  connected_display_id_list_ = CreateDisplayIdList(new_display_info_list);
  ClearMirroringSourceAndDestination();
  UpdateDisplaysWith(new_display_info_list);
}

void DisplayManager::InitConfigurator(
    std::unique_ptr<NativeDisplayDelegate> delegate) {
  display_configurator_ = std::make_unique<display::DisplayConfigurator>();
  display_configurator_->Init(std::move(delegate),
                              false /* is_panel_fitting_enabled */);
  display_configurator_->SetConfigureDisplays(configure_displays_);
}

void DisplayManager::ForceInitialConfigureWithObservers(
    display::DisplayChangeObserver* display_change_observer,
    display::DisplayConfigurator::Observer* display_error_observer) {
  // Register |display_change_observer_| first so that the rest of
  // observer gets invoked after the root windows are configured.
  display_configurator_->AddObserver(display_change_observer);
  display_configurator_->AddObserver(display_error_observer);
  display_configurator_->set_state_controller(display_change_observer);
  display_configurator_->set_mirroring_controller(this);
  display_configurator_->ForceInitialConfigure();
}

void DisplayManager::SetSoftwareMirroring(bool enabled) {
  SetMultiDisplayMode(enabled ? MIRRORING
                              : current_default_multi_display_mode_);
}

bool DisplayManager::SoftwareMirroringEnabled() const {
  return multi_display_mode_ == MIRRORING;
}

bool DisplayManager::IsSoftwareMirroringEnforced() const {
  // There is no source display for hardware mirroring, so enforce software
  // mirroring if the mixed mirror mode parameters are specified.
  // Enforce software mirroring if tablet mode is enabled as well because
  // the tablet's rotation should be offset in external display.
  return !!mixed_mirror_mode_params_ ||
         layout_store_->forced_mirror_mode_for_tablet();
}

void DisplayManager::SetTouchCalibrationData(
    int64_t display_id,
    const TouchCalibrationData::CalibrationPointPairQuad& point_pair_quad,
    const gfx::Size& display_bounds,
    const ui::TouchscreenDevice& touchdevice) {
  // We do not proceed with setting the calibration and association if the
  // touch device identified by |touch_device_identifier| is an internal touch
  // device.
  if (touchdevice.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
    return;

  // Id of the display the touch device in context is currently associated
  // with. This display id will be equal to |display_id| if no reassociation is
  // being performed.
  int64_t previous_display_id =
      touch_device_manager_->GetAssociatedDisplay(touchdevice);

  bool update_add_support = false;
  bool update_remove_support = false;

  TouchCalibrationData calibration_data(point_pair_quad, display_bounds);
  touch_device_manager_->AddTouchCalibrationData(touchdevice, display_id,
                                                 calibration_data);

  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      info.set_touch_support(Display::TouchSupport::AVAILABLE);
      update_add_support = true;
    } else if (info.id() == previous_display_id) {
      // Since we are reassociating the touch device to another display, we need
      // to check whether the display it was previous connected to still
      // supports touch.
      if (!touch_device_manager_
               ->GetAssociatedTouchDevicesForDisplay(previous_display_id)
               .empty()) {
        info.set_touch_support(Display::TouchSupport::UNAVAILABLE);
        update_remove_support = true;
      }
    }
    display_info_list.push_back(info);
  }

  // Update the non active displays.
  if (!update_add_support) {
    display_info_[display_id].set_touch_support(
        Display::TouchSupport::AVAILABLE);
  }
  if (!update_remove_support &&
      !touch_device_manager_
           ->GetAssociatedTouchDevicesForDisplay(previous_display_id)
           .empty()) {
    display_info_[previous_display_id].set_touch_support(
        Display::TouchSupport::UNAVAILABLE);
  }
  // Update the active displays.
  if (update_add_support || update_remove_support)
    UpdateDisplaysWith(display_info_list);
}

void DisplayManager::ClearTouchCalibrationData(
    int64_t display_id,
    absl::optional<ui::TouchscreenDevice> touchdevice) {
  if (touchdevice) {
    touch_device_manager_->ClearTouchCalibrationData(*touchdevice, display_id);
  } else {
    touch_device_manager_->ClearAllTouchCalibrationData(display_id);
  }

  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    display_info_list.push_back(info);
  }
  UpdateDisplaysWith(display_info_list);
}

void DisplayManager::UpdateZoomFactor(int64_t display_id, float zoom_factor) {
  DCHECK(zoom_factor > 0);
  DCHECK_NE(display_id, kInvalidDisplayId);
  auto iter = display_info_.find(display_id);
  if (iter == display_info_.end())
    return;

  if (IsInternalDisplayId(display_id)) {
    on_display_zoom_modify_timeout_.Cancel();
    on_display_zoom_modify_timeout_.Reset(
        base::BindOnce(&OnInternalDisplayZoomChanged, zoom_factor));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, on_display_zoom_modify_timeout_.callback(),
        base::Seconds(kDisplayZoomModifyTimeoutSec));
  }

  iter->second.set_zoom_factor(zoom_factor);

  for (const auto& display : active_display_list_) {
    if (display.id() == display_id) {
      UpdateDisplays();
      break;
    }
  }
}

bool DisplayManager::HasUnassociatedDisplay() const {
  return display_configurator_->has_unassociated_display();
}

void DisplayManager::SetDefaultMultiDisplayModeForCurrentDisplays(
    MultiDisplayMode mode) {
  DCHECK_NE(MIRRORING, mode);
  DisplayIdList list = GetConnectedDisplayIdList();
  layout_store_->UpdateDefaultUnified(list, mode == UNIFIED);
  ReconfigureDisplays();
}

void DisplayManager::SetMultiDisplayMode(MultiDisplayMode mode) {
  multi_display_mode_ = mode;
}

void DisplayManager::ReconfigureDisplays() {
  DisplayInfoList display_info_list;
  for (const Display& display : active_display_list_) {
    if (display.id() == kUnifiedDisplayId)
      continue;
    display_info_list.push_back(GetDisplayInfo(display.id()));
  }
  for (const Display& display : software_mirroring_display_list_)
    display_info_list.push_back(GetDisplayInfo(display.id()));
  ClearMirroringSourceAndDestination();
  UpdateDisplaysWith(display_info_list);
}

bool DisplayManager::UpdateDisplayBounds(int64_t display_id,
                                         const gfx::Rect& new_bounds) {
  if (!change_display_upon_host_resize_)
    return false;
  display_info_[display_id].SetBounds(new_bounds);
  // Don't notify observers if the mirrored window has changed.
  if (IsInSoftwareMirrorMode() &&
      base::Contains(software_mirroring_display_list_, display_id,
                     &Display::id)) {
    return false;
  }

  // In unified mode then |active_display_list_| won't have a display for
  // |display_id| but |software_mirroring_display_list_| should. Reconfigure
  // the displays so the unified display size is recomputed.
  if (IsInUnifiedMode() &&
      ContainsDisplayWithId(software_mirroring_display_list_, display_id)) {
    DCHECK(!IsActiveDisplayId(display_id));
    ReconfigureDisplays();
    return true;
  }

  Display* display = FindDisplayForId(display_id);
  DCHECK(display);
  display->SetSize(display_info_[display_id].size_in_pixel());
  BeginEndNotifier notifier(this);
  NotifyMetricsChanged(*display, DisplayObserver::DISPLAY_METRIC_BOUNDS);
  return true;
}

void DisplayManager::CreateMirrorWindowAsyncIfAny() {
  // Do not post a task if the software mirroring doesn't exist, or
  // during initialization when compositor's init task isn't posted yet.
  // ash::Shell::Init() will call this after the compositor is initialized.
  if (software_mirroring_display_list_.empty() || !delegate_)
    return;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DisplayManager::CreateMirrorWindowIfAny,
                                weak_ptr_factory_.GetWeakPtr()));
}

void DisplayManager::UpdateInternalManagedDisplayModeListForTest() {
  if (!HasInternalDisplay() ||
      display_info_.count(Display::InternalDisplayId()) == 0) {
    return;
  }
  ManagedDisplayInfo* info = &display_info_[Display::InternalDisplayId()];
  SetInternalManagedDisplayModeList(info);
}

bool DisplayManager::ZoomDisplay(int64_t display_id, bool up) {
  if (IsInUnifiedMode()) {
    DCHECK_EQ(display_id, kUnifiedDisplayId);
    const ManagedDisplayInfo& display_info = GetDisplayInfo(display_id);

    ManagedDisplayMode mode;
    bool result = GetDisplayModeForNextResolution(display_info, up, &mode);
    return result ? SetDisplayMode(display_id, mode) : false;
  }

  ManagedDisplayMode display_mode;
  if (!GetActiveModeForDisplayId(display_id, &display_mode))
    return false;
  auto iter = display_info_.find(display_id);
  if (iter == display_info_.end())
    return false;

  const float current_display_zoom = iter->second.zoom_factor();

  // Find the index of |current_display_zoom| in |zooms|. The nearest value is
  // used if the exact match is not found.
  const std::vector<float> zooms = GetDisplayZoomFactors(display_mode);
  std::size_t zoom_idx = 0;
  float min_diff = std::abs(zooms[zoom_idx] - current_display_zoom);
  for (std::size_t i = 1; i < zooms.size(); i++) {
    const float diff = std::abs(current_display_zoom - zooms[i]);
    if (diff < min_diff) {
      min_diff = diff;
      zoom_idx = i;
    }
  }
  // The index of the next zoom value.
  const std::size_t next_zoom_idx = zoom_idx + (up ? -1 : 1);

  // If the zoom index is out of bounds, that is, the display is already at
  // maximum or minimum zoom then do nothing.
  if (next_zoom_idx < 0 || next_zoom_idx >= zooms.size())
    return false;

  // Update zoom factor via the display manager API to ensure UMA metrics are
  // recorded.
  UpdateZoomFactor(display_id, zooms[next_zoom_idx]);
  return true;
}

void DisplayManager::ResetDisplayZoom(int64_t display_id) {
  if (IsInUnifiedMode()) {
    DCHECK_EQ(display_id, kUnifiedDisplayId);
    const ManagedDisplayInfo& display_info = GetDisplayInfo(kUnifiedDisplayId);
    const ManagedDisplayInfo::ManagedDisplayModeList& modes =
        display_info.display_modes();
    auto iter = base::ranges::find_if(modes, &ManagedDisplayMode::native);
    SetDisplayMode(kUnifiedDisplayId, *iter);
    return;
  }

  auto iter = display_info_.find(display_id);
  if (iter == display_info_.end())
    return;
  if (std::abs(iter->second.zoom_factor() - 1.f) > 0.001f) {
    iter->second.set_zoom_factor(1.f);
    UpdateDisplays();
  }
}

void DisplayManager::CreateSoftwareMirroringDisplayInfo(
    DisplayInfoList* display_info_list) {
  // Use the internal display or 1st as the mirror source, then scale
  // the root window so that it matches the external display's
  // resolution. This is necessary in order for scaling to work while
  // mirrored.
  switch (multi_display_mode_) {
    case MIRRORING: {
      if (display_info_list->size() < 2)
        return;

      std::set<int64_t> destination_ids;
      int64_t source_id = kInvalidDisplayId;
      if (mixed_mirror_mode_params_) {
        // Use the specified source and destination displays if mixed mirror
        // mode is requested.
        source_id = mixed_mirror_mode_params_->source_id;
        for (auto id : mixed_mirror_mode_params_->destination_ids)
          destination_ids.insert(id);
      } else {
        // Select a default source display and treat all other connected
        // displays as destination.
        if (HasInternalDisplay()) {
          // Use the internal display as mirroring source.
          source_id = Display::InternalDisplayId();
          if (!base::Contains(*display_info_list, source_id,
                              &ManagedDisplayInfo::id)) {
            // It is possible that internal display is removed (e.g. Use
            // Chromebook in Dock mode with two or more external displays). In
            // this case, we use the first connected display as mirroring
            // source.
            source_id = first_display_id_;
          }
        } else {
          // Use the first connected display as mirroring source
          source_id = first_display_id_;
        }
        DCHECK(source_id != kInvalidDisplayId);

        for (auto& info : *display_info_list) {
          if (source_id != info.id())
            destination_ids.insert(info.id());
        }
      }

      for (auto iter = display_info_list->begin();
           iter != display_info_list->end();) {
        if (destination_ids.count(iter->id())) {
          iter->SetOverscanInsets(gfx::Insets());
          InsertAndUpdateDisplayInfo(*iter);
          software_mirroring_display_list_.emplace_back(
              CreateMirroringDisplayFromDisplayInfoById(iter->id(),
                                                        gfx::Point(), 1.0f));

          // Remove the destination display.
          iter = display_info_list->erase(iter);
        } else {
          ++iter;
        }
      }

      mirroring_source_id_ = source_id;
      break;
    }
    case UNIFIED:
      CreateUnifiedDesktopDisplayInfo(display_info_list);
      break;

    case EXTENDED:
      break;
  }
}

void DisplayManager::CreateUnifiedDesktopDisplayInfo(
    DisplayInfoList* display_info_list) {
  if (display_info_list->size() == 1)
    return;

  for (auto& display_info : *display_info_list) {
    auto it = display_info_.find(display_info.id());
    if (it != display_info_.end()) {
      display_info.SetRotation(
          it->second.GetRotation(Display::RotationSource::USER),
          Display::RotationSource::USER);
      display_info.SetRotation(
          it->second.GetRotation(Display::RotationSource::ACTIVE),
          Display::RotationSource::ACTIVE);
      display_info.UpdateDisplaySize();
    }
  }

  if (!ValidateMatrix(current_unified_desktop_matrix_) ||
      !ValidateMatrixForDisplayInfoList(*display_info_list,
                                        current_unified_desktop_matrix_)) {
    // Recreate the default matrix where displays are laid out horizontally from
    // left to right.
    current_unified_desktop_matrix_.clear();
    current_unified_desktop_matrix_.resize(1);
    for (const auto& info : *display_info_list)
      current_unified_desktop_matrix_[0].emplace_back(info.id());
  }

  software_mirroring_display_list_.clear();
  mirroring_display_id_to_unified_matrix_row_.clear();
  unified_display_rows_heights_.clear();

  const size_t num_rows = current_unified_desktop_matrix_.size();
  const size_t num_columns = current_unified_desktop_matrix_[0].size();

  // 1 - Find the maximum height per each row.
  std::vector<int> rows_max_heights;
  rows_max_heights.reserve(num_rows);
  for (const auto& row : current_unified_desktop_matrix_) {
    int max_height = std::numeric_limits<int>::min();
    for (const auto& id : row) {
      const ManagedDisplayInfo* info = FindInfoById(*display_info_list, id);
      DCHECK(info);
      max_height = std::max(max_height, info->size_in_pixel().height());
    }
    rows_max_heights.emplace_back(max_height);
  }

  // 2 - Use the maximum height per each row to calculate the scale value for
  //     each display in each row so that it fits the max row height. Use that
  //     to calculate the total bounds of each row after all displays has been
  //     scaled.

  // Holds the scale value of each display in the matrix.
  std::vector<std::vector<float>> scales;
  scales.resize(num_rows);

  // Holds the total bounds of each row in the matrix.
  std::vector<gfx::Rect> rows_bounds;
  rows_bounds.reserve(num_rows);

  // Calculate the bounds of each row, and the maximum row width.
  int max_total_width = std::numeric_limits<int>::min();
  for (size_t i = 0; i < num_rows; ++i) {
    const auto& row = current_unified_desktop_matrix_[i];
    const int max_row_height = rows_max_heights[i];
    gfx::Rect this_row_bounds;
    scales[i].resize(num_columns);
    for (size_t j = 0; j < num_columns; ++j) {
      const auto& id = row[j];
      const ManagedDisplayInfo* info = FindInfoById(*display_info_list, id);
      DCHECK(info);

      InsertAndUpdateDisplayInfo(*info);
      const float scale =
          info->size_in_pixel().height() / static_cast<float>(max_row_height);
      scales[i][j] = scale;

      const gfx::Point origin(this_row_bounds.right(), 0);
      const auto display_bounds = gfx::Rect(
          origin, gfx::ScaleToFlooredSize(info->size_in_pixel(), 1.0f / scale));
      this_row_bounds.Union(display_bounds);
    }
    rows_bounds.emplace_back(this_row_bounds);
    max_total_width = std::max(max_total_width, this_row_bounds.width());
  }

  // 3 - Using the maximum row width, adjust the display scales so that each
  //     row width fits the maximum row width.
  for (size_t i = 0; i < num_rows; ++i) {
    const auto& row_bound = rows_bounds[i];
    const float scale = row_bound.width() / static_cast<float>(max_total_width);
    auto& row_scales = scales[i];
    for (auto& display_scale : row_scales)
      display_scale *= scale;
  }

  // 4 - Now that we know the final scales, compute the unified display size by
  //     computing the unified display size of each row and then getting the
  //     union of all rows.
  gfx::Rect unified_bounds;  // Will hold the final unified bounds.
  std::vector<UnifiedDisplayModeParam> modes_param_list;
  modes_param_list.reserve(num_rows * num_columns);
  int internal_display_index = -1;
  for (size_t i = 0; i < num_rows; ++i) {
    const auto& row = current_unified_desktop_matrix_[i];
    gfx::Rect row_displays_bounds;
    for (size_t j = 0; j < num_columns; ++j) {
      const auto& id = row[j];
      if (internal_display_index == -1 && IsInternalDisplayId(id))
        internal_display_index = i * num_columns + j;

      const ManagedDisplayInfo* info = FindInfoById(*display_info_list, id);
      DCHECK(info);

      const float scale = scales[i][j];
      const gfx::Point origin(row_displays_bounds.right(),
                              unified_bounds.bottom());
      // The display is scaled to fit the unified desktop size.
      Display display =
          CreateMirroringDisplayFromDisplayInfoById(id, origin, 1.0f / scale);

      row_displays_bounds.Union(display.bounds());
      modes_param_list.emplace_back(info->device_scale_factor(), scale, false);
      software_mirroring_display_list_.emplace_back(display);
    }

    unified_bounds.Union(row_displays_bounds);
  }

  // The index of the display that will be used for the default native mode.
  const int default_mode_param_index =
      internal_display_index != -1 ? internal_display_index : 0;
  modes_param_list[default_mode_param_index].is_default_mode = true;

  // 5 - Create the Unified display info and its modes.
  ManagedDisplayInfo unified_display_info(kUnifiedDisplayId, "Unified Desktop",
                                          /*has_overscan=*/false);
  ManagedDisplayMode native_mode(unified_bounds.size(), 60.0f, false, true,
                                 /*device_scale_factor=*/1.0);
  ManagedDisplayInfo::ManagedDisplayModeList modes =
      CreateUnifiedManagedDisplayModeList(native_mode, modes_param_list);

  // Find the default mode.
  auto default_mode_iter =
      base::ranges::find_if(modes, &ManagedDisplayMode::native);
  DCHECK(default_mode_iter != modes.end());

  if (default_mode_iter != modes.end()) {
    const ManagedDisplayMode& default_mode = *default_mode_iter;
    unified_display_info.set_device_scale_factor(
        default_mode.device_scale_factor());
    unified_display_info.SetBounds(gfx::Rect(default_mode.size()));
  }

  unified_display_info.SetManagedDisplayModes(modes);

  // Forget the configured resolution if the original unified desktop resolution
  // has changed.
  if (display_info_.count(kUnifiedDisplayId) != 0 &&
      GetMaxNativeSize(display_info_[kUnifiedDisplayId]) !=
          unified_bounds.size()) {
    display_modes_.erase(kUnifiedDisplayId);
  }

  // 6 - Set the selected mode.
  ManagedDisplayMode selected_mode;
  if (GetSelectedModeForDisplayId(kUnifiedDisplayId, &selected_mode) &&
      FindDisplayMode(unified_display_info, selected_mode) !=
          unified_display_info.display_modes().end()) {
    unified_display_info.set_device_scale_factor(
        selected_mode.device_scale_factor());
    unified_display_info.SetBounds(gfx::Rect(selected_mode.size()));
  } else {
    display_modes_.erase(kUnifiedDisplayId);
  }

  const float unified_bounds_scale_y =
      unified_display_info.size_in_pixel().height() /
      static_cast<float>(unified_bounds.size().height());

  // 7 - Now that we know the final unified display bounds, update the displays
  //     in the |software_mirroring_display_list_| list so that they have the
  //     correct bounds.
  DCHECK_EQ(num_rows * num_columns, software_mirroring_display_list_.size());
  int last_bottom = 0;
  for (size_t i = 0; i < num_rows; ++i) {
    int last_right = 0;
    int max_height = std::numeric_limits<int>::min();
    for (size_t j = 0; j < num_columns; ++j) {
      Display& current_display =
          software_mirroring_display_list_[i * num_columns + j];
      gfx::SizeF scaled_size(current_display.bounds().size());
      scaled_size.Scale(unified_bounds_scale_y);
      const gfx::Point origin(last_right, last_bottom);
      current_display.set_bounds(
          gfx::Rect(origin, gfx::ToRoundedSize(scaled_size)));
      current_display.UpdateWorkAreaFromInsets(gfx::Insets());
      const gfx::Rect display_bounds = current_display.bounds();
      max_height = std::max(max_height, display_bounds.height());
      last_right = display_bounds.right();
      mirroring_display_id_to_unified_matrix_row_[current_display.id()] = i;
    }

    unified_display_rows_heights_.emplace_back(max_height);
    last_bottom += max_height;
  }

  DCHECK_EQ(num_rows, unified_display_rows_heights_.size());

  display_info_list->clear();
  display_info_list->emplace_back(unified_display_info);
  InsertAndUpdateDisplayInfo(unified_display_info);

  UMA_HISTOGRAM_ENUMERATION(
      "DisplayManager.UnifiedDesktopDisplayCountRange",
      GetDisplayCountRange(software_mirroring_display_list_.size()),
      DisplayCountRange::kCount);
}

Display* DisplayManager::FindDisplayForId(int64_t id) {
  auto iter = base::ranges::find(active_display_list_, id, &Display::id);
  if (iter != active_display_list_.end())
    return &(*iter);
  return nullptr;
}

void DisplayManager::AddMirrorDisplayInfoIfAny(
    DisplayInfoList* display_info_list) {
  if (!IsInSoftwareMirrorMode())
    return;
  for (const auto& display : software_mirroring_display_list_)
    display_info_list->emplace_back(GetDisplayInfo(display.id()));
  software_mirroring_display_list_.clear();
}

void DisplayManager::InsertAndUpdateDisplayInfo(
    const ManagedDisplayInfo& new_info) {
  auto it = display_info_.find(new_info.id());
  if (it != display_info_.end()) {
    ManagedDisplayInfo* info = &(it->second);
    info->Copy(new_info);
  } else {
    display_info_[new_info.id()] = new_info;
    // Set from_native_platform to false so that all information
    // (rotation, zoom factor etc.) is copied.
    display_info_[new_info.id()].set_from_native_platform(false);
  }
  display_info_[new_info.id()].UpdateDisplaySize();
}

Display DisplayManager::CreateDisplayFromDisplayInfoById(int64_t id) {
  DCHECK(display_info_.find(id) != display_info_.end()) << "id=" << id;
  const ManagedDisplayInfo& display_info = display_info_[id];

  Display new_display(display_info.id());
  gfx::Rect bounds_in_native(display_info.size_in_pixel());
  float device_scale_factor = display_info.GetEffectiveDeviceScaleFactor();

  // Simply set the origin to (0,0).  The primary display's origin is
  // always (0,0) and the bounds of non-primary display(s) will be updated
  // in |UpdateNonPrimaryDisplayBoundsForLayout| called in |UpdateDisplay|.
  new_display.SetScaleAndBounds(device_scale_factor,
                                gfx::Rect(bounds_in_native.size()));
  new_display.set_rotation(display_info.GetActiveRotation());
  new_display.set_panel_rotation(display_info.GetLogicalActiveRotation());
  new_display.set_touch_support(display_info.touch_support());
  new_display.set_maximum_cursor_size(display_info.maximum_cursor_size());
  new_display.set_color_spaces(display_info.display_color_spaces());
  new_display.set_display_frequency(display_info.refresh_rate());
  new_display.set_label(display_info.name());
  new_display.SetDRMFormatsAndModifiers(
      display_info.GetDRMFormatsAndModifiers());

  constexpr uint32_t kNormalBitDepthNumBitsPerChannel = 8u;
  if (display_info.bits_per_channel() > kNormalBitDepthNumBitsPerChannel) {
    new_display.set_depth_per_component(display_info.bits_per_channel());
    constexpr uint32_t kRGBNumChannels = 3u;
    new_display.set_color_depth(display_info.bits_per_channel() *
                                kRGBNumChannels);
  }
  if (internal_display_has_accelerometer_ && IsInternalDisplayId(id)) {
    new_display.set_accelerometer_support(
        Display::AccelerometerSupport::AVAILABLE);
  } else {
    new_display.set_accelerometer_support(
        Display::AccelerometerSupport::UNAVAILABLE);
  }
  return new_display;
}

Display DisplayManager::CreateMirroringDisplayFromDisplayInfoById(
    int64_t id,
    const gfx::Point& origin,
    float scale) {
  DCHECK(display_info_.find(id) != display_info_.end()) << "id=" << id;
  const ManagedDisplayInfo& display_info = display_info_[id];

  Display new_display(display_info.id());
  new_display.SetScaleAndBounds(
      1.0f, gfx::Rect(origin, gfx::ScaleToFlooredSize(
                                  display_info.size_in_pixel(), scale)));
  new_display.set_touch_support(display_info.touch_support());
  new_display.set_maximum_cursor_size(display_info.maximum_cursor_size());
  new_display.set_rotation(display_info.GetActiveRotation());
  new_display.set_panel_rotation(display_info.GetLogicalActiveRotation());
  new_display.set_label(display_info.name());
  return new_display;
}

void DisplayManager::UpdateNonPrimaryDisplayBoundsForLayout(
    Displays* display_list,
    std::vector<size_t>* updated_indices) {
  if (display_list->size() == 1u)
    return;

  const DisplayLayout& layout =
      layout_store_->GetRegisteredDisplayLayout(GetConnectedDisplayIdList());

  DCHECK(IsConnectedDisplayIdListInSyncWithCurrentState(
      CreateDisplayIdList(*display_list)));

  // Ignore if a user has a old format (should be extremely rare)
  // and this will be replaced with DCHECK.
  if (layout.primary_id == kInvalidDisplayId)
    return;

  // display_list does not have translation set, so ApplyDisplayLayout cannot
  // provide accurate change information. We'll find the changes after the call.
  current_resolved_layout_ = layout.Copy();
  ApplyDisplayLayout(current_resolved_layout_.get(), display_list, nullptr);
  size_t num_displays = display_list->size();
  for (size_t index = 0; index < num_displays; ++index) {
    const Display& display = (*display_list)[index];
    int64_t id = display.id();
    const Display* active_display = FindDisplayForId(id);
    if (!active_display || (active_display->bounds() != display.bounds()))
      updated_indices->push_back(index);
  }
}

void DisplayManager::CreateMirrorWindowIfAny() {
  if (!software_mirroring_display_list_.empty() && delegate_) {
    DisplayInfoList list;
    for (auto& display : software_mirroring_display_list_)
      list.push_back(GetDisplayInfo(display.id()));
    delegate_->CreateOrUpdateMirroringDisplay(list);
  }
  if (created_mirror_window_)
    std::move(created_mirror_window_).Run();
  DCHECK(IsConnectedDisplayIdListInSyncWithCurrentState(
      CreateDisplayIdList(active_display_list())));
}

void DisplayManager::ApplyDisplayLayout(DisplayLayout* layout,
                                        Displays* display_list,
                                        std::vector<int64_t>* updated_ids) {
  if (multi_display_mode_ == UNIFIED) {
    // Applying the layout in unified mode doesn't make sense, since there's no
    // layout.
    return;
  }
  // In mixed mirror mode, temporarily remove the mirror destination from the
  // layout.
  if (mixed_mirror_mode_params_) {
    std::unique_ptr<DisplayLayout> temp_layout = layout->Copy();
    temp_layout->RemoveDisplayPlacements(
        mixed_mirror_mode_params_->destination_ids);
    temp_layout->ApplyToDisplayList(display_list, updated_ids,
                                    kMinimumOverlapForInvalidOffset);
  } else {
    layout->ApplyToDisplayList(display_list, updated_ids,
                               kMinimumOverlapForInvalidOffset);
  }
}

void DisplayManager::RunPendingTasksForTest() {
  if (software_mirroring_display_list_.empty())
    return;

  base::RunLoop run_loop;
  created_mirror_window_ = run_loop.QuitClosure();
  run_loop.Run();
}

void DisplayManager::SetTabletState(const TabletState& tablet_state) {
  tablet_state_ = tablet_state;
  for (auto& observer : observers_)
    observer.OnDisplayTabletStateChanged(tablet_state);
}

void DisplayManager::NotifyMetricsChanged(const Display& display,
                                          uint32_t metrics) {
  for (auto& observer : observers_)
    observer.OnDisplayMetricsChanged(display, metrics);
}

void DisplayManager::NotifyDisplayAdded(const Display& display) {
  for (auto& observer : observers_)
    observer.OnDisplayAdded(display);
}

void DisplayManager::NotifyDisplayRemoved(const Display& display) {
  for (auto& observer : observers_)
    observer.OnDisplayRemoved(display);
}

void DisplayManager::AddObserver(DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void DisplayManager::RemoveObserver(DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

display::TabletState DisplayManager::GetTabletState() const {
  return tablet_state_;
}

void DisplayManager::UpdateInfoForRestoringMirrorMode() {
  if (num_connected_displays() <= 1)
    return;

  // The display prefs have just been loaded and we're waiting for the
  // reconfiguration of the displays to apply the newly loaded prefs. We should
  // not overwrite the newly-loaded external display mirror configs.
  // https://crbug.com/936884.
  if (should_restore_mirror_mode_from_display_prefs_)
    return;

  // External displays mirrored because of forced tablet mode mirroring should
  // not be considered candidates for restoring their mirrored state.
  // https://crbug.com/919994.
  if (layout_store_->forced_mirror_mode_for_tablet())
    return;

  for (auto id : GetConnectedDisplayIdList()) {
    if (IsInternalDisplayId(id))
      continue;
    // Mask the output index out (8 bits) so that the user does not have to
    // reconnect a display to the same port to restore mirror mode.
    int64_t masked_id = GetDisplayIdWithoutOutputIndex(id);
    if (IsInMirrorMode())
      external_display_mirror_info_.emplace(masked_id);
    else
      external_display_mirror_info_.erase(masked_id);
  }
}

void DisplayManager::UpdatePrimaryDisplayIdIfNecessary() {
  if (num_connected_displays() < 2)
    return;

  const display::DisplayIdList list = GetConnectedDisplayIdList();
  const display::DisplayLayout& layout =
      layout_store()->GetRegisteredDisplayLayout(list);
  layout_store()->UpdateDefaultUnified(list, layout.default_unified);
  if (delegate_ && GetNumDisplays() > 1) {
    delegate_->SetPrimaryDisplayId(
        layout.primary_id == display::kInvalidDisplayId ? list[0]
                                                        : layout.primary_id);
  }
}

}  // namespace display
