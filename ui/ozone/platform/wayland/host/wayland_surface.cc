// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include <alpha-compositing-unstable-v1-client-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <overlay-prioritizer-client-protocol.h>
#include <surface-augmenter-client-protocol.h>
#include <viewporter-client-protocol.h>
#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/overlay_prioritizer.h"
#include "ui/ozone/platform/wayland/host/surface_augmenter.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {

uint32_t TranslatePriority(gfx::OverlayPriorityHint priority_hint) {
  uint32_t priority = OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_NONE;
  switch (priority_hint) {
    case gfx::OverlayPriorityHint::kNone:
      priority = OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_NONE;
      break;
    case gfx::OverlayPriorityHint::kRegular:
      priority = OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_REGULAR;
      break;
    case gfx::OverlayPriorityHint::kLowLatencyCanvas:
      priority =
          OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_PREFERRED_LOW_LATENCY_CANVAS;
      break;
    case gfx::OverlayPriorityHint::kHardwareProtection:
      priority =
          OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_REQUIRED_HARDWARE_PROTECTION;
      break;
  }
  return priority;
}

}  // namespace

WaylandSurface::ExplicitReleaseInfo::ExplicitReleaseInfo(
    wl::Object<zwp_linux_buffer_release_v1>&& linux_buffer_release,
    wl_buffer* buffer)
    : linux_buffer_release(std::move(linux_buffer_release)), buffer(buffer) {}

WaylandSurface::ExplicitReleaseInfo::~ExplicitReleaseInfo() = default;

WaylandSurface::ExplicitReleaseInfo::ExplicitReleaseInfo(
    ExplicitReleaseInfo&&) = default;

WaylandSurface::ExplicitReleaseInfo&
WaylandSurface::ExplicitReleaseInfo::operator=(ExplicitReleaseInfo&&) = default;

WaylandSurface::WaylandSurface(WaylandConnection* connection,
                               WaylandWindow* root_window)
    : connection_(connection),
      root_window_(root_window),
      surface_(connection->CreateSurface()) {}

WaylandSurface::~WaylandSurface() {
  if (explicit_release_callback_.is_null())
    return;
  for (auto& release : linux_buffer_releases_) {
    explicit_release_callback_.Run(release.second.buffer, base::ScopedFD());
  }
}

uint32_t WaylandSurface::GetSurfaceId() const {
  if (!surface_)
    return 0u;
  return surface_.id();
}

gfx::AcceleratedWidget WaylandSurface::GetWidget() const {
  return root_window_ ? root_window_->GetWidget() : gfx::kNullAcceleratedWidget;
}

bool WaylandSurface::Initialize() {
  if (!surface_)
    return false;

  static struct wl_surface_listener surface_listener = {
      &WaylandSurface::Enter,
      &WaylandSurface::Leave,
  };
  wl_surface_add_listener(surface_.get(), &surface_listener, this);

  if (connection_->viewporter()) {
    viewport_.reset(
        wp_viewporter_get_viewport(connection_->viewporter(), surface()));
    if (!viewport_) {
      LOG(ERROR) << "Failed to create wp_viewport";
      return false;
    }
  } else {
    LOG(WARNING) << "Server doesn't support wp_viewporter.";
  }

  if (connection_->alpha_compositing()) {
    blending_.reset(zcr_alpha_compositing_v1_get_blending(
        connection_->alpha_compositing(), surface()));
    if (!blending_) {
      LOG(ERROR) << "Failed to create zcr_blending_v1";
      return false;
    }
  } else {
    LOG(WARNING) << "Server doesn't support zcr_alpha_compositing_v1.";
  }

  if (auto* overlay_prioritizer = connection_->overlay_prioritizer()) {
    overlay_priority_surface_ =
        overlay_prioritizer->CreateOverlayPrioritizedSurface(surface());
    if (!overlay_priority_surface_) {
      LOG(ERROR) << "Failed to create overlay_priority_surface";
      return false;
    }
  } else {
    LOG(WARNING) << "Server doesn't support overlay_prioritizer.";
  }

  if (auto* surface_augmenter = connection_->surface_augmenter()) {
    augmented_surface_ = surface_augmenter->CreateAugmentedSurface(surface());
    if (!augmented_surface_) {
      LOG(ERROR) << "Failed to create augmented_surface.";
      return false;
    }
  } else {
    LOG(WARNING) << "Server doesn't support surface_augmenter.";
  }

  return true;
}

