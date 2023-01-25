// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include <alpha-compositing-unstable-v1-client-protocol.h>
#include <chrome-color-management-client-protocol.h>
#include <content-type-v1-client-protocol.h>
#include <keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <overlay-prioritizer-client-protocol.h>
#include <surface-augmenter-client-protocol.h>
#include <viewporter-client-protocol.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/overlay_prioritizer.h"
#include "ui/ozone/platform/wayland/host/surface_augmenter.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_manager.h"

namespace ui {

namespace {

uint32_t TranslatePriority(gfx::OverlayPriorityHint priority_hint) {
  uint32_t priority = OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_NONE;
  switch (priority_hint) {
    case gfx::OverlayPriorityHint::kNone:
      priority = OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_NONE;
      break;
    case gfx::OverlayPriorityHint::kRegular:
    case gfx::OverlayPriorityHint::kVideo:
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
    wl_buffer* buffer,
    ExplicitReleaseCallback explicit_release_callback)
    : linux_buffer_release(std::move(linux_buffer_release)),
      buffer(buffer),
      explicit_release_callback(std::move(explicit_release_callback)) {}

WaylandSurface::ExplicitReleaseInfo::~ExplicitReleaseInfo() = default;

WaylandSurface::ExplicitReleaseInfo::ExplicitReleaseInfo(
    ExplicitReleaseInfo&&) = default;

WaylandSurface::ExplicitReleaseInfo&
WaylandSurface::ExplicitReleaseInfo::operator=(ExplicitReleaseInfo&&) = default;

WaylandSurface::WaylandSurface(WaylandConnection* connection,
                               WaylandWindow* root_window)
    : connection_(connection),
      root_window_(root_window),
      surface_(connection->CreateSurface()),
      surface_submission_in_pixel_coordinates_(
          connection->surface_submission_in_pixel_coordinates()) {}

WaylandSurface::~WaylandSurface() {
  for (auto& release : linux_buffer_releases_) {
    DCHECK(release.second.explicit_release_callback);
    std::move(release.second.explicit_release_callback)
        .Run(release.second.buffer.get(), base::ScopedFD());
  }
}

void WaylandSurface::RequestExplicitRelease(ExplicitReleaseCallback callback) {
  DCHECK(!next_explicit_release_request_);
  next_explicit_release_request_ = std::move(callback);
}

gfx::AcceleratedWidget WaylandSurface::get_widget() const {
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
    static bool log_once = false;
    if (!log_once) {
      log_once = true;
      LOG(WARNING) << "Server doesn't support wp_viewporter.";
    }
  }

  if (connection_->alpha_compositing()) {
    blending_.reset(zcr_alpha_compositing_v1_get_blending(
        connection_->alpha_compositing(), surface()));
    if (!blending_) {
      LOG(ERROR) << "Failed to create zcr_blending_v1";
      return false;
    }
  } else {
    static bool log_once = false;
    if (!log_once) {
      log_once = true;
      LOG(WARNING) << "Server doesn't support zcr_alpha_compositing_v1.";
    }
  }

  if (auto* overlay_prioritizer = connection_->overlay_prioritizer()) {
    overlay_priority_surface_ =
        overlay_prioritizer->CreateOverlayPrioritizedSurface(surface());
    if (!overlay_priority_surface_) {
      LOG(ERROR) << "Failed to create overlay_priority_surface";
      return false;
    }
  } else {
    static bool log_once = false;
    if (!log_once) {
      log_once = true;
      LOG(WARNING) << "Server doesn't support overlay_prioritizer.";
    }
  }

  if (auto* surface_augmenter = connection_->surface_augmenter()) {
    augmented_surface_ = surface_augmenter->CreateAugmentedSurface(surface());
    if (!augmented_surface_) {
      LOG(ERROR) << "Failed to create augmented_surface.";
      return false;
    }
  } else {
    static bool log_once = false;
    if (!log_once) {
      log_once = true;
      LOG(WARNING) << "Server doesn't support surface_augmenter.";
    }
  }

  if (auto* content_type_manager = connection_->content_type_manager_v1()) {
    content_type_.reset(wp_content_type_manager_v1_get_surface_content_type(
        content_type_manager, surface()));
    if (!content_type_) {
      LOG(ERROR)
          << "Failed to create wp_content_type_v1. Continuing without it.";
    }
  } else {
    static bool log_once = false;
    if (!log_once) {
      log_once = true;
      LOG(WARNING) << "Server doesn't support wp_content_type_v1";
    }
  }

  if (auto* zcr_color_manager = connection_->zcr_color_manager()) {
    zcr_color_management_surface_ =
        std::make_unique<WaylandZcrColorManagementSurface>(
            zcr_color_manager->CreateColorManagementSurface(surface())
                .release(),
            connection_);
    if (!zcr_color_management_surface_) {
      LOG(ERROR) << "Failed to create zcr_color_management_surface.";
      return false;
    }
    zcr_color_management_surface_->SetDefaultColorSpace();
  } else {
    static bool log_once = false;
    if (!log_once) {
      log_once = true;
      LOG(WARNING) << "Server doesn't support zcr_color_management_surface.";
    }
  }

  return true;
}

void WaylandSurface::UnsetRootWindow() {
  DCHECK(surface_);
  root_window_ = nullptr;
}

void WaylandSurface::set_acquire_fence(gfx::GpuFenceHandle acquire_fence) {
  // WaylandBufferManagerGPU knows if the synchronization is not available and
  // must disallow clients to use explicit synchronization.
  DCHECK(!apply_state_immediately_);
  DCHECK(connection_->linux_explicit_synchronization_v1());
  if (!acquire_fence.is_null()) {
    base::TimeTicks ticks;
    auto status = gfx::GpuFence::GetStatusChangeTime(
        acquire_fence.owned_fd.get(), &ticks);
    if (status == gfx::GpuFence::kSignaled)
      acquire_fence = gfx::GpuFenceHandle();
  }
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
      buffer_handle->released(this)) {
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
    connection_->Flush();
}

void WaylandSurface::set_surface_buffer_scale(float scale) {
  pending_state_.buffer_scale_float = scale;

  if (apply_state_immediately_) {
    state_.buffer_scale_float = pending_state_.buffer_scale_float;
    if (!surface_submission_in_pixel_coordinates_)
      wl_surface_set_buffer_scale(surface_.get(), GetWaylandScale(state_));
    if (root_window_)
      root_window_->PropagateBufferScale(scale);
  }
}

void WaylandSurface::set_opaque_region(
    const std::vector<gfx::Rect>* region_px) {
  pending_state_.opaque_region_px.clear();
  if (!root_window_)
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
                                 GetWaylandScale(pending_state_))
                  .get());
  }
}

