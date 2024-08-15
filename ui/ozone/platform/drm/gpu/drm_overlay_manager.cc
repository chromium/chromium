// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_manager.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_candidates.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

namespace {

// The amount of time during which the manager cannot skip drm testing of
// fullscreen overlays if overlay swap failure handling is enabled.
constexpr base::TimeDelta kDisallowSkipFullscreenOverlaysDRMTestTime =
    base::Hours(2);

// Maximum number of overlay configurations to keep in MRU cache.
constexpr size_t kMaxCacheSize = 100;

// How many times an overlay configuration needs to be requested before sending
// a query to display controller to see if the request will work. The overlay
// configuration will be rejected until a query is sent and response received.
constexpr int kThrottleRequestSize = 3;

// Returns |candidates| but with all NativePixmap pointers removed in order to
// avoid keeping them alive.
std::vector<OverlaySurfaceCandidate> ToCacheKey(
    const std::vector<OverlaySurfaceCandidate>& candidates) {
  std::vector<OverlaySurfaceCandidate> result = candidates;
  for (auto& candidate : result) {
    // Make sure the cache entry does not keep the NativePixmap alive.
    candidate.native_pixmap = nullptr;
  }
  return result;
}

}  // namespace

DrmOverlayManager::DrmOverlayManager(
    bool handle_overlays_swap_failure,
    bool allow_sync_and_real_buffer_page_flip_testing)
    : handle_overlays_swap_failure_(handle_overlays_swap_failure) {
  allow_sync_and_real_buffer_page_flip_testing_ =
      allow_sync_and_real_buffer_page_flip_testing;
  DETACH_FROM_THREAD(thread_checker_);
}

DrmOverlayManager::~DrmOverlayManager() = default;

std::unique_ptr<OverlayCandidatesOzone>
DrmOverlayManager::CreateOverlayCandidates(gfx::AcceleratedWidget widget) {
  return std::make_unique<DrmOverlayCandidates>(this, widget);
}

void DrmOverlayManager::DisplaysConfigured() {
  TRACE_EVENT0("hwoverlays", "DrmOverlayManager::DisplaysConfigured");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  widget_cache_map_.clear();

  for (auto& entry : hardware_capabilities_callbacks_) {
    GetHardwareCapabilities(entry.first, entry.second);
  }
}

void DrmOverlayManager::StartObservingHardwareCapabilities(
    gfx::AcceleratedWidget widget,
    HardwareCapabilitiesCallback receive_callback) {
  GetHardwareCapabilities(widget, receive_callback);
  hardware_capabilities_callbacks_.emplace(widget, std::move(receive_callback));
}

void DrmOverlayManager::StopObservingHardwareCapabilities(
    gfx::AcceleratedWidget widget) {
  hardware_capabilities_callbacks_.erase(widget);
}