void WaylandSurface::UnsetRootWindow() {
  DCHECK(surface_);
  root_window_ = nullptr;
}

void WaylandSurface::SetAcquireFence(gfx::GpuFenceHandle acquire_fence) {
  // WaylandBufferManagerGPU knows if the synchronization is not available and
  // must disallow clients to use explicit synchronization.
  DCHECK(!apply_state_immediately_);
  DCHECK(connection_->linux_explicit_synchronization_v1());
  pending_state_.acquire_fence = std::move(acquire_fence);
  return;
}

bool WaylandSurface::AttachBuffer(WaylandBufferHandle* buffer_handle) {
  DCHECK(!apply_state_immediately_);
  if (!buffer_handle) {
    pending_state_.buffer = nullptr;
    pending_state_.buffer_id = 0;
    return false;
  }

  pending_state_.buffer_size_px = buffer_handle->size();
  pending_state_.buffer = buffer_handle->wl_buffer();
  pending_state_.buffer_id = buffer_handle->id();

  if (state_.buffer_id == pending_state_.buffer_id &&
      buffer_handle->released()) {
    state_.buffer = nullptr;
    state_.buffer_id = 0;
  }
  // Compare buffer_id because it is monotonically increasing. state_.buffer
  // may have been de-allocated.
  return state_.buffer_id != pending_state_.buffer_id;
}

void WaylandSurface::UpdateBufferDamageRegion(const gfx::Rect& damage_px) {
  DCHECK(!apply_state_immediately_);
  pending_state_.damage_px.push_back(damage_px);
}

void WaylandSurface::Commit(bool flush) {
  wl_surface_commit(surface_.get());
  if (flush)
    connection_->ScheduleFlush();
}

void WaylandSurface::SetBufferTransform(gfx::OverlayTransform transform) {
  DCHECK(!apply_state_immediately_);
  DCHECK(transform != gfx::OVERLAY_TRANSFORM_INVALID);
  pending_state_.buffer_transform = transform;
  return;
}

void WaylandSurface::SetSurfaceBufferScale(float scale) {
  if (SurfaceSubmissionInPixelCoordinates())
    return;

  pending_state_.buffer_scale = (scale < 1.0f) ? 1 : static_cast<int>(scale);

  if (apply_state_immediately_) {
    state_.buffer_scale = pending_state_.buffer_scale;
    wl_surface_set_buffer_scale(surface_.get(), state_.buffer_scale);
  }
}

void WaylandSurface::SetOpaqueRegion(const std::vector<gfx::Rect>* region_px) {
  pending_state_.opaque_region_px.clear();
  if (!root_window_)
    return;
  bool is_primary_or_root =
      root_window_->root_surface() == this ||
      (root_window()->primary_subsurface() &&
       root_window()->primary_subsurface()->wayland_surface() == this);
  if (is_primary_or_root && !root_window_->IsOpaqueWindow())
    return;
  if (region_px)
    pending_state_.opaque_region_px = *region_px;

  if (apply_state_immediately_) {
    state_.opaque_region_px.swap(pending_state_.opaque_region_px);
    wl_surface_set_opaque_region(
        surface_.get(),
        pending_state_.opaque_region_px.empty()
            ? nullptr
            : CreateAndAddRegion(pending_state_.opaque_region_px,
                                 pending_state_.buffer_scale)
                  .get());
  }
}

void WaylandSurface::SetInputRegion(const gfx::Rect* region_px) {
  pending_state_.input_region_px.reset();
  if (!root_window_)
    return;
  if (root_window_->root_surface() == this &&
      root_window_->ShouldUseNativeFrame()) {
    return;
  }
  if (region_px)
    pending_state_.input_region_px = *region_px;

  if (apply_state_immediately_) {
    state_.input_region_px = pending_state_.input_region_px;
    wl_surface_set_input_region(
        surface_.get(),
        pending_state_.input_region_px.has_value()
            ? CreateAndAddRegion({pending_state_.input_region_px.value()},
                                 pending_state_.buffer_scale)
                  .get()
            : nullptr);
  }
}

