// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/configure_displays_task.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "ui/display/manager/display_util.h"
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
      is_configuring_(false),
      num_displays_configured_(0),
      task_status_(SUCCESS) {
  for (size_t i = 0; i < requests_.size(); ++i)
    pending_request_indexes_.push(i);
  delegate_->AddObserver(this);
}

ConfigureDisplaysTask::~ConfigureDisplaysTask() {
  delegate_->RemoveObserver(this);
}

void ConfigureDisplaysTask::Run() {
  // Synchronous configurators will recursively call Run(). In that case just
  // defer their call to the next iteration in the while-loop. This is done to
  // guard against stack overflows if the display has a large list of broken
  // modes.
  if (is_configuring_)
    return;

  {
    base::AutoReset<bool> recursivity_guard(&is_configuring_, true);
    while (!pending_request_indexes_.empty()) {
      size_t index = pending_request_indexes_.front();
      DisplayConfigureRequest* request = &requests_[index];
      pending_request_indexes_.pop();
      const bool internal =
          request->display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
      base::UmaHistogramExactLinear(
          internal ? "ConfigureDisplays.Internal.Modeset.Resolution"
                   : "ConfigureDisplays.External.Modeset.Resolution",
          ComputeDisplayResolutionEnum(request->mode),
          base::size(kDisplayResolutionSamples) *
                  base::size(kDisplayResolutionSamples) +
              2);

      base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
          internal ? "ConfigureDisplays.Internal.Modeset.RefreshRate"
                   : "ConfigureDisplays.External.Modeset.RefreshRate",
          1, 240, 18, base::HistogramBase::kUmaTargetedHistogramFlag);
      histogram->Add(request->mode ? std::round(request->mode->refresh_rate())
                                   : 0);

      delegate_->Configure(
          *request->display, request->mode, request->origin,
          base::BindOnce(&ConfigureDisplaysTask::OnConfigured,
                         weak_ptr_factory_.GetWeakPtr(), index));
    }
  }

  // Nothing should be modified after the |callback_| is called since the
  // task may be deleted in the callback.
  if (num_displays_configured_ == requests_.size())
    std::move(callback_).Run(task_status_);
}

void ConfigureDisplaysTask::OnConfigurationChanged() {}

void ConfigureDisplaysTask::OnDisplaySnapshotsInvalidated() {
  base::queue<size_t> empty_queue;
  pending_request_indexes_.swap(empty_queue);
  // From now on, don't access |requests_[index]->display|; they're invalid.
  task_status_ = ERROR;
  weak_ptr_factory_.InvalidateWeakPtrs();
  Run();
}

void ConfigureDisplaysTask::OnConfigured(size_t index, bool success) {
  DisplayConfigureRequest* request = &requests_[index];
  VLOG(2) << "Configured status=" << success
          << " display=" << request->display->display_id()
          << " origin=" << request->origin.ToString()
          << " mode=" << (request->mode ? request->mode->ToString() : "null");

  const bool internal =
      request->display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
  base::UmaHistogramBoolean(
      internal ? "ConfigureDisplays.Internal.Modeset.AttemptSucceeded"
               : "ConfigureDisplays.External.Modeset.AttemptSucceeded",
      success);

  if (!success) {
    request->mode = FindNextMode(*request->display, request->mode);
    if (request->mode) {
      pending_request_indexes_.push(index);
      if (task_status_ == SUCCESS)
        task_status_ = PARTIAL_SUCCESS;

      Run();
      return;
    }
  } else {
    request->display->set_current_mode(request->mode);
    request->display->set_origin(request->origin);
  }

  num_displays_configured_++;

  base::UmaHistogramBoolean(
      internal ? "ConfigureDisplays.Internal.Modeset.FinalStatus"
               : "ConfigureDisplays.External.Modeset.FinalStatus",
      success);
  if (!success)
    task_status_ = ERROR;

  Run();
}

}  // namespace display
