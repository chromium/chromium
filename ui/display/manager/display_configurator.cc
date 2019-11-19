// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_configurator.h"

#include <cstddef>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chromeos/system/devicemode.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/content_protection_manager.h"
#include "ui/display/manager/display_layout_manager.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/update_display_configuration_task.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/display/util/display_util.h"

namespace display {

namespace {

typedef std::vector<const DisplayMode*> DisplayModeList;

struct DisplayState {
  DisplaySnapshot* display = nullptr;  // Not owned.

  // User-selected mode for the display.
  const DisplayMode* selected_mode = nullptr;

  // Mode used when displaying the same desktop on multiple displays.
  const DisplayMode* mirror_mode = nullptr;
};

// This is used for calling either SetColorMatrix() or SetGammaCorrection()
// depending on the given |color_correction_closure| which is run synchronously.
// If |reset_color_space_on_success| is true and running
// |color_correction_closure| returns true, then the color space of the display
// with |display_id| will be reset.
bool RunColorCorrectionClosureSync(
    int64_t display_id,
    const DisplayConfigurator::DisplayStateList& cached_displays,
    bool reset_color_space_on_success,
    base::OnceCallback<bool(void)> color_correction_closure) {
  for (DisplaySnapshot* display : cached_displays) {
    if (display->display_id() != display_id)
      continue;

    const bool success = std::move(color_correction_closure).Run();

    // Nullify the |display|s ColorSpace to avoid correcting colors twice, if
    // we have successfully configured something.
    if (success && reset_color_space_on_success)
      display->reset_color_space();

    return success;
  }

  return false;
}

// Returns true if a platform native |mode| is equal to a |managed_mode|.
bool AreModesEqual(const DisplayMode& mode,
                   const ManagedDisplayMode& managed_mode) {
  return mode.size() == managed_mode.size() &&
         mode.refresh_rate() == managed_mode.refresh_rate() &&
         mode.is_interlaced() == managed_mode.is_interlaced();
}

// Finds and returns a pointer to a platform native mode in the given |display|
// snapshot's modes which exactly matches the given |managed_mode|. Returns
// nullptr if nothing was found.
const DisplayMode* FindExactMatchingMode(
    const DisplaySnapshot& display,
    const ManagedDisplayMode& managed_mode) {
  if (managed_mode.native()) {
    return display.native_mode() &&
                   AreModesEqual(*display.native_mode(), managed_mode)
               ? display.native_mode()
               : nullptr;
  }

  for (const std::unique_ptr<const DisplayMode>& mode : display.modes()) {
    if (AreModesEqual(*mode, managed_mode))
      return mode.get();
  }

  return nullptr;
}

}  // namespace

const int DisplayConfigurator::kSetDisplayPowerNoFlags = 0;
const int DisplayConfigurator::kSetDisplayPowerForceProbe = 1 << 0;
const int DisplayConfigurator::kSetDisplayPowerOnlyIfSingleInternalDisplay =
    1 << 1;

bool DisplayConfigurator::TestApi::TriggerConfigureTimeout() {
  if (configurator_->configure_timer_.IsRunning()) {
    configurator_->configure_timer_.FireNow();
    return true;
  } else {
    return false;
  }
}

base::TimeDelta DisplayConfigurator::TestApi::GetConfigureDelay() const {
  return configurator_->configure_timer_.IsRunning()
             ? configurator_->configure_timer_.GetCurrentDelay()
             : base::TimeDelta();
}

////////////////////////////////////////////////////////////////////////////////
// DisplayConfigurator::DisplayLayoutManagerImpl implementation

class DisplayConfigurator::DisplayLayoutManagerImpl
    : public DisplayLayoutManager {
 public:
  explicit DisplayLayoutManagerImpl(DisplayConfigurator* configurator);
  ~DisplayLayoutManagerImpl() override;

  // DisplayLayoutManager:
  SoftwareMirroringController* GetSoftwareMirroringController() const override;
  StateController* GetStateController() const override;
  MultipleDisplayState GetDisplayState() const override;
  chromeos::DisplayPowerState GetPowerState() const override;
  bool GetDisplayLayout(
      const std::vector<DisplaySnapshot*>& displays,
      MultipleDisplayState new_display_state,
      chromeos::DisplayPowerState new_power_state,
      std::vector<DisplayConfigureRequest>* requests) const override;
  DisplayStateList GetDisplayStates() const override;
  bool IsMirroring() const override;

 private:
  // Parses the |displays| into a list of DisplayStates. This effectively adds
  // |mirror_mode| and |selected_mode| to the returned results.
  // TODO(dnicoara): Break this into GetSelectedMode() and GetMirrorMode() and
  // remove DisplayState.
  std::vector<DisplayState> ParseDisplays(
      const std::vector<DisplaySnapshot*>& displays) const;

  const DisplayMode* GetUserSelectedMode(const DisplaySnapshot& display) const;

  // Return true if all displays are on the same device.
  bool AllDisplaysOnSameDevice(
      const std::vector<DisplayState*>& displays) const;

  // Return true if all displays have display mode.
  bool AllDisplaysHaveDisplayMode(
      const std::vector<DisplayState*>& displays) const;

  // Return true if |mode| has the same aspect ratio as the native mode of
  // |display|.
  bool HasSameAspectRatioAsNativeMode(const DisplaySnapshot* display,
                                      const DisplayMode* mode) const;

  // Helper method for ParseDisplays() that initializes the passed-in displays'
  // |mirror_mode| fields by looking for a matching mode among these displays'
  // mode list. |preserve_native_aspect_ratio| limits the search only to the
  // modes having the native aspect ratio of each external display.
  bool FindExactMatchingMirrorMode(const std::vector<DisplayState*>& displays,
                                   bool preserve_native_aspect_ratio) const;

  DisplayConfigurator* configurator_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DisplayLayoutManagerImpl);
};