wl::Object<wl_region> WaylandSurface::CreateAndAddRegion(
    const std::vector<gfx::Rect>& region_px,
    int32_t buffer_scale) {
  DCHECK(root_window_);

  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));

  auto window_shape_in_dips = root_window_->GetWindowShape();

  bool surface_submission_in_pixel_coordinates =
      SurfaceSubmissionInPixelCoordinates();
  // Only root_surface and primary_subsurface should use |window_shape_in_dips|.
  // Do not use non empty |window_shape_in_dips| if |region_px| is empty, i.e.
  // this surface is transluscent.
  bool is_primary_or_root =
      root_window_->root_surface() == this ||
      (root_window()->primary_subsurface() &&
       root_window()->primary_subsurface()->wayland_surface() == this);
  bool is_empty =
      std::all_of(region_px.begin(), region_px.end(),
                  [](const gfx::Rect& rect) { return rect.IsEmpty(); });
  if (window_shape_in_dips.has_value() && !is_empty && is_primary_or_root) {
    for (auto& rect : window_shape_in_dips.value()) {
      if (surface_submission_in_pixel_coordinates)
        rect = gfx::ScaleToEnclosingRect(rect, root_window_->window_scale());
      wl_region_add(region.get(), rect.x(), rect.y(), rect.width(),
                    rect.height());
    }
  } else {
    for (const auto& rect_px : region_px) {
      if (surface_submission_in_pixel_coordinates) {
        wl_region_add(region.get(), rect_px.x(), rect_px.y(), rect_px.width(),
                      rect_px.height());
      } else {
        gfx::Rect rect = gfx::ScaleToEnclosingRect(rect_px, 1.f / buffer_scale);
        wl_region_add(region.get(), rect.x(), rect.y(), rect.width(),
                      rect.height());
      }
    }
  }
  return region;
}

zwp_linux_surface_synchronization_v1* WaylandSurface::GetSurfaceSync() {
  // The server needs to support the linux_explicit_synchronization protocol.
  if (!connection_->linux_explicit_synchronization_v1()) {
    NOTIMPLEMENTED_LOG_ONCE();
    return nullptr;
  }

  if (!surface_sync_) {
    surface_sync_.reset(
        zwp_linux_explicit_synchronization_v1_get_synchronization(
            connection_->linux_explicit_synchronization_v1(), surface_.get()));
  }
  return surface_sync_.get();
}

augmented_surface* WaylandSurface::GetAugmentedSurface() {
  return augmented_surface_.get();
}

void WaylandSurface::SetViewportSource(const gfx::RectF& src_rect) {
  DCHECK(!apply_state_immediately_);
  pending_state_.crop =
      src_rect == gfx::RectF{1.f, 1.f} ? gfx::RectF() : src_rect;
}

void WaylandSurface::SetOpacity(const float opacity) {
  DCHECK(!apply_state_immediately_);
  if (blending())
    pending_state_.opacity = opacity;
}

void WaylandSurface::SetBlending(const bool use_blending) {
  DCHECK(!apply_state_immediately_);
  if (blending())
    pending_state_.use_blending = use_blending;
}

void WaylandSurface::SetViewportDestination(const gfx::SizeF& dest_size_px) {
  DCHECK(!apply_state_immediately_);
  pending_state_.viewport_px = dest_size_px;
}

wl::Object<wl_subsurface> WaylandSurface::CreateSubsurface(
    WaylandSurface* parent) {
  DCHECK(parent);
  wl_subcompositor* subcompositor = connection_->subcompositor();
  DCHECK(subcompositor);
  wl::Object<wl_subsurface> subsurface(wl_subcompositor_get_subsurface(
      subcompositor, surface_.get(), parent->surface_.get()));
  return subsurface;
}

