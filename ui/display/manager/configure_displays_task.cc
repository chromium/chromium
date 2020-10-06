// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/configure_displays_task.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

namespace {

// Find the next best mode after |display_mode|. If none can be found return
// nullptr.
const DisplayMode* FindNextMode(const DisplaySnapshot& display_state,
                                const DisplayMode* display_mode) {
  if (!display_mode)
    return nullptr;

  int best_mode_pixels = 0;
  const DisplayMode* best_mode = nullptr;
  int current_mode_pixels = display_mode->size().GetArea();
  for (const std::unique_ptr<const DisplayMode>& mode : display_state.modes()) {
    int pixel_count = mode->size().GetArea();
    if (pixel_count < current_mode_pixels && pixel_count > best_mode_pixels) {
      best_mode = mode.get();
      best_mode_pixels = pixel_count;
    }
  }

  return best_mode;
}

// Samples used to define buckets used by DisplayResolution enum.
// The enum is used to record screen resolution statistics.
const int32_t kDisplayResolutionSamples[] = {1024, 1280, 1440, 1920,
                                             2560, 3840, 5120, 7680};

// Computes the index of the enum DisplayResolution.
// The index has to match the definition of the enum in enums.xml
int ComputeDisplayResolutionEnum(const DisplayMode* mode) {
  if (!mode)
    return 0;  // Display is powered off

  const gfx::Size size = mode->size();
  uint32_t width_idx = 0;
  uint32_t height_idx = 0;
  for (; width_idx < base::size(kDisplayResolutionSamples); width_idx++) {
    if (size.width() <= kDisplayResolutionSamples[width_idx])
      break;
  }
  for (; height_idx < base::size(kDisplayResolutionSamples); height_idx++) {
    if (size.height() <= kDisplayResolutionSamples[height_idx])
      break;
  }

  if (width_idx == base::size(kDisplayResolutionSamples) ||
      height_idx == base::size(kDisplayResolutionSamples))
    return base::size(kDisplayResolutionSamples) *
               base::size(kDisplayResolutionSamples) +
           1;  // Overflow bucket
  // Computes the index of DisplayResolution, starting from 1, since 0 is used
  // when powering off the display.
  return width_idx * base::size(kDisplayResolutionSamples) + height_idx + 1;
}

std::__wrap_iter<const DisplayConfigureRequest*> GetRequestForDisplayId(
    int64_t display_id,
    const std::vector<DisplayConfigureRequest>& requests) {
  return find_if(requests.begin(), requests.end(),
                 [display_id](const DisplayConfigureRequest& request) {
                   return request.display->display_id() == display_id;
                 });
}

}  // namespace

DisplayConfigureRequest::DisplayConfigureRequest(DisplaySnapshot* display,
                                                 const DisplayMode* mode,
                                                 const gfx::Point& origin)
    : display(display), mode(mode), origin(origin) {}

ConfigureDisplaysTask::ConfigureDisplaysTask(
    NativeDisplayDelegate* delegate,
    const std::vector<DisplayConfigureRequest>& requests,
    ResponseCallback callback)
    : delegate_(delegate),
      requests_(requests),
      callback_(std::move(callback)),
      task_status_(SUCCESS) {
  delegate_->AddObserver(this);
}

ConfigureDisplaysTask::~ConfigureDisplaysTask() {
  delegate_->RemoveObserver(this);
}

void ConfigureDisplaysTask::Run() {
  DCHECK(!requests_.empty());

  std::vector<display::DisplayConfigurationParams> config_requests;
  for (const auto& request : requests_) {
    config_requests.emplace_back(request.display->display_id(), request.origin,
                                 request.mode);

    const bool internal =
        request.display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    base::UmaHistogramExactLinear(
        internal ? "ConfigureDisplays.Internal.Modeset.Resolution"
                 : "ConfigureDisplays.External.Modeset.Resolution",
        ComputeDisplayResolutionEnum(request.mode),
        base::size(kDisplayResolutionSamples) *
                base::size(kDisplayResolutionSamples) +
            2);
    base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
        internal ? "ConfigureDisplays.Internal.Modeset.RefreshRate"
                 : "ConfigureDisplays.External.Modeset.RefreshRate",
        1, 240, 18, base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(request.mode ? std::round(request.mode->refresh_rate()) : 0);
  }

  delegate_->Configure(config_requests,
                       base::BindOnce(&ConfigureDisplaysTask::OnConfigured,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void ConfigureDisplaysTask::OnConfigurationChanged() {}

void ConfigureDisplaysTask::OnDisplaySnapshotsInvalidated() {
  // From now on, don't access |requests_[index]->display|; they're invalid.
  task_status_ = ERROR;
  weak_ptr_factory_.InvalidateWeakPtrs();
  std::move(callback_).Run(task_status_);
}

void ConfigureDisplaysTask::OnConfigured(
    const base::flat_map<int64_t, bool>& statuses) {
  bool config_success = true;
  // Check if all displays are successfully configured.
  for (const auto& status : statuses) {
    int64_t display_id = status.first;
    bool display_success = status.second;
    config_success &= display_success;

    auto request = GetRequestForDisplayId(display_id, requests_);
    DCHECK(request != requests_.end());

    VLOG(2) << "Configured status=" << display_success
            << " display=" << request->display->display_id()
            << " origin=" << request->origin.ToString()
            << " mode=" << (request->mode ? request->mode->ToString() : "null");

    bool internal =
        request->display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    base::UmaHistogramBoolean(
        internal ? "ConfigureDisplays.Internal.Modeset.AttemptSucceeded"
                 : "ConfigureDisplays.External.Modeset.AttemptSucceeded",
        display_success);
  }

  // Update displays upon success or prep |requests_| for reconfiguration.
  if (config_success) {
    for (auto& request : requests_) {
      request.display->set_current_mode(request.mode);
      request.display->set_origin(request.origin);
    }
  } else {
    bool should_reconfigure = false;
    // For the failing config, check if there is another mode to be requested.
    // If there is one, attempt to reconfigure everything again.
    for (const auto& status : statuses) {
      int64_t display_id = status.first;
      bool display_success = status.second;
      if (!display_success) {
        const DisplayConfigureRequest* request =
            GetRequestForDisplayId(display_id, requests_).base();
        const DisplayMode* next_mode =
            FindNextMode(*request->display, request->mode);
        if (next_mode) {
          const_cast<DisplayConfigureRequest*>(request)->mode = next_mode;
          should_reconfigure = true;
        }
      }
    }
    if (should_reconfigure) {
      task_status_ = PARTIAL_SUCCESS;
      Run();
      return;
    }
  }

  // Update the final state.
  for (auto& request : requests_) {
    bool internal = request.display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    base::UmaHistogramBoolean(
        internal ? "ConfigureDisplays.Internal.Modeset.FinalStatus"
                 : "ConfigureDisplays.External.Modeset.FinalStatus",
        config_success);
  }

  if (!config_success)
    task_status_ = ERROR;
  std::move(callback_).Run(task_status_);
}

}  // namespace display
