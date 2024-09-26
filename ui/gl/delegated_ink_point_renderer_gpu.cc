// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/delegated_ink_point_renderer_gpu.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace gl {

namespace {

// Maximum number of pointer ids that will be considered for drawing. Number is
// chosen arbitrarily, as it seems unlikely that the total number of input
// sources using delegated ink trails would exceed 10, but it can be raised if
// the need arises.
constexpr uint64_t kMaximumNumberOfPointerIds = 10;

// Note that this returns true if the HRESULT is anything other than S_OK,
// meaning that it returns true when an event is traced (because of a
// failure).
bool TraceEventOnFailure(HRESULT hr, const char* name) {
  if (SUCCEEDED(hr)) {
    return false;
  }

  TRACE_EVENT_INSTANT1("delegated_ink_trails", name, TRACE_EVENT_SCOPE_THREAD,
                       "hr", hr);
  return true;
}

}  // namespace

DelegatedInkPointRendererGpu::DelegatedInkPointRendererGpu() = default;
DelegatedInkPointRendererGpu::~DelegatedInkPointRendererGpu() = default;

void DelegatedInkPointRendererGpu::InitMessagePipeline(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  // The remote end of this pipeline exists on a per-tab basis, so if tab A
  // is using the feature and then tab B starts trying to use it, a new
  // PendingReceiver will arrive here while |receiver_| is still bound to the
  // remote in tab A. Because of this possibility, we unconditionally reset
  // |receiver_| before trying to bind anything here. This is fine to do
  // because we only ever need to actively receive points from one tab as only
  // one tab can be in the foreground and actively inking at a time.
  receiver_.reset();

  receiver_.Bind(std::move(pending_receiver));
}

bool DelegatedInkPointRendererGpu::Initialize(
    IDCompositionDevice2* dcomp_device2,
    IDXGISwapChain1* root_swap_chain) {
  if (swap_chain_ == root_swap_chain && delegated_ink_trail_) {
    return true;
  }

  swap_chain_ = root_swap_chain;

  if (!ink_trail_device_ &&
      TraceEventOnFailure(
          dcomp_device2->QueryInterface(IID_PPV_ARGS(&ink_trail_device_)),
          "DelegatedInkPointRendererGpu::Initialize - "
          "DCompDevice2 as InkTrailDevice failed")) {
    return false;
  }

  if (TraceEventOnFailure(
          ink_trail_device_->CreateDelegatedInkTrailForSwapChain(
              root_swap_chain, &delegated_ink_trail_),
          "DelegatedInkPointRendererGpu::Initialize - Failed to create "
          "delegated ink trail.")) {
    return false;
  }

  // Start a new trail if the renderer needs to be (re)initialized.
  force_new_ink_trail_ = true;

  return true;
}

bool DelegatedInkPointRendererGpu::DelegatedInkIsSupported(
    const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device) const {
  // Issues related to the delegated ink trail API, such as flickering in the
  // top 3rd of the screen are addressed in 24H2.
  if (base::win::GetVersion() < base::win::Version::WIN11_24H2) {
    return false;
  }

  Microsoft::WRL::ComPtr<IDCompositionInkTrailDevice> ink_trail_device;
  HRESULT hr = dcomp_device.As(&ink_trail_device);
  return hr == S_OK;
}

uint64_t DelegatedInkPointRendererGpu::GetMaximumNumberOfPointerIdsForTesting()
    const {
  CHECK_IS_TEST();
  return kMaximumNumberOfPointerIds;
}