void WaylandSurface::ApplyPendingState() {
  DCHECK(!apply_state_immediately_);
  if (pending_state_.buffer_id != state_.buffer_id) {
    // The logic in DamageBuffer currently relies on attachment coordinates of
    // (0, 0). If this changes, then the calculation in DamageBuffer will also
    // need to be updated.
    wl_surface_attach(surface_.get(), pending_state_.buffer, 0, 0);

    // Do not call GetSurfaceSync() if the buffer management doesn't happen with
    // WaylandBufferManagerHost. That is, if Wayland EGL implementation is used,
    // buffers are attached/swapped via eglSwapBuffers, which may internally
    // (depends on the implementation) also create a surface sync. Creating a
    // surface sync in this case is not necessary. Moreover, a Wayland protocol
    // error will be raised as only one surface sync can exist.
    if (pending_state_.buffer) {
      auto* surface_sync = GetSurfaceSync();
      if (surface_sync) {
        if (!pending_state_.acquire_fence.is_null()) {
          zwp_linux_surface_synchronization_v1_set_acquire_fence(
              surface_sync, pending_state_.acquire_fence.owned_fd.get());
        }

        if (!explicit_release_callback_.is_null()) {
          auto* linux_buffer_release =
              zwp_linux_surface_synchronization_v1_get_release(surface_sync);

          static struct zwp_linux_buffer_release_v1_listener release_listener =
              {
                  &WaylandSurface::FencedRelease,
                  &WaylandSurface::ImmediateRelease,
              };
          zwp_linux_buffer_release_v1_add_listener(linux_buffer_release,
                                                   &release_listener, this);

          linux_buffer_releases_.emplace(
              linux_buffer_release,
              ExplicitReleaseInfo(
                  wl::Object<zwp_linux_buffer_release_v1>(linux_buffer_release),
                  pending_state_.buffer));
        }
      }
    }
  }

  if (pending_state_.buffer_transform != state_.buffer_transform) {
    wl_output_transform wl_transform =
        wl::ToWaylandTransform(pending_state_.buffer_transform);
    wl_surface_set_buffer_transform(surface_.get(), wl_transform);
  }

  if (pending_state_.opacity != state_.opacity) {
    DCHECK(blending());
    zcr_blending_v1_set_alpha(blending(),
                              wl_fixed_from_double(pending_state_.opacity));
  }
  if (pending_state_.use_blending != state_.use_blending) {
    DCHECK(blending());
    zcr_blending_v1_set_blending(blending(),
                                 pending_state_.use_blending
                                     ? ZCR_BLENDING_V1_BLENDING_EQUATION_PREMULT
                                     : ZCR_BLENDING_V1_BLENDING_EQUATION_NONE);
  }

  if (pending_state_.priority_hint != state_.priority_hint) {
    DCHECK(overlay_priority_surface());
    overlay_prioritized_surface_set_overlay_priority(
        overlay_priority_surface(),
        TranslatePriority(pending_state_.priority_hint));
  }

  // Don't set input region when use_native_frame is enabled.
  if (pending_state_.input_region_px != state_.input_region_px ||
      pending_state_.buffer_scale != state_.buffer_scale) {
    // Sets input region for input events to allow go through and
    // for the compositor to ignore the parts of the input region that fall
    // outside of the surface.
    wl_surface_set_input_region(
        surface_.get(),
        pending_state_.input_region_px.has_value()
            ? CreateAndAddRegion({pending_state_.input_region_px.value()},
                                 pending_state_.buffer_scale)
                  .get()
            : nullptr);
  }

  // It's important to set opaque region for opaque windows (provides
  // optimization hint for the Wayland compositor).
  if (pending_state_.opaque_region_px != state_.opaque_region_px ||
      pending_state_.buffer_scale != state_.buffer_scale) {
    wl_surface_set_opaque_region(
        surface_.get(),
        pending_state_.opaque_region_px.empty()
            ? nullptr
            : CreateAndAddRegion(pending_state_.opaque_region_px,
                                 pending_state_.buffer_scale)
                  .get());
  }

  if (pending_state_.rounded_clip_bounds != state_.rounded_clip_bounds) {
    DCHECK(GetAugmentedSurface());
    if (augmented_surface_get_version(GetAugmentedSurface()) >=
        AUGMENTED_SURFACE_SET_ROUNDED_CLIP_BOUNDS_SINCE_VERSION) {
      gfx::RRectF rounded_clip_bounds = pending_state_.rounded_clip_bounds;
      gfx::Transform scale_transform;
      scale_transform.Scale(1.f / pending_state_.buffer_scale,
                            1.f / pending_state_.buffer_scale);
      scale_transform.TransformRRectF(&rounded_clip_bounds);

      augmented_surface_set_rounded_clip_bounds(
          GetAugmentedSurface(), rounded_clip_bounds.rect().x(),
          rounded_clip_bounds.rect().y(), rounded_clip_bounds.rect().width(),
          rounded_clip_bounds.rect().height(),
          wl_fixed_from_double(
              rounded_clip_bounds
                  .GetCornerRadii(gfx::RRectF::Corner::kUpperLeft)
                  .x()),
          wl_fixed_from_double(
              rounded_clip_bounds
                  .GetCornerRadii(gfx::RRectF::Corner::kUpperRight)
                  .x()),
          wl_fixed_from_double(
              rounded_clip_bounds
                  .GetCornerRadii(gfx::RRectF::Corner::kLowerRight)
                  .x()),
          wl_fixed_from_double(
              rounded_clip_bounds
                  .GetCornerRadii(gfx::RRectF::Corner::kLowerLeft)
                  .x()));
    }
  }

  // Buffer-local coordinates are in pixels, surface coordinates are in DIP.
  // The coordinate transformations from buffer pixel coordinates up to
  // the surface-local coordinates happen in the following order:
  //   1. buffer_transform (wl_surface.set_buffer_transform)
  //   2. buffer_scale (wl_surface.set_buffer_scale)
  //   3. crop and scale (wp_viewport.set*)
  // Apply buffer_transform (wl_surface.set_buffer_transform).
  gfx::SizeF bounds = wl::ApplyWaylandTransform(
      gfx::SizeF(pending_state_.buffer_size_px),
      wl::ToWaylandTransform(pending_state_.buffer_transform));
  int32_t applying_surface_scale = surface_scale_set_;

  // When viewport_px is set, wp_viewport will scale the surface accordingly.
  // Thus, there is no need to downscale bounds as Wayland compositor
  // understands that.
  if (!pending_state_.viewport_px.IsEmpty() && viewport()) {
    // Unset buffer scale if wp_viewport.destination will be set.
    applying_surface_scale = 1;
  } else {
    applying_surface_scale = pending_state_.buffer_scale;
    bounds = gfx::ScaleSize(bounds, 1.f / pending_state_.buffer_scale);
  }
  if (!SurfaceSubmissionInPixelCoordinates() &&
      surface_scale_set_ != applying_surface_scale) {
    wl_surface_set_buffer_scale(surface_.get(), applying_surface_scale);
    surface_scale_set_ = applying_surface_scale;
  }
  DCHECK_GE(surface_scale_set_, 1);

  gfx::RectF viewport_src_dip;
  wl_fixed_t src_to_set[4] = {wl_fixed_from_int(-1), wl_fixed_from_int(-1),
                              wl_fixed_from_int(-1), wl_fixed_from_int(-1)};
  if (pending_state_.crop.IsEmpty()) {
    viewport_src_dip = gfx::RectF(bounds);
  } else {
    viewport_src_dip =
        gfx::ScaleRect(pending_state_.crop, bounds.width(), bounds.height());
    DCHECK(viewport());
    if (wl_fixed_from_double(viewport_src_dip.width()) == 0 ||
        wl_fixed_from_double(viewport_src_dip.height()) == 0) {
      LOG(ERROR) << "Sending viewport src with width/height zero will result "
                    "in wayland disconnection";
    }
    src_to_set[0] = wl_fixed_from_double(viewport_src_dip.x()),
    src_to_set[1] = wl_fixed_from_double(viewport_src_dip.y());
    src_to_set[2] = wl_fixed_from_double(viewport_src_dip.width());
    src_to_set[3] = wl_fixed_from_double(viewport_src_dip.height());
  }
  // Apply crop (wp_viewport.set_source).
  if (viewport() && !std::equal(std::begin(src_to_set), std::end(src_to_set),
                                std::begin(src_set_))) {
    wp_viewport_set_source(viewport(), src_to_set[0], src_to_set[1],
                           src_to_set[2], src_to_set[3]);
    memcpy(src_set_, src_to_set, 4 * sizeof(*src_to_set));
  }

  gfx::SizeF viewport_dst_dip =
      pending_state_.viewport_px.IsEmpty()
          ? viewport_src_dip.size()
          : gfx::ScaleSize(pending_state_.viewport_px,
                           1.f / pending_state_.buffer_scale);
  float dst_to_set[2] = {-1.f, -1.f};
  if (viewport_dst_dip != viewport_src_dip.size()) {
    dst_to_set[0] = viewport_dst_dip.width();
    dst_to_set[1] = viewport_dst_dip.height();
  }
  // Apply viewport scale (wp_viewport.set_destination).
  if (!std::equal(std::begin(dst_to_set), std::end(dst_to_set),
                  std::begin(dst_set_))) {
    auto* augmented_surface = GetAugmentedSurface();
    if (dst_to_set[0] > 0.f && augmented_surface &&
        connection_->surface_augmenter()->SupportsSubpixelAccuratePosition()) {
      // Subpixel accurate positioning is available since the surface augmenter
      // version 2. Since that version, the augmented surface also supports
      // setting destination with wl_fixed. Verify that with dchecks.
      DCHECK_EQ(AUGMENTED_SURFACE_SET_DESTINATION_SIZE_SINCE_VERSION,
                SURFACE_AUGMENTER_GET_AUGMENTED_SUBSURFACE_SINCE_VERSION);
      DCHECK(augmented_surface_get_version(GetAugmentedSurface()) >=
             AUGMENTED_SURFACE_SET_DESTINATION_SIZE_SINCE_VERSION);
      augmented_surface_set_destination_size(
          augmented_surface, wl_fixed_from_double(viewport_dst_dip.width()),
          wl_fixed_from_double(viewport_dst_dip.height()));
    } else if (viewport()) {
      wp_viewport_set_destination(
          viewport(),
          dst_to_set[0] > 0.f ? base::ClampCeil(viewport_dst_dip.width())
                              : static_cast<int>(dst_to_set[0]),
          dst_to_set[1] > 0.f ? base::ClampCeil(viewport_dst_dip.height())
                              : static_cast<int>(dst_to_set[1]));
    }
    memcpy(dst_set_, dst_to_set, 2 * sizeof(*dst_to_set));
  }

  DCHECK_LE(pending_state_.damage_px.size(), 1u);
  if (pending_state_.damage_px.empty() ||
      pending_state_.damage_px.back().IsEmpty()) {
    pending_state_.damage_px.clear();
    state_ = pending_state_;
    return;
  }

  DCHECK(pending_state_.buffer);
  if (connection_->compositor_version() >=
      WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
    // wl_surface_damage_buffer relies on compositor API version 4. See
    // https://bit.ly/2u00lv6 for details.
    // We don't need to apply any scaling because pending_state_.damage_px is
    // already in buffer coordinates.
    wl_surface_damage_buffer(surface_.get(),
                             pending_state_.damage_px.back().x(),
                             pending_state_.damage_px.back().y(),
                             pending_state_.damage_px.back().width(),
                             pending_state_.damage_px.back().height());
  } else {
    gfx::RectF damage_uv =
        gfx::ScaleRect(gfx::RectF(pending_state_.damage_px.back()),
                       1.0f / pending_state_.buffer_size_px.width(),
                       1.0f / pending_state_.buffer_size_px.height());

    if (!pending_state_.crop.IsEmpty()) {
      damage_uv.Offset(-pending_state_.crop.OffsetFromOrigin());
      damage_uv.Scale(1.0f / pending_state_.crop.width(),
                      1.0f / pending_state_.crop.height());
    }
    damage_uv.Intersect(gfx::RectF(1, 1));

    gfx::RectF damage_uv_transformed = wl::ApplyWaylandTransform(
        damage_uv, gfx::SizeF(1, 1),
        wl::ToWaylandTransform(pending_state_.buffer_transform));

    gfx::RectF damage_float =
        gfx::ScaleRect(damage_uv_transformed, viewport_dst_dip.width(),
                       viewport_dst_dip.height());
    constexpr float kAcceptableSubDipDamageError = 0.001f;
    gfx::Rect damage = gfx::ToEnclosingRectIgnoringError(
        damage_float, kAcceptableSubDipDamageError);
    wl_surface_damage(surface_.get(), damage.x(), damage.y(), damage.width(),
                      damage.height());
  }

  pending_state_.damage_px.clear();
  state_ = pending_state_;
}