DisplayConfigurator::DisplayLayoutManagerImpl::DisplayLayoutManagerImpl(
    DisplayConfigurator* configurator)
    : configurator_(configurator) {}

DisplayConfigurator::DisplayLayoutManagerImpl::~DisplayLayoutManagerImpl() {}

DisplayConfigurator::SoftwareMirroringController*
DisplayConfigurator::DisplayLayoutManagerImpl::GetSoftwareMirroringController()
    const {
  return configurator_->mirroring_controller_;
}

DisplayConfigurator::StateController*
DisplayConfigurator::DisplayLayoutManagerImpl::GetStateController() const {
  return configurator_->state_controller_;
}

MultipleDisplayState
DisplayConfigurator::DisplayLayoutManagerImpl::GetDisplayState() const {
  return configurator_->current_display_state_;
}

chromeos::DisplayPowerState
DisplayConfigurator::DisplayLayoutManagerImpl::GetPowerState() const {
  return configurator_->current_power_state_;
}

std::vector<DisplayState>
DisplayConfigurator::DisplayLayoutManagerImpl::ParseDisplays(
    const std::vector<DisplaySnapshot*>& snapshots) const {
  std::vector<DisplayState> cached_displays;
  for (auto* snapshot : snapshots) {
    DisplayState display_state;
    display_state.display = snapshot;
    display_state.selected_mode = GetUserSelectedMode(*snapshot);
    cached_displays.push_back(display_state);
  }

  // Hardware mirroring doesn't work on desktop-linux Chrome OS's fake displays.
  // Skip mirror mode setup in that case to fall back on software mirroring.
  if (!chromeos::IsRunningAsSystemCompositor())
    return cached_displays;

  if (cached_displays.size() <= 1)
    return cached_displays;

  std::vector<DisplayState*> displays;
  int num_internal_displays = 0;
  for (auto& display : cached_displays) {
    if (display.display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
      ++num_internal_displays;
    displays.emplace_back(&display);
  }
  CHECK_LT(num_internal_displays, 2);
  LOG_IF(WARNING, num_internal_displays >= 2)
      << "At least two internal displays detected.";

  // Hardware mirroring doesn't work among displays on different devices. In
  // this case we revert to software mirroring.
  if (!AllDisplaysOnSameDevice(displays))
    return cached_displays;

  // Hardware mirroring doesn't work for displays that do not have display
  // mode. In this case we revert to software mirroring.
  if (!AllDisplaysHaveDisplayMode(displays))
    return cached_displays;

  bool can_mirror = false;
  for (int attempt = 0; !can_mirror && attempt < 2; ++attempt) {
    // Try preserving external display's aspect ratio on the first attempt.
    // If that fails, fall back to the highest matching resolution.
    bool preserve_aspect = attempt == 0;
    can_mirror = FindExactMatchingMirrorMode(displays, preserve_aspect);
  }
  return cached_displays;
}

bool DisplayConfigurator::DisplayLayoutManagerImpl::GetDisplayLayout(
    const std::vector<DisplaySnapshot*>& displays,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state,
    std::vector<DisplayConfigureRequest>* requests) const {
  std::vector<DisplayState> states = ParseDisplays(displays);
  std::vector<bool> display_power;
  int num_on_displays =
      GetDisplayPower(displays, new_power_state, &display_power);
  VLOG(1) << "EnterState: display="
          << MultipleDisplayStateToString(new_display_state)
          << " power=" << DisplayPowerStateToString(new_power_state);

  // Framebuffer dimensions.
  gfx::Size size;

  for (size_t i = 0; i < displays.size(); ++i) {
    requests->push_back(DisplayConfigureRequest(
        displays[i], displays[i]->current_mode(), gfx::Point()));
  }

  switch (new_display_state) {
    case MULTIPLE_DISPLAY_STATE_INVALID:
      NOTREACHED() << "Ignoring request to enter invalid state with "
                   << displays.size() << " connected display(s)";
      return false;
    case MULTIPLE_DISPLAY_STATE_HEADLESS:
      if (displays.size() != 0) {
        LOG(WARNING) << "Ignoring request to enter headless mode with "
                     << displays.size() << " connected display(s)";
        return false;
      }
      break;
    case MULTIPLE_DISPLAY_STATE_SINGLE: {
      // If there are multiple displays connected, only one should be turned on.
      if (displays.size() != 1 && num_on_displays != 1) {
        LOG(WARNING) << "Ignoring request to enter single mode with "
                     << displays.size() << " connected displays and "
                     << num_on_displays << " turned on";
        return false;
      }

      for (size_t i = 0; i < states.size(); ++i) {
        const DisplayState* state = &states[i];
        (*requests)[i].mode = display_power[i] ? state->selected_mode : NULL;

        if (display_power[i] || states.size() == 1) {
          const DisplayMode* mode_info = state->selected_mode;
          if (!mode_info) {
            LOG(WARNING) << "No selected mode when configuring display: "
                         << state->display->ToString();
            return false;
          }
          if (mode_info->size() == gfx::Size(1024, 768)) {
            VLOG(1) << "Potentially misdetecting display(1024x768):"
                    << " displays size=" << states.size()
                    << ", num_on_displays=" << num_on_displays
                    << ", current size:" << size.width() << "x" << size.height()
                    << ", i=" << i << ", display=" << state->display->ToString()
                    << ", display_mode=" << mode_info->ToString();
          }
          size = mode_info->size();
        }
      }
      break;
    }
    case MULTIPLE_DISPLAY_STATE_MULTI_MIRROR: {
      if (configurator_->mirroring_controller_->IsSoftwareMirroringEnforced()) {
        LOG(WARNING) << "Ignoring request to enter hardware mirror mode "
                        "because software mirroring is enforced";
        return false;
      }

      const bool can_set_mirror_mode =
          states.size() > 1 && num_on_displays != 1;
      if (!can_set_mirror_mode) {
        LOG(WARNING) << "Ignoring request to enter mirrored mode with "
                     << states.size() << " connected display(s) and "
                     << num_on_displays << " turned on";
        return false;
      }

      const DisplayMode* mode_info = states[0].mirror_mode;
      if (!mode_info) {
        LOG(WARNING) << "No mirror mode when configuring display: "
                     << states[0].display->ToString();
        return false;
      }
      size = mode_info->size();

      for (size_t i = 0; i < states.size(); ++i) {
        const DisplayState* state = &states[i];
        (*requests)[i].mode = display_power[i] ? state->mirror_mode : NULL;
      }
      break;
    }
    case MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED: {
      if (states.size() < 2) {
        LOG(WARNING) << "Ignoring request to enter extended mode with "
                     << states.size() << " connected display(s) and "
                     << num_on_displays << " turned on";
        return false;
      }

      for (size_t i = 0; i < states.size(); ++i) {
        const DisplayState* state = &states[i];
        (*requests)[i].origin.set_y(size.height() ? size.height() + kVerticalGap
                                                  : 0);
        (*requests)[i].mode = display_power[i] ? state->selected_mode : NULL;

        // Retain the full screen size even if all displays are off so the
        // same desktop configuration can be restored when the displays are
        // turned back on.
        const DisplayMode* mode_info = states[i].selected_mode;
        if (!mode_info) {
          LOG(WARNING) << "No selected mode when configuring display: "
                       << state->display->ToString();
          return false;
        }

        size.set_width(std::max<int>(size.width(), mode_info->size().width()));
        size.set_height(size.height() + (size.height() ? kVerticalGap : 0) +
                        mode_info->size().height());
      }
      break;
    }
  }
  DCHECK(new_display_state == MULTIPLE_DISPLAY_STATE_HEADLESS ||
         !size.IsEmpty());
  return true;
}

DisplayConfigurator::DisplayStateList
DisplayConfigurator::DisplayLayoutManagerImpl::GetDisplayStates() const {
  return configurator_->cached_displays();
}

bool DisplayConfigurator::DisplayLayoutManagerImpl::IsMirroring() const {
  if (GetDisplayState() == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR)
    return true;

  return GetSoftwareMirroringController() &&
         GetSoftwareMirroringController()->SoftwareMirroringEnabled();
}

const DisplayMode*
DisplayConfigurator::DisplayLayoutManagerImpl::GetUserSelectedMode(
    const DisplaySnapshot& display) const {
  const DisplayMode* selected_mode = nullptr;
  auto* state_controller = GetStateController();
  if (state_controller) {
    ManagedDisplayMode mode;
    const bool mode_found = state_controller->GetSelectedModeForDisplayId(
        display.display_id(), &mode);
    if (display::features::IsListAllDisplayModesEnabled()) {
      // When selecting any arbitrary display mode is enabled, we don't try to
      // be smart about finding the best mode matching the user-selected display
      // size, rather we find an exact match to the selected display mode.
      selected_mode =
          mode_found ? FindExactMatchingMode(display, mode) : nullptr;
    } else {
      selected_mode = mode_found
                          ? FindDisplayModeMatchingSize(display, mode.size())
                          : nullptr;
    }
  }

  // Fall back to native mode.
  return selected_mode ? selected_mode : display.native_mode();
}

bool DisplayConfigurator::DisplayLayoutManagerImpl::AllDisplaysOnSameDevice(
    const std::vector<DisplayState*>& displays) const {
  DisplayState* first_display = displays.front();
  for (auto it = displays.begin() + 1; it != displays.end(); ++it) {
    if (first_display->display->sys_path() != (*it)->display->sys_path())
      return false;
  }
  return true;
}

bool DisplayConfigurator::DisplayLayoutManagerImpl::AllDisplaysHaveDisplayMode(
    const std::vector<DisplayState*>& displays) const {
  for (const auto* display : displays) {
    if (display->display->modes().empty())
      return false;
  }
  return true;
}

bool DisplayConfigurator::DisplayLayoutManagerImpl::
    HasSameAspectRatioAsNativeMode(const DisplaySnapshot* display,
                                   const DisplayMode* mode) const {
  return display->native_mode()->size().width() * mode->size().height() ==
         display->native_mode()->size().height() * mode->size().width();
}

bool DisplayConfigurator::DisplayLayoutManagerImpl::FindExactMatchingMirrorMode(
    const std::vector<DisplayState*>& displays,
    bool preserve_native_aspect_ratio) const {
  DCHECK(displays.size() > 0);

  // Put each display's display modes in |mode_lists| and sort the display modes
  // for each display by size area and refresh rate.
  std::vector<std::vector<const DisplayMode*>> mode_lists;
  for (auto* d : displays) {
    std::vector<const DisplayMode*> mode_list;
    for (auto& mode : d->display->modes()) {
      if (d->display->type() != DISPLAY_CONNECTION_TYPE_INTERNAL &&
          preserve_native_aspect_ratio &&
          !HasSameAspectRatioAsNativeMode(d->display, mode.get())) {
        // Only preserve aspect ratio for external displays.
        continue;
      }
      mode_list.emplace_back(mode.get());
    }
    std::sort(
        mode_list.begin(), mode_list.end(),
        [](const DisplayMode* const& a, const DisplayMode* const& b) -> bool {
          if (a->size().GetArea() > b->size().GetArea())
            return true;
          if (a->size().GetArea() < b->size().GetArea())
            return false;
          return a->refresh_rate() > b->refresh_rate();
        });
    mode_lists.emplace_back(mode_list);
  }

  std::vector<std::vector<const DisplayMode*>::iterator> it_list;
  for (auto& mode_list : mode_lists)
    it_list.emplace_back(mode_list.begin());

  // Find matching display modes among all displays and use them as mirror
  // mirror modes.
  for (; it_list[0] != mode_lists[0].end(); ++it_list[0]) {
    bool found = true;
    for (size_t i = 1; i < mode_lists.size(); ++i) {
      while (it_list[i] != mode_lists[i].end() &&
             (*it_list[i])->size().GetArea() >=
                 (*it_list[0])->size().GetArea()) {
        if ((*it_list[i])->size() == (*it_list[0])->size() &&
            (*it_list[i])->is_interlaced() == (*it_list[0])->is_interlaced()) {
          displays[i]->mirror_mode = *it_list[i];
          break;
        }
        ++it_list[i];
      }
      if (!displays[i]->mirror_mode) {
        found = false;
        break;
      }
    }
    if (found) {
      displays[0]->mirror_mode = *it_list[0];
      return true;
    }
    for (auto* d : displays)
      d->mirror_mode = nullptr;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// DisplayConfigurator implementation

// static
const DisplayMode* DisplayConfigurator::FindDisplayModeMatchingSize(
    const DisplaySnapshot& display,
    const gfx::Size& size) {
  const DisplayMode* best_mode = NULL;
  for (const std::unique_ptr<const DisplayMode>& mode : display.modes()) {
    if (mode->size() != size)
      continue;

    if (mode.get() == display.native_mode()) {
      best_mode = mode.get();
      break;
    }

    if (!best_mode) {
      best_mode = mode.get();
      continue;
    }

    if (mode->is_interlaced()) {
      if (!best_mode->is_interlaced())
        continue;
    } else {
      // Reset the best rate if the non interlaced is
      // found the first time.
      if (best_mode->is_interlaced()) {
        best_mode = mode.get();
        continue;
      }
    }
    if (mode->refresh_rate() < best_mode->refresh_rate())
      continue;

    best_mode = mode.get();
  }

  return best_mode;
}

DisplayConfigurator::DisplayConfigurator()
    : state_controller_(NULL),
      mirroring_controller_(NULL),
      is_panel_fitting_enabled_(false),
      configure_display_(chromeos::IsRunningAsSystemCompositor()),
      current_display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
      current_power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      requested_display_state_(MULTIPLE_DISPLAY_STATE_INVALID),
      pending_power_state_(chromeos::DISPLAY_POWER_ALL_ON),
      has_pending_power_state_(false),
      pending_power_flags_(kSetDisplayPowerNoFlags),
      force_configure_(false),
      display_externally_controlled_(false),
      display_control_changing_(false),
      displays_suspended_(false),
      layout_manager_(new DisplayLayoutManagerImpl(this)),
      content_protection_manager_(new ContentProtectionManager(
          layout_manager_.get(),
          base::BindRepeating(&DisplayConfigurator::configurator_disabled,
                              base::Unretained(this)))),
      has_unassociated_display_(false) {
  AddObserver(content_protection_manager_.get());
}

DisplayConfigurator::~DisplayConfigurator() {
  RemoveObserver(content_protection_manager_.get());

  if (native_display_delegate_)
    native_display_delegate_->RemoveObserver(this);

  CallAndClearInProgressCallbacks(false);
  CallAndClearQueuedCallbacks(false);
}

void DisplayConfigurator::SetDelegateForTesting(
    std::unique_ptr<NativeDisplayDelegate> display_delegate) {
  DCHECK(!native_display_delegate_);

  native_display_delegate_ = std::move(display_delegate);
  configure_display_ = true;
}

void DisplayConfigurator::SetInitialDisplayPower(
    chromeos::DisplayPowerState power_state) {
  if (requested_power_state_) {
    // A new power state has alreday been requested so ignore the initial state.
    return;
  }

  // Set the initial requested power state.
  requested_power_state_ = power_state;

  if (current_display_state_ == MULTIPLE_DISPLAY_STATE_INVALID) {
    // DisplayConfigurator::OnConfigured has not been called yet so just set
    // the current state and notify observers.
    current_power_state_ = power_state;
    NotifyPowerStateObservers();
    return;
  }

  // DisplayConfigurator::OnConfigured has been called so update the current
  // and pending states.
  UpdatePowerState(power_state);
}

void DisplayConfigurator::InitializeDisplayPowerState() {
  SetInitialDisplayPower(chromeos::DISPLAY_POWER_ALL_ON);
}

void DisplayConfigurator::Init(
    std::unique_ptr<NativeDisplayDelegate> display_delegate,
    bool is_panel_fitting_enabled) {
  is_panel_fitting_enabled_ = is_panel_fitting_enabled;
  if (configurator_disabled())
    return;

  // If the delegate is already initialized don't update it (For example, tests
  // set their own delegates).
  if (!native_display_delegate_)
    native_display_delegate_ = std::move(display_delegate);

  native_display_delegate_->AddObserver(this);

  content_protection_manager_->set_native_display_delegate(
      native_display_delegate_.get());
}

void DisplayConfigurator::TakeControl(DisplayControlCallback callback) {
  if (display_control_changing_) {
    std::move(callback).Run(false);
    return;
  }

  if (!display_externally_controlled_) {
    std::move(callback).Run(true);
    return;
  }

  display_control_changing_ = true;
  native_display_delegate_->TakeDisplayControl(
      base::BindOnce(&DisplayConfigurator::OnDisplayControlTaken,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void DisplayConfigurator::OnDisplayControlTaken(DisplayControlCallback callback,
                                                bool success) {
  display_control_changing_ = false;
  display_externally_controlled_ = !success;
  if (success) {
    // Force a configuration since the display configuration may have changed.
    force_configure_ = true;
    if (requested_power_state_) {
      // Restore the requested power state before releasing control.
      SetDisplayPower(*requested_power_state_, kSetDisplayPowerNoFlags,
                      base::DoNothing());
    }
  }

  std::move(callback).Run(success);
}

void DisplayConfigurator::RelinquishControl(DisplayControlCallback callback) {
  if (display_control_changing_) {
    std::move(callback).Run(false);
    return;
  }

  if (display_externally_controlled_) {
    std::move(callback).Run(true);
    return;
  }

  // For simplicity, just fail if in the middle of a display configuration.
  if (configuration_task_) {
    std::move(callback).Run(false);
    return;
  }

  display_control_changing_ = true;

  // Turn off the displays before releasing control since we're no longer using
  // them for output.
  SetDisplayPowerInternal(
      chromeos::DISPLAY_POWER_ALL_OFF, kSetDisplayPowerNoFlags,
      base::BindOnce(&DisplayConfigurator::SendRelinquishDisplayControl,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void DisplayConfigurator::SendRelinquishDisplayControl(
    DisplayControlCallback callback,
    bool success) {
  if (success) {
    // Set the flag early such that an incoming configuration event won't start
    // while we're releasing control of the displays.
    display_externally_controlled_ = true;
    native_display_delegate_->RelinquishDisplayControl(base::BindOnce(
        &DisplayConfigurator::OnDisplayControlRelinquished,
        weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
  } else {
    display_control_changing_ = false;
    std::move(callback).Run(false);
  }
}

void DisplayConfigurator::OnDisplayControlRelinquished(
    DisplayControlCallback callback,
    bool success) {
  display_control_changing_ = false;
  display_externally_controlled_ = success;
  if (!success) {
    force_configure_ = true;
    RunPendingConfiguration();
  }

  std::move(callback).Run(success);
}

void DisplayConfigurator::ForceInitialConfigure() {
  if (configurator_disabled())
    return;

  DCHECK(native_display_delegate_);
  native_display_delegate_->Initialize();

  // ForceInitialConfigure should be the first configuration so there shouldn't
  // be anything scheduled.
  DCHECK(!configuration_task_);

  configuration_task_ = std::make_unique<UpdateDisplayConfigurationTask>(
      native_display_delegate_.get(), layout_manager_.get(),
      requested_display_state_, GetRequestedPowerState(),
      kSetDisplayPowerForceProbe, /*force_configure=*/true,
      base::BindOnce(&DisplayConfigurator::OnConfigured,
                     weak_ptr_factory_.GetWeakPtr()));
  configuration_task_->Run();
}

bool DisplayConfigurator::SetColorMatrix(
    int64_t display_id,
    const std::vector<float>& color_matrix) {
  return RunColorCorrectionClosureSync(
      display_id, cached_displays_,
      !color_matrix.empty() /* reset_color_space_on_success */,
      base::BindOnce(&NativeDisplayDelegate::SetColorMatrix,
                     base::Unretained(native_display_delegate_.get()),
                     display_id, color_matrix));
}

bool DisplayConfigurator::SetGammaCorrection(
    int64_t display_id,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut) {
  const bool reset_color_space_on_success =
      !degamma_lut.empty() || !gamma_lut.empty();
  return RunColorCorrectionClosureSync(
      display_id, cached_displays_, reset_color_space_on_success,
      base::BindOnce(&NativeDisplayDelegate::SetGammaCorrection,
                     base::Unretained(native_display_delegate_.get()),
                     display_id, degamma_lut, gamma_lut));
}

chromeos::DisplayPowerState DisplayConfigurator::GetRequestedPowerState()
    const {
  return requested_power_state_.value_or(chromeos::DISPLAY_POWER_ALL_ON);
}

void DisplayConfigurator::PrepareForExit() {
  configure_display_ = false;
}

void DisplayConfigurator::SetDisplayPowerInternal(
    chromeos::DisplayPowerState power_state,
    int flags,
    ConfigurationCallback callback) {
  // Only skip if the current power state is the same and the latest requested
  // power state is the same. If |pending_power_state_ != current_power_state_|
  // then there is a current task pending or the last configuration failed. In
  // either case request a new configuration to make sure the state is
  // consistent with the expectations.
  if (power_state == current_power_state_ &&
      power_state == pending_power_state_ &&
      !(flags & kSetDisplayPowerForceProbe)) {
    std::move(callback).Run(true);
    return;
  }

  pending_power_state_ = power_state;
  has_pending_power_state_ = true;
  pending_power_flags_ = flags;
  queued_configuration_callbacks_.push_back(std::move(callback));

  if (configure_timer_.IsRunning()) {
    // If there is a configuration task scheduled, avoid performing
    // configuration immediately. Instead reset the timer to wait for things to
    // settle.
    configure_timer_.Reset();
    return;
  }

  RunPendingConfiguration();
}

void DisplayConfigurator::SetDisplayPower(
    chromeos::DisplayPowerState power_state,
    int flags,
    ConfigurationCallback callback) {
  if (configurator_disabled()) {
    std::move(callback).Run(false);
    return;
  }

  VLOG(1) << "SetDisplayPower: power_state="
          << DisplayPowerStateToString(power_state) << " flags=" << flags
          << ", configure timer="
          << (configure_timer_.IsRunning() ? "Running" : "Stopped");

  requested_power_state_ = power_state;
  SetDisplayPowerInternal(*requested_power_state_, flags, std::move(callback));
}

void DisplayConfigurator::SetDisplayMode(MultipleDisplayState new_state) {
  if (configurator_disabled())
    return;

  VLOG(1) << "SetDisplayMode: state="
          << MultipleDisplayStateToString(new_state);
  if (current_display_state_ == new_state) {
    // Cancel software mirroring if the state is moving from
    // MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED to
    // MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED.
    if (mirroring_controller_ &&
        new_state == MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED)
      mirroring_controller_->SetSoftwareMirroring(false);
    NotifyDisplayStateObservers(true, new_state);
    return;
  }

  requested_display_state_ = new_state;

  RunPendingConfiguration();
}

void DisplayConfigurator::OnConfigurationChanged() {
  // Don't do anything if the displays are currently suspended.  Instead we will
  // probe and reconfigure the displays if necessary in ResumeDisplays().
  if (displays_suspended_) {
    VLOG(1) << "Displays are currently suspended.  Not attempting to "
            << "reconfigure them.";
    return;
  }

  // Configure displays with |kConfigureDelayMs| delay,
  // so that time-consuming ConfigureDisplays() won't be called multiple times.
  configure_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(kConfigureDelayMs),
                         this, &DisplayConfigurator::ConfigureDisplays);
}

void DisplayConfigurator::OnDisplaySnapshotsInvalidated() {
  VLOG(1) << "Display snapshots invalidated.";
  cached_displays_.clear();
}

void DisplayConfigurator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DisplayConfigurator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DisplayConfigurator::SuspendDisplays(ConfigurationCallback callback) {
  if (configurator_disabled()) {
    std::move(callback).Run(false);
    return;
  }

  displays_suspended_ = true;

  // Stop |configure_timer_| because we will force probe and configure all the
  // displays at resume time anyway.
  configure_timer_.Stop();

  // Turn off the displays for suspend. This way, if we wake up for lucid sleep,
  // the displays will not turn on (all displays should be off for lucid sleep
  // unless explicitly requested by lucid sleep code). Use
  // SetDisplayPowerInternal so requested_power_state_ is maintained.
  SetDisplayPowerInternal(chromeos::DISPLAY_POWER_ALL_OFF,
                          kSetDisplayPowerNoFlags, std::move(callback));
}

void DisplayConfigurator::ResumeDisplays() {
  if (configurator_disabled())
    return;

  displays_suspended_ = false;

  if (current_display_state_ == MULTIPLE_DISPLAY_STATE_MULTI_MIRROR ||
      current_display_state_ == MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED) {
    // When waking up from suspend while being in a multi display mode, we
    // schedule a delayed forced configuration, which will make
    // SetDisplayPowerInternal() avoid performing the configuration immediately.
    // This gives a chance to wait for all displays to be added and detected
    // before configuration is performed, so we won't immediately resize the
    // desktops and the windows on it to fit on a single display.
    configure_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kResumeConfigureMultiDisplayDelayMs),
        this, &DisplayConfigurator::ConfigureDisplays);
  }

  // TODO(crbug.com/794831): Solve the issue of mirror mode on display resume.

  // If requested_power_state_ is ALL_OFF due to idle suspend, powerd will turn
  // the display power on when it enables the backlight.
  if (requested_power_state_) {
    SetDisplayPower(*requested_power_state_, kSetDisplayPowerNoFlags,
                    base::DoNothing());
  }
}

void DisplayConfigurator::ConfigureDisplays() {
  if (configurator_disabled())
    return;

  force_configure_ = true;
  RunPendingConfiguration();
}

void DisplayConfigurator::RunPendingConfiguration() {
  // Configuration task is currently running. Do not start a second
  // configuration.
  if (configuration_task_)
    return;

  if (!ShouldRunConfigurationTask()) {
    LOG(ERROR) << "Called RunPendingConfiguration without any changes"
                  " requested";
    CallAndClearQueuedCallbacks(true);
    return;
  }

  configuration_task_ = std::make_unique<UpdateDisplayConfigurationTask>(
      native_display_delegate_.get(), layout_manager_.get(),
      requested_display_state_, pending_power_state_, pending_power_flags_,
      force_configure_,
      base::BindOnce(&DisplayConfigurator::OnConfigured,
                     weak_ptr_factory_.GetWeakPtr()));

  // Reset the flags before running the task; otherwise it may end up scheduling
  // another configuration.
  force_configure_ = false;
  pending_power_flags_ = kSetDisplayPowerNoFlags;
  has_pending_power_state_ = false;
  requested_display_state_ = MULTIPLE_DISPLAY_STATE_INVALID;

  DCHECK(in_progress_configuration_callbacks_.empty());
  in_progress_configuration_callbacks_.swap(queued_configuration_callbacks_);

  configuration_task_->Run();
}

void DisplayConfigurator::OnConfigured(
    bool success,
    const std::vector<DisplaySnapshot*>& displays,
    const std::vector<DisplaySnapshot*>& unassociated_displays,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state) {
  VLOG(1) << "OnConfigured: success=" << success << " new_display_state="
          << MultipleDisplayStateToString(new_display_state)
          << " new_power_state=" << DisplayPowerStateToString(new_power_state);

  cached_displays_ = displays;
  has_unassociated_display_ = unassociated_displays.size();

  if (success) {
    current_display_state_ = new_display_state;
    UpdatePowerState(new_power_state);
  }

  configuration_task_.reset();
  NotifyDisplayStateObservers(success, new_display_state);
  CallAndClearInProgressCallbacks(success);

  if (success && !configure_timer_.IsRunning() &&
      ShouldRunConfigurationTask()) {
    configure_timer_.Start(FROM_HERE,
                           base::TimeDelta::FromMilliseconds(kConfigureDelayMs),
                           this, &DisplayConfigurator::RunPendingConfiguration);
  } else {
    // If a new configuration task isn't scheduled respond to all queued
    // callbacks (for example if requested state is current state).
    if (!configure_timer_.IsRunning())
      CallAndClearQueuedCallbacks(success);
  }
}

void DisplayConfigurator::UpdatePowerState(
    chromeos::DisplayPowerState new_power_state) {
  chromeos::DisplayPowerState old_power_state = current_power_state_;
  current_power_state_ = new_power_state;
  // If the pending power state hasn't changed then make sure that value gets
  // updated as well since the last requested value may have been dependent on
  // certain conditions (ie: if only the internal monitor was present).
  if (!has_pending_power_state_)
    pending_power_state_ = new_power_state;
  if (old_power_state != current_power_state_)
    NotifyPowerStateObservers();
}

bool DisplayConfigurator::ShouldRunConfigurationTask() const {
  if (force_configure_)
    return true;

  // Schedule if there is a request to change the display state.
  if (requested_display_state_ != current_display_state_ &&
      requested_display_state_ != MULTIPLE_DISPLAY_STATE_INVALID)
    return true;

  // Schedule if there is a request to change the power state.
  if (has_pending_power_state_)
    return true;

  return false;
}

void DisplayConfigurator::CallAndClearInProgressCallbacks(bool success) {
  for (auto& callback : in_progress_configuration_callbacks_)
    std::move(callback).Run(success);

  in_progress_configuration_callbacks_.clear();
}

void DisplayConfigurator::CallAndClearQueuedCallbacks(bool success) {
  for (auto& callback : queued_configuration_callbacks_)
    std::move(callback).Run(success);

  queued_configuration_callbacks_.clear();
}

void DisplayConfigurator::NotifyDisplayStateObservers(
    bool success,
    MultipleDisplayState attempted_state) {
  if (success) {
    for (Observer& observer : observers_)
      observer.OnDisplayModeChanged(cached_displays_);
  } else {
    for (Observer& observer : observers_)
      observer.OnDisplayModeChangeFailed(cached_displays_, attempted_state);
  }
}

void DisplayConfigurator::NotifyPowerStateObservers() {
  for (Observer& observer : observers_)
    observer.OnPowerStateChanged(current_power_state_);
}

bool DisplayConfigurator::IsDisplayOn() const {
  return current_power_state_ != chromeos::DISPLAY_POWER_ALL_OFF;
}

}  // namespace display