void DelegatedInkPointRendererGpu::ReportPointsDrawn() {
  const base::TimeTicks now = base::TimeTicks::Now();
  // If there is a point that matches the metadata and the histogram has not yet
  // been fired, then this is the first frame that the metadata point will be
  // painted via the JS API.
  if (metadata_paint_time_.has_value()) {
    base::UmaHistogramCustomTimes(
        "Renderer.DelegatedInkTrail.OS.TimeFromDelegatedInkToApiPaint",
        now - metadata_paint_time_.value(), base::Milliseconds(1),
        base::Seconds(1), 50);
    metadata_paint_time_ = std::nullopt;
  }

  if (points_to_be_drawn_.empty()) {
    return;
  }

  CHECK(metadata_);
  base::TimeTicks most_recent_timestamp = base::TimeTicks::Min();
  for (const auto& point : points_to_be_drawn_) {
    UMA_HISTOGRAM_TIMES("Renderer.DelegatedInkTrail.OS.TimeToDrawPointsMillis",
                        now - point.timestamp());
    most_recent_timestamp = std::max(point.timestamp(), most_recent_timestamp);

    // Update the point's `paint_timestamp` if this is the first time it is
    // being painted so that it can later be compared with the metadata's first
    // paint time.
    DelegatedInkPointRendererGpu::DelegatedInkPointTokenMap& token_map =
        delegated_ink_points_[point.pointer_id()];
    auto trail_point_it = token_map.find(point);
    if (trail_point_it != token_map.end()) {
      gfx::DelegatedInkPoint& trail_point = trail_point_it->first;
      if (!trail_point.paint_timestamp().has_value()) {
        trail_point.set_paint_timestamp(now);
      }
    }
  }

  CHECK_GE(most_recent_timestamp, metadata_->timestamp());
  base::UmaHistogramTimes(
      "Renderer.DelegatedInkTrail.LatencyImprovement.OS.WithoutPrediction",
      most_recent_timestamp - metadata_->timestamp());
  base::UmaHistogramCounts100(
      "Renderer.DelegatedInkTrail.OS.OutstandingPointsToDraw",
      points_to_be_drawn_.size());

  points_to_be_drawn_.clear();
}

void DelegatedInkPointRendererGpu::SetDelegatedInkTrailStartPoint(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  TRACE_EVENT_WITH_FLOW1(
      "delegated_ink_trails",
      "DelegatedInkPointRendererGpu::SetDelegatedInkTrailStartPoint",
      TRACE_ID_GLOBAL(metadata->trace_id()), TRACE_EVENT_FLAG_FLOW_IN,
      "metadata", metadata->ToString());

  DCHECK(delegated_ink_trail_);

  // If certain conditions are met, we can simply erase the trail up to the
  // point matching |metadata|, instead of starting a new trail. These
  // conditions include:
  //  1. A trail has already been started, meaning that |metadata_| and
  //     |pointer_id_| exist.
  //  2. The current |metadata_| and |metadata| both have the same color, so
  //     the color of the trail does not need to change.
  //  3. |force_new_ink_trail_| is false - renderer has not been
  //     re-initialized.
  // (continued below)
  if (!force_new_ink_trail_ && metadata_ &&
      metadata->color() == metadata_->color() && pointer_id_) {
    DelegatedInkPointRendererGpu::DelegatedInkPointTokenMap& token_map =
        delegated_ink_points_[pointer_id_.value()];
    auto point_matching_metadata_it = token_map.find(
        gfx::DelegatedInkPoint(metadata->point(), metadata->timestamp()));
    // (continued from above)
    //  4. We have a DelegatedInkPoint with a timestamp equal to or greater
    //     than |metadata|'s timestamp in the token map associated with
    //     |pointer_id_|.
    // (continued below)
    if (point_matching_metadata_it != token_map.end()) {
      gfx::DelegatedInkPoint point_matching_metadata =
          point_matching_metadata_it->first;
      std::optional<unsigned int> token = point_matching_metadata_it->second;
      // (continued from above)
      //  5. The DelegatedInkPoint retrieved above matches |metadata| - this
      //     means that the timestamp is an exact match and the point is
      //     acceptably close.
      //  6. The token associated with this DelegatedInkPoint is valid,
      //     meaning that the point has previously been drawn as part of a
      //     trail.
      //
      // If all of these conditions are met, then we can attempt to just
      // remove points from the trail instead of starting a new trail
      // altogether.
      if (point_matching_metadata.MatchesDelegatedInkMetadata(metadata.get()) &&
          token) {
        metadata_paint_time_ = point_matching_metadata.paint_timestamp();
        bool remove_trail_points_failed = TraceEventOnFailure(
            delegated_ink_trail_->RemoveTrailPoints(token.value()),
            "DelegatedInkPointRendererGpu::SetDelegatedInkTrailStartPoint - "
            "Failed to remove trail points.");
        if (!remove_trail_points_failed) {
          // Remove all points up to and including the point that matches
          // |metadata|. No need to hold on to the point that matches metadata
          // because we've already added it to AddTrailPoints previously, and
          // the next valid |metadata| is guaranteed to be after it.
          token_map.erase(token_map.begin(),
                          std::next(point_matching_metadata_it));
          // Ensure that points that are being removed from the trail are not
          // being reported as painted in `ReportPointsDrawn()`.
          points_to_be_drawn_.erase(
              std::remove_if(points_to_be_drawn_.begin(),
                             points_to_be_drawn_.end(),
                             [&](const gfx::DelegatedInkPoint& x) {
                               return metadata->timestamp() > x.timestamp();
                             }),
              points_to_be_drawn_.end());
          metadata_ = std::move(metadata);
          return;
        }
      }
    }
  }

  D2D1_COLOR_F d2d1_color;
  const float kMaxRGBAValue = 255.f;
  d2d1_color.a = SkColorGetA(metadata->color()) / kMaxRGBAValue;
  d2d1_color.r = SkColorGetR(metadata->color()) / kMaxRGBAValue;
  d2d1_color.g = SkColorGetG(metadata->color()) / kMaxRGBAValue;
  d2d1_color.b = SkColorGetB(metadata->color()) / kMaxRGBAValue;
  if (TraceEventOnFailure(
          delegated_ink_trail_->StartNewTrail(d2d1_color),
          "DelegatedInkPointRendererGpu::SetDelegatedInkTrailStartPoint - "
          "Failed to start a new trail.")) {
    return;
  }

  points_to_be_drawn_.clear();
  wait_for_new_trail_to_draw_ = false;
  metadata_ = std::move(metadata);
  DrawSavedTrailPoints();
  force_new_ink_trail_ = false;
}