void WaylandSurface::SetApplyStateImmediately() {
  apply_state_immediately_ = true;
}

void WaylandSurface::ExplicitRelease(
    struct zwp_linux_buffer_release_v1* linux_buffer_release,
    base::ScopedFD fence) {
  auto iter = linux_buffer_releases_.find(linux_buffer_release);
  DCHECK(iter != linux_buffer_releases_.end());
  DCHECK(iter->second.buffer);
  if (!explicit_release_callback_.is_null())
    explicit_release_callback_.Run(iter->second.buffer, std::move(fence));
  linux_buffer_releases_.erase(iter);
}

WaylandSurface::State::State() = default;

WaylandSurface::State::~State() = default;

WaylandSurface::State& WaylandSurface::State::operator=(
    WaylandSurface::State& other) {
  opaque_region_px = other.opaque_region_px;
  input_region_px = other.input_region_px;
  acquire_fence = std::move(other.acquire_fence);
  buffer_id = other.buffer_id;
  buffer = other.buffer;
  buffer_size_px = other.buffer_size_px;
  buffer_scale = other.buffer_scale;
  buffer_transform = other.buffer_transform;
  crop = other.crop;
  viewport_px = other.viewport_px;
  opacity = other.opacity;
  rounded_clip_bounds = other.rounded_clip_bounds;
  use_blending = other.use_blending;
  priority_hint = other.priority_hint;
  return *this;
}

