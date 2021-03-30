// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/configure_displays_task.h"

#include <cstddef>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "ui/display/manager/display_util.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace display {

namespace {

// Because we do not offer hardware mirroring, the maximal number of external
// displays that can be configured is limited by the number of available CRTCs,
// which is usually three. Since the lifetime of the UMA using this value is one
// year (exp. Nov. 2021), five buckets are more than enough for
// its histogram (between 0 to 4 external monitors).
constexpr int kMaxDisplaysCount = 5;

// Find the next best mode after |display_mode|. If none can be found return
// nullptr.
const DisplayMode* FindNextMode(const DisplaySnapshot& display_state,
                                const DisplayMode* display_mode) {
  // Internal displays are restricted to their native mode. We do not attempt to
  // downgrade their modes upon failure.
  if (display_state.type() == DISPLAY_CONNECTION_TYPE_INTERNAL) {
    return nullptr;
  }

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

void LogIfInvalidRequestForInternalDisplay(
    const DisplayConfigureRequest& request) {
  if (request.display->type() != DISPLAY_CONNECTION_TYPE_INTERNAL)
    return;

  if (request.mode == nullptr)
    return;

  if (request.mode == request.display->native_mode())
    return;

  LOG(ERROR) << "A mode other than the preferred mode was requested for the "
                "internal display: preferred="
             << request.display->native_mode()->ToString()
             << " vs. requested=" << request.mode->ToString()
             << ". Current mode="
             << (request.display->current_mode()
                     ? request.display->current_mode()->ToString()
                     : "nullptr (disabled)")
             << ".";
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

void UpdateResolutionAndRefreshRateUma(const DisplayConfigureRequest& request) {
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

void UpdateAttemptSucceededUma(
    const std::vector<DisplayConfigureRequest>& requests,
    bool display_success) {
  for (const auto& request : requests) {
    const bool internal =
        request.display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    base::UmaHistogramBoolean(
        internal ? "ConfigureDisplays.Internal.Modeset.AttemptSucceeded"
                 : "ConfigureDisplays.External.Modeset.AttemptSucceeded",
        display_success);

    VLOG(2) << "Configured status=" << display_success
            << " display=" << request.display->display_id()
            << " origin=" << request.origin.ToString()
            << " mode=" << (request.mode ? request.mode->ToString() : "null");
  }
}

void UpdateFinalStatusUma(
    const std::vector<RequestAndStatusList>& requests_and_statuses) {
  int mst_external_displays = 0;
  size_t total_external_displays = requests_and_statuses.size();
  for (auto& request_and_status : requests_and_statuses) {
    const DisplayConfigureRequest& request = request_and_status.first;

    // Is this display SST (single-stream vs. MST multi-stream).
    bool sst_display = request.display->base_connector_id() &&
                       request.display->path_topology().empty();
    if (!sst_display)
      mst_external_displays++;

    bool internal = request.display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    if (internal)
      total_external_displays--;

    base::UmaHistogramBoolean(
        internal ? "ConfigureDisplays.Internal.Modeset.FinalStatus"
                 : "ConfigureDisplays.External.Modeset.FinalStatus",
        request_and_status.second);
  }

  base::UmaHistogramExactLinear(
      "ConfigureDisplays.Modeset.TotalExternalDisplaysCount",
      base::checked_cast<int>(total_external_displays), kMaxDisplaysCount);

  base::UmaHistogramExactLinear(
      "ConfigureDisplays.Modeset.MstExternalDisplaysCount",
      mst_external_displays, kMaxDisplaysCount);

  if (total_external_displays > 0) {
    const int mst_displays_percentage =
        100.0 * mst_external_displays / total_external_displays;
    UMA_HISTOGRAM_PERCENTAGE(
        "ConfigureDisplays.Modeset.MstExternalDisplaysPercentage",
        mst_displays_percentage);
  }
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
    LogIfInvalidRequestForInternalDisplay(request);

    config_requests.emplace_back(request.display->display_id(), request.origin,
                                 request.mode);

    UpdateResolutionAndRefreshRateUma(request);
  }

  const auto& on_configured =
      pending_display_group_requests_.empty()
          ? &ConfigureDisplaysTask::OnFirstAttemptConfigured
          : &ConfigureDisplaysTask::OnRetryConfigured;

  delegate_->Configure(
      config_requests,
      base::BindOnce(on_configured, weak_ptr_factory_.GetWeakPtr()));
}

void ConfigureDisplaysTask::OnConfigurationChanged() {}

void ConfigureDisplaysTask::OnDisplaySnapshotsInvalidated() {
  // From now on, don't access |requests_[index]->display|; they're invalid.
  task_status_ = ERROR;
  weak_ptr_factory_.InvalidateWeakPtrs();
  std::move(callback_).Run(task_status_);
}

void ConfigureDisplaysTask::OnFirstAttemptConfigured(bool config_success) {
  UpdateAttemptSucceededUma(requests_, config_success);

  if (!config_success) {
    // Partition |requests_| into smaller groups, update the task's state, and
    // initiate the retry logic. The next time |delegate_|->Configure()
    // terminates OnRetryConfigured() will be executed instead.
    PartitionRequests();
    DCHECK(!pending_display_group_requests_.empty());
    requests_ = pending_display_group_requests_.front();
    task_status_ = PARTIAL_SUCCESS;
    Run();
    return;
  }

  // This code execute only when the first modeset attempt fully succeeds.
  // Update the displays' status and report success.
  for (const auto& request : requests_) {
    request.display->set_current_mode(request.mode);
    request.display->set_origin(request.origin);
    final_requests_status_.emplace_back(std::make_pair(request, true));
  }

  UpdateFinalStatusUma(final_requests_status_);
  std::move(callback_).Run(task_status_);
}

void ConfigureDisplaysTask::OnRetryConfigured(bool config_success) {
  UpdateAttemptSucceededUma(requests_, config_success);

  if (!config_success) {
    // If one of the largest display request can be downgraded, try again.
    // Otherwise this configuration task is a failure.
    if (DowngradeLargestRequestWithAlternativeModes()) {
      Run();
      return;
    } else {
      task_status_ = ERROR;
    }
  }

  // This code executes only when this display group request fully succeeds or
  // fails to modeset. Update the final status of this group.
  for (const auto& request : requests_) {
    final_requests_status_.emplace_back(
        std::make_pair(request, config_success));
    if (config_success) {
      request.display->set_current_mode(request.mode);
      request.display->set_origin(request.origin);
    }
  }

  // Subsequent modeset attempts will be done on the next pending display group,
  // if one exists.
  pending_display_group_requests_.pop();
  requests_.clear();
  if (!pending_display_group_requests_.empty()) {
    requests_ = pending_display_group_requests_.front();
    Run();
    return;
  }

  // No more display groups to retry.
  UpdateFinalStatusUma(final_requests_status_);
  std::move(callback_).Run(task_status_);
}

void ConfigureDisplaysTask::PartitionRequests() {
  pending_display_group_requests_ = PartitionedRequestsQueue();
  base::flat_set<uint64_t> handled_connectors;

  for (size_t i = 0; i < requests_.size(); ++i) {
    uint64_t connector_id = requests_[i].display->base_connector_id();
    if (handled_connectors.find(connector_id) != handled_connectors.end())
      continue;

    std::vector<DisplayConfigureRequest> request_group;
    for (size_t j = i; j < requests_.size(); ++j) {
      if (connector_id == requests_[j].display->base_connector_id())
        request_group.push_back(requests_[j]);
    }

    pending_display_group_requests_.push(request_group);
    handled_connectors.insert(connector_id);
  }
}

bool ConfigureDisplaysTask::DowngradeLargestRequestWithAlternativeModes() {
  auto cmp = [](DisplayConfigureRequest* lhs, DisplayConfigureRequest* rhs) {
    return *lhs->mode < *rhs->mode;
  };
  std::priority_queue<DisplayConfigureRequest*,
                      std::vector<DisplayConfigureRequest*>, decltype(cmp)>
      sorted_requests(cmp);

  for (auto& request : requests_) {
    if (request.display->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
      continue;

    if (!request.mode)
      continue;

    sorted_requests.push(&request);
  }

  // Fail if there are no viable candidates to downgrade
  if (sorted_requests.empty())
    return false;

  while (!sorted_requests.empty()) {
    DisplayConfigureRequest* next_request = sorted_requests.top();
    sorted_requests.pop();

    const DisplayMode* next_mode =
        FindNextMode(*next_request->display, next_request->mode);
    if (next_mode) {
      next_request->mode = next_mode;
      return true;
    }
  }

  return false;
}

}  // namespace display