void DelegatedInkPointRendererGpu::StoreDelegatedInkPoint(
    const gfx::DelegatedInkPoint& point) {
  TRACE_EVENT_WITH_FLOW1("delegated_ink_trails",
                         "DelegatedInkPointRendererGpu::StoreDelegatedInkPoint",
                         TRACE_ID_GLOBAL(point.trace_id()),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "point", point.ToString());

  const int32_t pointer_id = point.pointer_id();

  DCHECK(delegated_ink_points_.find(pointer_id) ==
             delegated_ink_points_.end() ||
         delegated_ink_points_[pointer_id].empty() ||
         point.timestamp() >
             delegated_ink_points_[pointer_id].rbegin()->first.timestamp());

  if (metadata_ && point.timestamp() < metadata_->timestamp()) {
    return;
  }

  DelegatedInkPointRendererGpu::DelegatedInkPointTokenMap& token_map =
      delegated_ink_points_[pointer_id];
  // Always save the point so that it can be drawn later. This allows points
  // that arrive before the first metadata makes it here to be drawn after
  // StartNewTrail is called. If we get to the maximum number of points that
  // we will store, start erasing the oldest ones first. This matches what the
  // OS compositor does internally when it hits the max number of points.
  if (token_map.size() == gfx::kMaximumNumberOfDelegatedInkPoints) {
    token_map.erase(token_map.begin());
  }
  token_map.insert({point, std::nullopt});

  EraseExcessPointerIds();

  if (pointer_id_ && pointer_id_.value() == pointer_id) {
    DrawDelegatedInkPoint(point);
  } else if (!pointer_id_ && metadata_) {
    DrawSavedTrailPoints();
  }
}

void DelegatedInkPointRendererGpu::ResetPrediction() {
  // Don't reset |metadata_| here so that RemoveTrailPoints() can continue
  // to be called as the final metadata(s) arrive.
  // TODO(crbug.com/40118757): Start predicting points and reset it here.
  wait_for_new_trail_to_draw_ = true;
}

uint64_t DelegatedInkPointRendererGpu::InkTrailTokenCountForTesting() const {
  CHECK_IS_TEST();
  DCHECK_EQ(delegated_ink_points_.size(), 1u);
  uint64_t valid_tokens = 0u;
  for (const auto& it : delegated_ink_points_.begin()->second) {
    if (it.second) {
      valid_tokens++;
    }
  }
  return valid_tokens;
}

bool DelegatedInkPointRendererGpu::CheckForPointerIdForTesting(
    int32_t pointer_id) const {
  CHECK_IS_TEST();
  return delegated_ink_points_.find(pointer_id) != delegated_ink_points_.end();
}

