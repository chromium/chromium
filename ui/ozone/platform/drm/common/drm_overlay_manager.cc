// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_overlay_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/drm/common/drm_overlay_candidates.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {
namespace {

// Maximum number of overlay configurations to keep in MRU cache.
constexpr size_t kMaxCacheSize = 10;

// How many times an overlay configuration needs to be requested before sending
// a query to display controller to see if the request will work. The overlay
// configuration will be rejected until a query is sent and response received.
constexpr int kThrottleRequestSize = 3;

}  // namespace

DrmOverlayManager::DrmOverlayManager() {
  DETACH_FROM_THREAD(thread_checker_);
}

DrmOverlayManager::~DrmOverlayManager() = default;

std::unique_ptr<OverlayCandidatesOzone>
DrmOverlayManager::CreateOverlayCandidates(gfx::AcceleratedWidget widget) {
  return std::make_unique<DrmOverlayCandidates>(this, widget);
}

void DrmOverlayManager::ResetCache() {
  TRACE_EVENT0("hwoverlays", "DrmOverlayManager::ResetCache");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  widget_cache_map_.clear();
}

void DrmOverlayManager::CheckOverlaySupport(
    std::vector<OverlaySurfaceCandidate>* candidates,
    gfx::AcceleratedWidget widget) {
  TRACE_EVENT0("hwoverlays", "DrmOverlayManager::CheckOverlaySupport");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<OverlaySurfaceCandidate> result_candidates;
  for (auto& candidate : *candidates) {
    bool can_handle = CanHandleCandidate(candidate, widget);

    // CanHandleCandidate() should never return false if the candidate is
    // the primary plane.
    DCHECK(can_handle || candidate.plane_z_order != 0);

    // If we can't handle the candidate in an overlay replace it with default
    // value. The quad might have a non-integer display rect which hits a
    // DCHECK when converting to gfx::Rect in the comparator.
    result_candidates.push_back(can_handle ? candidate
                                           : OverlaySurfaceCandidate());
    result_candidates.back().overlay_handled = can_handle;
  }

  auto widget_cache_map_it = widget_cache_map_.find(widget);
  if (widget_cache_map_it == widget_cache_map_.end()) {
    widget_cache_map_it =
        widget_cache_map_.emplace(widget, kMaxCacheSize).first;
  }
  OverlayCandidatesListCache& cache = widget_cache_map_it->second;
  auto iter = cache.Get(result_candidates);
  if (iter == cache.end()) {
    // We can skip GPU side validation in case all candidates are invalid.
    bool needs_gpu_validation = std::any_of(
        result_candidates.begin(), result_candidates.end(),
        [](OverlaySurfaceCandidate& c) { return c.overlay_handled; });
    OverlayValidationCacheValue value;
    value.status.resize(result_candidates.size(), needs_gpu_validation
                                                      ? OVERLAY_STATUS_PENDING
                                                      : OVERLAY_STATUS_NOT);
    iter = cache.Put(result_candidates, std::move(value));
  }

  OverlayValidationCacheValue& value = iter->second;
  if (value.request_num < kThrottleRequestSize) {
    value.request_num++;
  } else if (value.request_num == kThrottleRequestSize) {
    value.request_num++;
    if (value.status.back() == OVERLAY_STATUS_PENDING)
      SendOverlayValidationRequest(result_candidates, widget);
  } else {
    // We haven't received an answer yet.
    if (value.status.back() == OVERLAY_STATUS_PENDING)
      return;

    size_t size = candidates->size();
    const std::vector<OverlayStatus>& status = value.status;
    DCHECK_EQ(size, status.size());
    for (size_t i = 0; i < size; i++) {
      DCHECK(status[i] == OVERLAY_STATUS_ABLE ||
             status[i] == OVERLAY_STATUS_NOT);
      candidates->at(i).overlay_handled = status[i] == OVERLAY_STATUS_ABLE;
    }
  }
}

bool DrmOverlayManager::CanHandleCandidate(
    const OverlaySurfaceCandidate& candidate,
    gfx::AcceleratedWidget widget) const {
  if (candidate.buffer_size.IsEmpty())
    return false;

  if (candidate.transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;

  // Reject candidates that don't fall on a pixel boundary.
  if (!gfx::IsNearestRectWithinDistance(candidate.display_rect, 0.01f))
    return false;

  if (candidate.is_clipped && !candidate.clip_rect.Contains(
                                  gfx::ToNearestRect(candidate.display_rect))) {
    return false;
  }

  return true;
}

void DrmOverlayManager::UpdateCacheForOverlayCandidates(
    const std::vector<OverlaySurfaceCandidate>& candidates,
    const gfx::AcceleratedWidget widget,
    const std::vector<OverlayStatus>& status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto widget_cache_map_it = widget_cache_map_.find(widget);
  if (widget_cache_map_it == widget_cache_map_.end())
    return;

  OverlayCandidatesListCache& cache = widget_cache_map_it->second;
  auto iter = cache.Peek(candidates);
  if (iter != cache.end())
    iter->second.status = status;
}

DrmOverlayManager::OverlayValidationCacheValue::OverlayValidationCacheValue() =
    default;
DrmOverlayManager::OverlayValidationCacheValue::OverlayValidationCacheValue(
    OverlayValidationCacheValue&&) = default;
DrmOverlayManager::OverlayValidationCacheValue::~OverlayValidationCacheValue() =
    default;
DrmOverlayManager::OverlayValidationCacheValue&
DrmOverlayManager::OverlayValidationCacheValue::operator=(
    OverlayValidationCacheValue&& other) = default;

}  // namespace ui