void WaylandSurface::set_input_region(const gfx::Rect* region_px) {
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
                                 GetWaylandScale(pending_state_))
                  .get()
            : nullptr);
  }
}

int WaylandSurface::GetWaylandScale(const State& state) {
  if (surface_submission_in_pixel_coordinates_)
    return 1;
  return (state.buffer_scale_float < 1.0f)
             ? 1
             : std::ceil(state.buffer_scale_float);
}

wl::Object<wl_region> WaylandSurface::CreateAndAddRegion(
    const std::vector<gfx::Rect>& region_px,
    int32_t buffer_scale) {
  DCHECK(root_window_);
  DCHECK(!surface_submission_in_pixel_coordinates_ || buffer_scale == 1);

  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));

  for (const auto& rect_px : region_px) {
    gfx::Rect rect = gfx::ScaleToEnclosingRect(rect_px, 1.f / buffer_scale);
    wl_region_add(region.get(), rect.x(), rect.y(), rect.width(),
                  rect.height());
  }
  return region;
}

zwp_linux_surface_synchronization_v1* WaylandSurface::GetOrCreateSurfaceSync() {
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
    // Setting Color Space of surface.
    // Should be called infrequently: only when color space is changing to a
    // a different one.
    if (pending_state_.color_space != state_.color_space) {
      zcr_color_management_surface_->SetColorSpace(pending_state_.color_space);
    }

    // The logic in DamageBuffer currently relies on attachment coordinates of
    // (0, 0). If this changes, then the calculation in DamageBuffer will also
    // need to be updated.
    // Note: should the offset be non-zero, use wl_surface_offset() to set it.
    wl_surface_attach(surface_.get(), pending_state_.buffer, 0, 0);

    // Do not call GetOrCreateSurfaceSync() if the buffer management doesn't
    // happen with WaylandBufferManagerHost. That is, if Wayland EGL
    // implementation is used, buffers are attached/swapped via eglSwapBuffers,
    // which may internally (depends on the implementation) also create a
    // surface sync. Creating a surface sync in this case is not necessary.
    // Moreover, a Wayland protocol error will be raised as only one surface
    // sync can exist.
    if (pending_state_.buffer) {
      auto* surface_sync = GetOrCreateSurfaceSync();
      if (surface_sync) {
        if (!pending_state_.acquire_fence.is_null()) {
          zwp_linux_surface_synchronization_v1_set_acquire_fence(
              surface_sync, pending_state_.acquire_fence.owned_fd.get());
        }

        if (!next_explicit_release_request_.is_null()) {
          auto* linux_buffer_release =
              zwp_linux_surface_synchronization_v1_get_release(surface_sync);
          // This must be very unlikely to happen, but there is a bug for this.
          // Thus, add a check for this object to ensure it's not null. See
          // https://crbug.com/1382976
          LOG_IF(FATAL, !linux_buffer_release)
              << "Unable to get an explicit release object.";

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
                  pending_state_.buffer,
                  std::move(next_explicit_release_request_)));
        }
      }
    }
  }
  pending_state_.acquire_fence = gfx::GpuFenceHandle();

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
      GetWaylandScale(pending_state_) != GetWaylandScale(state_)) {
    // Sets input region for input events to allow go through and
    // for the compositor to ignore the parts of the input region that fall
    // outside of the surface.
    wl_surface_set_input_region(
        surface_.get(),
        pending_state_.input_region_px.has_value()
            ? CreateAndAddRegion({pending_state_.input_region_px.value()},
                                 GetWaylandScale(pending_state_))
                  .get()
            : nullptr);
  }

  // It's important to set opaque region for opaque windows (provides
  // optimization hint for the Wayland compositor).
  if (pending_state_.opaque_region_px != state_.opaque_region_px ||
      GetWaylandScale(pending_state_) != GetWaylandScale(state_)) {
    wl_surface_set_opaque_region(
        surface_.get(),
        pending_state_.opaque_region_px.empty()
            ? nullptr
            : CreateAndAddRegion(pending_state_.opaque_region_px,
                                 GetWaylandScale(pending_state_))
                  .get());
  }

  if (pending_state_.background_color != state_.background_color) {
    DCHECK(get_augmented_surface());
    if (augmented_surface_get_version(get_augmented_surface()) >=
        static_cast<uint32_t>(
            AUGMENTED_SURFACE_SET_BACKGROUND_COLOR_SINCE_VERSION)) {
      wl_array color_data;
      wl_array_init(&color_data);
      if (pending_state_.background_color.has_value())
        wl::SkColorToWlArray(pending_state_.background_color.value(),
                             color_data);

      augmented_surface_set_background_color(get_augmented_surface(),
                                             &color_data);

      wl_array_release(&color_data);
    }
  }

  if (pending_state_.rounded_clip_bounds != state_.rounded_clip_bounds) {
    DCHECK(get_augmented_surface());
    if (augmented_surface_get_version(get_augmented_surface()) >=
        AUGMENTED_SURFACE_SET_ROUNDED_CLIP_BOUNDS_SINCE_VERSION) {
      gfx::RRectF rounded_clip_bounds = pending_state_.rounded_clip_bounds;
      rounded_clip_bounds.Scale(1.f / GetWaylandScale(pending_state_));

      augmented_surface_set_rounded_clip_bounds(
          get_augmented_surface(), rounded_clip_bounds.rect().x(),
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

  if (content_type_ &&
      (pending_state_.contains_video != state_.contains_video)) {
    wp_content_type_v1_set_content_type(content_type_.get(),
                                        pending_state_.contains_video
                                            ? WP_CONTENT_TYPE_V1_TYPE_VIDEO
                                            : WP_CONTENT_TYPE_V1_TYPE_NONE);
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
    applying_surface_scale = GetWaylandScale(pending_state_);
    bounds = gfx::ScaleSize(bounds, 1.f / GetWaylandScale(pending_state_));
  }
  if (!surface_submission_in_pixel_coordinates_ &&
      surface_scale_set_ != applying_surface_scale) {
    wl_surface_set_buffer_scale(surface_.get(), applying_surface_scale);
    surface_scale_set_ = applying_surface_scale;
  }
  DCHECK_GE(surface_scale_set_, 1);

  // If this is not a subsurface, propagate the buffer scale.
  if (root_window_ && root_window_->root_surface() == this)
    root_window_->PropagateBufferScale(pending_state_.buffer_scale_float);

  gfx::RectF viewport_src_dip;
  wl_fixed_t src_to_set[4] = {wl_fixed_from_int(-1), wl_fixed_from_int(-1),
                              wl_fixed_from_int(-1), wl_fixed_from_int(-1)};
  if (pending_state_.crop.IsEmpty()) {
    viewport_src_dip = gfx::RectF(bounds);
  } else {
    // viewport_src_dip needs to be in post-transform coordinates.
    gfx::RectF crop_transformed = wl::ApplyWaylandTransform(
        pending_state_.crop, gfx::SizeF(1, 1),
        wl::ToWaylandTransform(pending_state_.buffer_transform));
    viewport_src_dip =
        gfx::ScaleRect(crop_transformed, bounds.width(), bounds.height());
    DCHECK(viewport());
    if (wl_fixed_from_double(viewport_src_dip.width()) == 0 ||
        wl_fixed_from_double(viewport_src_dip.height()) == 0 ||
        wl_fixed_from_double(viewport_src_dip.x()) < 0 ||
        wl_fixed_from_double(viewport_src_dip.y()) < 0) {
      LOG(ERROR) << "Sending viewport src with width/height zero or negative "
                    "origin will result in wayland disconnection";
      // TODO(crbug.com/1325344): Resolve why this viewport size ends up being
      // zero and remove the fix below.
      LOG(ERROR) << "viewport_src_dip=" << viewport_src_dip.ToString()
                 << " pending_state_.crop=" << pending_state_.crop.ToString()
                 << " bounds=" << bounds.ToString()
                 << "  pending_state_.buffer_size_px="
                 << pending_state_.buffer_size_px.ToString();
      constexpr wl_fixed_t kViewportSizeMin = 1;
      const float kViewPortSizeMinFloat =
          static_cast<float>(wl_fixed_to_double(kViewportSizeMin));
      LOG(ERROR)
          << "Limiting viewport_src_dip size to be non zero with a minium of "
          << kViewportSizeMin;
      viewport_src_dip.set_width(
          std::max(viewport_src_dip.width(), kViewPortSizeMinFloat));
      viewport_src_dip.set_height(
          std::max(viewport_src_dip.height(), kViewPortSizeMinFloat));
      viewport_src_dip.set_x(std::max(viewport_src_dip.x(), 0.f));
      viewport_src_dip.set_y(std::max(viewport_src_dip.y(), 0.f));
    }
    src_to_set[0] = wl_fixed_from_double(viewport_src_dip.x()),
    src_to_set[1] = wl_fixed_from_double(viewport_src_dip.y());
    src_to_set[2] = wl_fixed_from_double(viewport_src_dip.width());
    src_to_set[3] = wl_fixed_from_double(viewport_src_dip.height());
  }
  // Apply crop (wp_viewport.set_source).
  if (viewport() && !base::ranges::equal(src_to_set, src_set_)) {
    wp_viewport_set_source(viewport(), src_to_set[0], src_to_set[1],
                           src_to_set[2], src_to_set[3]);
    memcpy(src_set_, src_to_set, 4 * sizeof(*src_to_set));
  }

  gfx::SizeF viewport_dst_dip =
      pending_state_.viewport_px.IsEmpty()
          ? viewport_src_dip.size()
          : gfx::ScaleSize(pending_state_.viewport_px,
                           1.f / GetWaylandScale(pending_state_));
  float dst_to_set[2] = {-1.f, -1.f};
  if (viewport_dst_dip != viewport_src_dip.size()) {
    dst_to_set[0] = viewport_dst_dip.width();
    dst_to_set[1] = viewport_dst_dip.height();
  }
  // Apply viewport scale (wp_viewport.set_destination).
  if (!base::ranges::equal(dst_to_set, dst_set_)) {
    auto* augmented_surface = get_augmented_surface();
    if (dst_to_set[0] > 0.f && augmented_surface &&
        connection_->surface_augmenter()->SupportsSubpixelAccuratePosition()) {
      // Subpixel accurate positioning is available since the surface augmenter
      // version 2. Since that version, the augmented surface also supports
      // setting destination with wl_fixed. Verify that with dchecks.
      DCHECK_EQ(AUGMENTED_SURFACE_SET_DESTINATION_SIZE_SINCE_VERSION,
                SURFACE_AUGMENTER_GET_AUGMENTED_SUBSURFACE_SINCE_VERSION);
      DCHECK(augmented_surface_get_version(get_augmented_surface()) >=
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
  if (wl::get_version_of_object(surface_.get()) >=
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
      damage_uv.InvScale(pending_state_.crop.width(),
                         pending_state_.crop.height());
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

void WaylandSurface::ForceImmediateStateApplication() {
  apply_state_immediately_ = true;
}

void WaylandSurface::SetKeyboardShortcutsInhibition(bool enabled) {
  if (!enabled) {
    keyboard_shortcuts_inhibitor_.reset();
    return;
  }
  if (auto* manager = connection_->keyboard_shortcuts_inhibit_manager_v1()) {
    keyboard_shortcuts_inhibitor_ =
        wl::Object<zwp_keyboard_shortcuts_inhibitor_v1>(
            zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(
                manager, surface_.get(), connection_->seat()->wl_object()));
  }
}

void WaylandSurface::ExplicitRelease(
    struct zwp_linux_buffer_release_v1* linux_buffer_release,
    base::ScopedFD fence) {
  auto iter = linux_buffer_releases_.find(linux_buffer_release);
  DCHECK(iter != linux_buffer_releases_.end());
  DCHECK(iter->second.buffer);
  std::move(iter->second.explicit_release_callback)
      .Run(iter->second.buffer.get(), std::move(fence));
  linux_buffer_releases_.erase(iter);
}

WaylandSurface::State::State() = default;

WaylandSurface::State::~State() = default;

WaylandSurface::State& WaylandSurface::State::operator=(
    const WaylandSurface::State& other) {
  damage_px = other.damage_px;
  opaque_region_px = other.opaque_region_px;
  input_region_px = other.input_region_px;
  color_space = other.color_space;
  buffer_id = other.buffer_id;
  buffer = other.buffer;
  buffer_size_px = other.buffer_size_px;
  buffer_scale_float = other.buffer_scale_float;
  buffer_transform = other.buffer_transform;
  crop = other.crop;
  viewport_px = other.viewport_px;
  opacity = other.opacity;
  use_blending = other.use_blending;
  rounded_clip_bounds = other.rounded_clip_bounds;
  priority_hint = other.priority_hint;
  background_color = other.background_color;
  contains_video = other.contains_video;
  return *this;
}

// static
void WaylandSurface::Enter(void* data,
                           struct wl_surface* wl_surface,
                           struct wl_output* output) {
  auto* const surface = static_cast<WaylandSurface*>(data);
  DCHECK(surface);

  // The compositor can send a null output.
  // crbug.com/1332540
  if (!output) {
    LOG(ERROR) << "NULL output received, cannot enter it!";
    return;
  }

  auto* wayland_output =
      static_cast<WaylandOutput*>(wl_output_get_user_data(output));

  DCHECK_NE(surface->connection_->wayland_output_manager()->GetOutput(
                wayland_output->output_id()),
            nullptr);

  if (auto it = base::ranges::find(surface->entered_outputs_,
                                   wayland_output->output_id());
      it == surface->entered_outputs_.end()) {
    surface->entered_outputs_.emplace_back(wayland_output->output_id());
  }

  if (surface->root_window_)
    surface->root_window_->OnEnteredOutput();
}

// static
void WaylandSurface::Leave(void* data,
                           struct wl_surface* wl_surface,
                           struct wl_output* output) {
  auto* const surface = static_cast<WaylandSurface*>(data);
  DCHECK(surface);

  // The compositor can send a null output.
  // crbug.com/1332540
  if (!output) {
    LOG(ERROR) << "NULL output received, cannot leave it!";
    return;
  }

  auto* wayland_output =
      static_cast<WaylandOutput*>(wl_output_get_user_data(output));
  surface->RemoveEnteredOutput(wayland_output->output_id());
}

void WaylandSurface::RemoveEnteredOutput(uint32_t output_id) {
  auto it = base::ranges::find(entered_outputs_, output_id);
  if (it == entered_outputs_.end())
    return;

  // In certain use cases, such as switching outputs in the single output
  // configuration, the compositor may move the surface from one output to
  // another one, send wl_surface::leave event to it, but defer sending
  // wl_surface::enter until the user moves or resizes the surface on the new
  // output.
  entered_outputs_.erase(it);

  if (root_window_)
    root_window_->OnLeftOutput();
}

void WaylandSurface::set_color_space(gfx::ColorSpace color_space) {
  if (!connection_->zcr_color_manager())
    return;

  if (color_space.GetPrimaryID() == gfx::ColorSpace::PrimaryID::INVALID ||
      color_space.GetTransferID() == gfx::ColorSpace::TransferID::INVALID ||
      color_space.GetMatrixID() == gfx::ColorSpace::MatrixID::INVALID ||
      color_space.GetRangeID() == gfx::ColorSpace::RangeID::INVALID) {
    DLOG(ERROR)
        << "WaylandSurface::SetColorSpace: Encountered invalid surface.";
    return;
  }
  auto wayland_zcr_color_space =
      connection_->zcr_color_manager()->GetColorSpace(color_space);
  if (wayland_zcr_color_space != nullptr)
    pending_state_.color_space = wayland_zcr_color_space;
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