void DelegatedInkPointRendererGpu::EraseExcessPointerIds() {
  auto token_map_it = delegated_ink_points_.begin();
  while (token_map_it != delegated_ink_points_.end()) {
    const DelegatedInkPointRendererGpu::DelegatedInkPointTokenMap& token_map =
        token_map_it->second;
    if (token_map.empty()) {
      token_map_it = delegated_ink_points_.erase(token_map_it);
    } else {
      token_map_it++;
    }
  }

  // In order to get to the maximum pointer id limit, sometimes one token map
  // will need to be removed even if it has a valid DelegatedInkPoint. In this
  // case, we'll remove the token map that has gone the longest without
  // receiving a new point - similar to a least recently used eviction policy.
  // This would also mean that the token map that would result in the smallest
  // latency improvement if it were drawn is being removed. The only exception
  // to this is if the above heuristic would result in the token map that has
  // a pointer id matching |pointer_id_| being removed. Since |pointer_id_|
  // matches the trail that is currently being drawn to the screen, we prefer
  // to save it so we can try to continue that trail. In this case, just
  // remove the token map with the oldest new point that is not currently
  // being drawn.
  if (delegated_ink_points_.size() > kMaximumNumberOfPointerIds) {
    std::vector<std::pair<base::TimeTicks, int32_t>>
        earliest_time_and_pointer_id;
    for (const auto& it : delegated_ink_points_) {
      earliest_time_and_pointer_id.emplace_back(
          it.second.rbegin()->first.timestamp(), it.first);
    }

    std::sort(earliest_time_and_pointer_id.begin(),
              earliest_time_and_pointer_id.end());

    uint64_t pointer_id_to_remove = 0;
    while (pointer_id_to_remove < earliest_time_and_pointer_id.size() &&
           pointer_id_ &&
           earliest_time_and_pointer_id[pointer_id_to_remove].second ==
               pointer_id_.value()) {
      pointer_id_to_remove++;
    }

    CHECK_LT(pointer_id_to_remove, earliest_time_and_pointer_id.size());
    delegated_ink_points_.erase(
        earliest_time_and_pointer_id[pointer_id_to_remove].second);
  }

  DCHECK_LE(delegated_ink_points_.size(), kMaximumNumberOfPointerIds);
}

std::optional<int32_t> DelegatedInkPointRendererGpu::GetPointerIdForMetadata() {
  if (pointer_id_ && delegated_ink_points_.find(pointer_id_.value()) !=
                         delegated_ink_points_.end()) {
    // Since we remove all DelegatedInkPoints with timestamp before
    // |metadata_|'s before calling GetPointerIdForMetadata(), we know we can
    // just grab the first DelegatedInkPoint matching |pointer_id_|.
    const gfx::DelegatedInkPoint& point_matching_metadata =
        delegated_ink_points_[pointer_id_.value()].begin()->first;
    if (point_matching_metadata.MatchesDelegatedInkMetadata(metadata_.get())) {
      return pointer_id_;
    }
  }

  pointer_id_ = std::nullopt;

  for (auto token_map_it = delegated_ink_points_.begin();
       token_map_it != delegated_ink_points_.end() && !pointer_id_;
       ++token_map_it) {
    int32_t potential_pointer_id = token_map_it->first;
    const DelegatedInkPointRendererGpu::DelegatedInkPointTokenMap& token_map =
        token_map_it->second;
    DCHECK(!token_map.empty());
    if (token_map.begin()->first.MatchesDelegatedInkMetadata(metadata_.get())) {
      DCHECK(!pointer_id_);
      pointer_id_ = potential_pointer_id;
    }
  }

  return pointer_id_;
}