// static
void WaylandSurface::Enter(void* data,
                           struct wl_surface* wl_surface,
                           struct wl_output* output) {
  auto* const surface = static_cast<WaylandSurface*>(data);
  DCHECK(surface);

  auto* wayland_output =
      static_cast<WaylandOutput*>(wl_output_get_user_data(output));

  DCHECK_NE(surface->connection_->wayland_output_manager()->GetOutput(
                wayland_output->output_id()),
            nullptr);

  surface->entered_outputs_.emplace_back(wayland_output->output_id());

  if (surface->root_window_)
    surface->root_window_->OnEnteredOutput();
}

// static
void WaylandSurface::Leave(void* data,
                           struct wl_surface* wl_surface,
                           struct wl_output* output) {
  auto* const surface = static_cast<WaylandSurface*>(data);
  DCHECK(surface);

  auto* wayland_output =
      static_cast<WaylandOutput*>(wl_output_get_user_data(output));
  surface->RemoveEnteredOutput(wayland_output->output_id());
}

void WaylandSurface::RemoveEnteredOutput(uint32_t output_id) {
  auto entered_outputs_it_ =
      std::find_if(entered_outputs_.begin(), entered_outputs_.end(),
                   [&output_id](uint32_t id) { return id == output_id; });
  if (entered_outputs_it_ == entered_outputs_.end())
    return;

  // In certain use cases, such as switching outputs in the single output
  // configuration, the compositor may move the surface from one output to
  // another one, send wl_surface::leave event to it, but defer sending
  // wl_surface::enter until the user moves or resizes the surface on the new
  // output.
  entered_outputs_.erase(entered_outputs_it_);

  if (root_window_)
    root_window_->OnLeftOutput();
}

void WaylandSurface::SetOverlayPriority(
    gfx::OverlayPriorityHint priority_hint) {
  if (overlay_priority_surface())
    pending_state_.priority_hint = priority_hint;
}

bool WaylandSurface::SurfaceSubmissionInPixelCoordinates() const {
  return connection_->surface_submission_in_pixel_coordinates();
}

void WaylandSurface::SetRoundedClipBounds(
    const gfx::RRectF& rounded_clip_bounds) {
  if (GetAugmentedSurface())
    pending_state_.rounded_clip_bounds = rounded_clip_bounds;
}

// static
void WaylandSurface::FencedRelease(
    void* data,
    struct zwp_linux_buffer_release_v1* linux_buffer_release,
    int32_t fence) {
  auto fd = base::ScopedFD(fence);
  static_cast<WaylandSurface*>(data)->ExplicitRelease(linux_buffer_release,
                                                      std::move(fd));
}

// static
void WaylandSurface::ImmediateRelease(
    void* data,
    struct zwp_linux_buffer_release_v1* linux_buffer_release) {
  static_cast<WaylandSurface*>(data)->ExplicitRelease(linux_buffer_release,
                                                      base::ScopedFD());
}

}  // namespace ui