void DrmOverlayManager::CheckOverlaySupport(
    std::vector<OverlaySurfaceCandidate>* candidates,
    gfx::AcceleratedWidget widget) {
  TRACE_EVENT0("hwoverlays", "DrmOverlayManager::CheckOverlaySupport");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Check if another display has an overlay requirement, and if so do not
  // allow overlays. Some ChromeOS boards only support one overlay across all
  // displays so we want the overlay to go somewhere that requires it first vs.
  // a display that will just be using it as an optimization.
  if (!widgets_with_required_overlays_.empty() &&
      !widgets_with_required_overlays_.contains(widget)) {
    return;
  }

  struct OverlayReindexZOrder {
    size_t index;
    int plane_z_order;
  };

  std::vector<OverlayReindexZOrder> overlay_reindex;
  for (size_t i = 0; i < candidates->size(); i++) {
    overlay_reindex.emplace_back(
        (OverlayReindexZOrder{i, (*candidates)[i].plane_z_order}));
  }
  // Create a mapping for sorted z order to candidate index.
  std::sort(overlay_reindex.begin(), overlay_reindex.end(),
            [](const auto& a, const auto& b) {
              return a.plane_z_order > b.plane_z_order;
            });

  // Active |display_rect| occluders that have a clip. This list is in z plane
  // order.
  std::vector<gfx::RectF> display_rect_with_clip;
  // List of underlays that have failed by being occluded by an underlay with a
  // clip rect. This list is in |candidate| ordering.
  std::vector<bool> underlay_fail_clip;
  underlay_fail_clip.resize(candidates->size());
  for (auto& reindex : overlay_reindex) {
    const auto& candidate = (*candidates)[reindex.index];

    if (candidate.plane_z_order < 0) {
      for (auto& rect : display_rect_with_clip) {
        if (rect.Intersects(candidate.display_rect)) {
          underlay_fail_clip[reindex.index] = true;
          break;
        }
      }
    }

    if (candidate.plane_z_order < 0 && candidate.clip_rect &&
        !gfx::RectF(*candidate.clip_rect).Contains(candidate.display_rect)) {
      // This underlay has a clip that is incompatible with all future
      // intersecting underlays.
      display_rect_with_clip.emplace_back(candidate.display_rect);
    }
  }

  // Check if we can skip fullscreen overlay drm test in case if it was
  // disallowed some time ago because of a failure.
  if (!allow_skip_fullscreen_overlay_drm_test_ &&
      base::TimeTicks::Now() >= disallow_fullscreen_overlays_end_time_) {
    allow_skip_fullscreen_overlay_drm_test_ = true;
    disallow_fullscreen_overlays_end_time_ = base::TimeTicks();
  }

  std::vector<OverlaySurfaceCandidate> result_candidates;
  bool can_skip_fullscreen_overlay_drm_test = false;
  for (size_t i = 0; i < candidates->size(); i++) {
    auto& candidate = (*candidates)[i];
    bool can_handle =
        !underlay_fail_clip[i] && CanHandleCandidate(candidate, widget);

    // CanHandleCandidate() should never return false if the candidate is
    // the primary plane.
    DCHECK(can_handle || candidate.plane_z_order != 0);

    // Also verify if hardware supports required buffer format. It can happen,
    // that a system doesn't support overlays for certain buffer formats. Thus,
    // doing IPC below to do validation is just waste of resources given |this|
    // is aware of that limitation.
    can_handle &= IsBufferFormatSupported(candidate.format, widget);

    // If we can't handle the candidate in an overlay replace it with default
    // value. The quad might have a non-integer display rect which hits a
    // DCHECK when converting to gfx::Rect in the comparator.
    result_candidates.push_back(can_handle ? candidate
                                           : OverlaySurfaceCandidate());
    result_candidates.back().overlay_handled = can_handle;

    can_skip_fullscreen_overlay_drm_test =
        handle_overlays_swap_failure_ &&
        allow_skip_fullscreen_overlay_drm_test_ &&
        result_candidates.back().overlay_type == gfx::OverlayType::kFullScreen;
  }

  // This is a fast path for the fullscreen overlays, which avoids drm page flip
  // test and relies on a real swap. If swap fails, fullscreen overlays are
  // again drm tested for |kDisallowSkipFullscreenOverlaysDRMTestTime|.
  if (can_skip_fullscreen_overlay_drm_test) {
    // Fullscreen candidates are tested individually. Other candidates are not
    // expected here.
    DCHECK_EQ(1U, candidates->size());
    candidates->front().overlay_handled = true;
    return;
  }

  if (allow_sync_and_real_buffer_page_flip_testing_ &&
      features::IsSynchronousPageFlipTestingEnabled()) {
    std::vector<OverlayStatus> status =
        SendOverlayValidationRequestSync(result_candidates, widget);
    size_t size = candidates->size();
    DCHECK_EQ(size, status.size());
    for (size_t i = 0; i < size; i++) {
      DCHECK(status[i] == OVERLAY_STATUS_ABLE ||
             status[i] == OVERLAY_STATUS_NOT);
      candidates->at(i).overlay_handled = status[i] == OVERLAY_STATUS_ABLE;
    }
    return;
  }

  auto widget_cache_map_it = widget_cache_map_.find(widget);
  if (widget_cache_map_it == widget_cache_map_.end()) {
    widget_cache_map_it =
        widget_cache_map_.emplace(widget, kMaxCacheSize).first;
  }
  OverlayCandidatesListCache& cache = widget_cache_map_it->second;
  std::vector<OverlaySurfaceCandidate> cache_key =
      ToCacheKey(result_candidates);
  auto iter = cache.Get(cache_key);
  if (iter == cache.end()) {
    // We can skip GPU side validation in case all candidates are invalid.
    bool needs_gpu_validation = base::ranges::any_of(
        result_candidates,
        [](OverlaySurfaceCandidate& c) { return c.overlay_handled; });
    OverlayValidationCacheValue value;
    value.status.resize(result_candidates.size(), needs_gpu_validation
                                                      ? OVERLAY_STATUS_PENDING
                                                      : OVERLAY_STATUS_NOT);
    iter = cache.Put(cache_key, std::move(value));
  }

  OverlayValidationCacheValue& value = iter->second;
  if (value.request_num < kThrottleRequestSize) {
    value.request_num++;
  } else if (value.request_num == kThrottleRequestSize) {
    value.request_num++;
    if (value.status.back() == OVERLAY_STATUS_PENDING)
      SendOverlayValidationRequest(result_candidates, widget);
  } else if (value.status.back() != OVERLAY_STATUS_PENDING) {
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

void DrmOverlayManager::RegisterOverlayRequirement(
    gfx::AcceleratedWidget widget,
    bool requires_overlay) {
  if (requires_overlay)
    widgets_with_required_overlays_.insert(widget);
  else
    widgets_with_required_overlays_.erase(widget);
}

void DrmOverlayManager::OnSwapBuffersComplete(gfx::SwapResult swap_result) {
  DCHECK(!in_flight_overlay_types_.empty());
  auto committed_overlay_types = std::move(in_flight_overlay_types_.front());
  in_flight_overlay_types_.pop_front();

  // If it's a fullscreen, it is a single instance.
  const bool is_fullscreen_overlay =
      committed_overlay_types.size() == 1U &&
      committed_overlay_types.front() == gfx::kFullScreen;

  const bool allow_skip_fullscreen_overlay_drm_test_prev =
      allow_skip_fullscreen_overlay_drm_test_;
  if (swap_result == gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED) {
    LOG_IF(FATAL, !handle_overlays_swap_failure_)
        << "Handling non-simple overlays' swap failure requires the "
           "kHandleOverlaysSwapFailure feature to be enabled.";

    if (is_fullscreen_overlay) {
      if (allow_skip_fullscreen_overlay_drm_test_) {
        allow_skip_fullscreen_overlay_drm_test_ = false;
        disallow_fullscreen_overlays_end_time_ =
            base::TimeTicks::Now() + kDisallowSkipFullscreenOverlaysDRMTestTime;
      } else {
        NOTREACHED() << "It's not expected to receive swap failures for "
                        "fullscreen overlays as they are drm tested.";
      }
    } else {
      NOTREACHED() << "Only fullscreen overlays are treated specially.";
    }
  }

  if (is_fullscreen_overlay && handle_overlays_swap_failure_ &&
      allow_skip_fullscreen_overlay_drm_test_prev &&
      (swap_result == gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED ||
       swap_result == gfx::SwapResult::SWAP_ACK)) {
    // Record how many times fullscreen overlays failed if skipping drm test was
    // allowed.
    UMA_HISTOGRAM_BOOLEAN(
        "Compositing.Display.DrmOverlayManager.FullscreenOverlayFailed",
        swap_result == gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED);
  }
}

void DrmOverlayManager::SetSupportedBufferFormats(
    gfx::AcceleratedWidget widget,
    base::flat_set<gfx::BufferFormat> supported_buffer_formats) {
  per_widget_overlay_supported_buffer_formats_.insert_or_assign(
      widget, std::move(supported_buffer_formats));
}

void DrmOverlayManager::OnPromotedOverlayTypes(
    std::vector<gfx::OverlayType> promoted_overlay_types) {
  // Hold promoted overlay types so that it's possible to distinguish swap
  // buffers completion calls.
  in_flight_overlay_types_.push_back(std::move(promoted_overlay_types));
}

bool DrmOverlayManager::CanHandleCandidate(
    const OverlaySurfaceCandidate& candidate,
    gfx::AcceleratedWidget widget) const {
  if (candidate.buffer_size.IsEmpty())
    return false;

  if (!absl::holds_alternative<gfx::OverlayTransform>(candidate.transform) ||
      absl::get<gfx::OverlayTransform>(candidate.transform) ==
          gfx::OVERLAY_TRANSFORM_INVALID) {
    return false;
  }

  // The remaining checks are for ensuring consistency between GL compositing
  // and overlays. If we must use an overlay, then skip the remaining checks.
  if (candidate.requires_overlay)
    return true;

  // Reject candidates that don't fall on a pixel boundary.
  if (!gfx::IsNearestRectWithinDistance(candidate.display_rect, 0.01f)) {
    VLOG(3) << "Overlay Rejected: display_rect="
            << candidate.display_rect.ToString();
    return false;
  }

  // DRM supposedly supports subpixel source crop. However, according to
  // drm_plane_funcs.update_plane, devices which don't support that are
  // free to ignore the fractional part, and every device seems to do that as
  // of 5.4. So reject candidates that require subpixel source crop.
  gfx::RectF crop(candidate.crop_rect);
  crop.Scale(candidate.buffer_size.width(), candidate.buffer_size.height());
  if (!gfx::IsNearestRectWithinDistance(crop, 0.01f)) {
    VLOG(3) << "Overlay Rejected: crop=" << crop.ToString();
    return false;
  }

  if (candidate.plane_z_order >= 0 && candidate.clip_rect &&
      !candidate.clip_rect->Contains(
          gfx::ToNearestRect(candidate.display_rect))) {
    VLOG(3) << "Overlay Rejected: clip_rect=" << candidate.clip_rect->ToString()
            << ", display_rect=" << candidate.display_rect.ToString();
    return false;
  }

  return true;
}

bool DrmOverlayManager::IsBufferFormatSupported(
    gfx::BufferFormat required_overlay_buffer_format,
    gfx::AcceleratedWidget widget) const {
  auto supported_formats_it =
      per_widget_overlay_supported_buffer_formats_.find(widget);
  if (supported_formats_it ==
      per_widget_overlay_supported_buffer_formats_.end()) {
    // Supported formats are unknown.
    return false;
  }

  auto format_it = base::ranges::find_if(
      supported_formats_it->second,
      [required_overlay_buffer_format](const auto& supported_format) {
        return required_overlay_buffer_format == supported_format;
      });
  return format_it != supported_formats_it->second.end();
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
  auto iter = cache.Peek(ToCacheKey(candidates));
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