void DelegatedInkPointRendererGpu::DrawSavedTrailPoints() {
  DCHECK(metadata_);
  TRACE_EVENT0("delegated_ink_trails", "DrawSavedTrailPoints");

  // Remove all points that have a timestamp earlier than |metadata_|'s, since
  // we know that we won't need to draw them. This is subtly different than
  // the erasing done in SetDelegatedInkTrailStartPoint() - there the point
  // matching the metadata is removed, here it is not. The reason for this
  // difference is because there we know that it matches |metadata_| and
  // therefore it cannot possibly be drawn again, so it is safe to remove.
  // Here however, we are erasing all points up to, but not including, the
  // first point with a timestamp equal to or greater than |metadata_|'s so
  // that we can then check that point to confirm that it matches |metadata_|
  // before deciding to draw an ink trail. The point could then be erased
  // after it is successfully checked against |metadata_| and drawn, it just
  // isn't for simplicity's sake.
  for (auto& it : delegated_ink_points_) {
    DelegatedInkPointRendererGpu::DelegatedInkPointTokenMap& token_map =
        it.second;
    token_map.erase(token_map.begin(),
                    token_map.lower_bound(gfx::DelegatedInkPoint(
                        metadata_->point(), metadata_->timestamp())));
  }

  EraseExcessPointerIds();

  std::optional<unsigned int> pointer_id = GetPointerIdForMetadata();

  // Now, the very first point must match |metadata_|, and as long as it does
  // we can continue to draw everything else. If at any point something can't
  // or fails to draw though, don't attempt to draw anything after it so that
  // the trail can match the user's actual stroke.
  if (pointer_id && delegated_ink_points_.find(pointer_id.value()) !=
                        delegated_ink_points_.end()) {
    DelegatedInkPointRendererGpu::DelegatedInkPointTokenMap& token_map =
        delegated_ink_points_[pointer_id.value()];
    if (!token_map.empty() &&
        token_map.begin()->first.MatchesDelegatedInkMetadata(metadata_.get())) {
      for (const auto& it : token_map) {
        if (!DrawDelegatedInkPoint(it.first)) {
          break;
        }
      }
    }
  } else {
    TRACE_EVENT_INSTANT0("delegated_ink_trails",
                         "DrawSavedTrailPoints failed - no pointer id",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

std::unique_ptr<DCLayerOverlayParams>
DelegatedInkPointRendererGpu::MakeDelegatedInkOverlay(
    IDCompositionDevice2* dcomp_device2,
    IDXGISwapChain1* root_swap_chain,
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  if (!Initialize(dcomp_device2, root_swap_chain)) {
    return nullptr;
  }
  auto ink_layer = std::make_unique<DCLayerOverlayParams>();
  // Ink trail should be rendered on top of all content.
  ink_layer->z_order = INT_MAX;
  const gfx::Rect presentation_rect =
      gfx::ToEnclosedRect(metadata->presentation_area());
  const gfx::Size presentation_area_enclosed_size =
      gfx::Size(presentation_rect.width(), presentation_rect.height());
  ink_layer->quad_rect = gfx::Rect(presentation_area_enclosed_size);
  ink_layer->content_rect = gfx::RectF(presentation_area_enclosed_size);
  // If (0,0) of a visual is clipped out, it can result in delegated ink not
  // being drawn at all. This is more common when DComp Surfaces are enabled,
  // but doesn't negatively impact things when the swapchain is used, so just
  // offset the visual instead of clipping the top left corner in all cases.
  ink_layer->clip_rect = std::make_optional<gfx::Rect>(
      0, 0, presentation_rect.right(), presentation_rect.bottom());
  ink_layer->transform = gfx::Transform::MakeTranslation(
      metadata->presentation_area().OffsetFromOrigin());
  ink_layer->overlay_image = DCLayerOverlayImage(
      presentation_area_enclosed_size, delegated_ink_trail_);
  SetDelegatedInkTrailStartPoint(std::move(metadata));
  return ink_layer;
}

bool DelegatedInkPointRendererGpu::DrawDelegatedInkPoint(
    const gfx::DelegatedInkPoint& point) {
  // Always wait for a new trail to be started before attempting to draw
  // anything, even if |metadata_| exists.
  if (wait_for_new_trail_to_draw_) {
    return false;
  }

  DCHECK(metadata_);
  if (!metadata_->presentation_area().Contains(point.point())) {
    return false;
  }

  DCHECK(delegated_ink_trail_);

  DCompositionInkTrailPoint ink_point;
  ink_point.radius = metadata_->diameter() / 2.f;
  // In order to account for the visual offset, the point must be offset in
  // the opposite direction.
  ink_point.x = point.point().x() - metadata_->presentation_area().x();
  ink_point.y = point.point().y() - metadata_->presentation_area().y();
  unsigned int token;

  // AddTrailPoints() can accept and draw more than one InkTrailPoint per
  // call. However, all the points get lumped together in the one token then,
  // which means that they can only be removed all together. This may be fine
  // in some scenarios, but in the vast majority of cases we will need to
  // remove one point at a time, so we choose to only add one InkTrailPoint at
  // a time.
  if (TraceEventOnFailure(
          delegated_ink_trail_->AddTrailPoints(&ink_point,
                                               /*inkPointsCount*/ 1, &token),
          "DelegatedInkPointRendererGpu::DrawDelegatedInkPoint - Failed to "
          "add trail point")) {
    // TODO(crbug.com/40118757): Start predicting points.
    return false;
  }

  TRACE_EVENT_WITH_FLOW1("delegated_ink_trails",
                         "DelegatedInkPointRendererGpu::DrawDelegatedInkPoint "
                         "- Point added to trail",
                         TRACE_ID_GLOBAL(point.trace_id()),
                         TRACE_EVENT_FLAG_FLOW_IN, "point", point.ToString());

  if (point.timestamp().IsHighResolution() &&
      point.timestamp().IsConsistentAcrossProcesses()) {
    points_to_be_drawn_.push_back(point);
  }
  delegated_ink_points_[point.pointer_id()][point] = token;
  return true;
}

void DelegatedInkPointRendererGpu::InitializeForTesting(
    IDCompositionDevice2* dcomp_device2) {
  CHECK_IS_TEST();
  Initialize(dcomp_device2, nullptr);
}

}  // namespace gl
