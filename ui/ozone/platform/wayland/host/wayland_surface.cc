// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include <alpha-compositing-unstable-v1-client-protocol.h>
#include <chrome-color-management-client-protocol.h>
#include <content-type-v1-client-protocol.h>
#include <fractional-scale-v1-client-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <overlay-prioritizer-client-protocol.h>
#include <surface-augmenter-client-protocol.h>
#include <viewporter-client-protocol.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/fractional_scale_manager.h"
#include "ui/ozone/platform/wayland/host/overlay_prioritizer.h"
#include "ui/ozone/platform/wayland/host/surface_augmenter.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_surface.h"
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

const wl_fixed_t kMinusOne = wl_fixed_from_int(-1);

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
          connection->surface_submission_in_pixel_coordinates()),
      use_viewporter_surface_scaling_(
          connection->UseViewporterSurfaceScaling()) {
  // Inherit per-surface preferred scale when owned by non-toplevel windows.
  // See https://wayland.app/protocols/fractional-scale-v1.
  if (root_window_ && root_window_->parent_window()) {
    preferred_scale_factor_ =
        root_window_->parent_window()->GetPreferredScaleFactor();
  }
}

WaylandSurface::~WaylandSurface() {
  for (auto& release : linux_buffer_releases_) {
    DCHECK(release.second.explicit_release_callback);
    std::move(release.second.explicit_release_callback)
        .Run(release.second.buffer.get(), base::ScopedFD());
  }
}

WaylandZAuraSurface* WaylandSurface::CreateZAuraSurface() {
  auto* zaura_shell = connection_->zaura_shell();
  if (!zaura_surface_ && zaura_shell) {
    zaura_surface_ = std::make_unique<WaylandZAuraSurface>(
        zaura_shell->wl_object(), surface(), connection_);
  }
  return zaura_surface_.get();
}

void WaylandSurface::ResetZAuraSurface() {
  zaura_surface_.reset();
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

  static constexpr wl_surface_listener kSurfaceListener = {
      .enter = &OnEnter,
      .leave = &OnLeave,
  };
  // If this surface is not the surface for its root_window, it don't need to
  // listen to output enter/leave events.
  // Not having a root_window() means this is an icon_surface from
  // WaylandDataDragController.
  if (!root_window() || root_window()->root_surface() == this) {
    wl_surface_add_listener(surface_.get(), &kSurfaceListener, this);
  }

  if (connection_->fractional_scale_manager_v1()) {
    static constexpr wp_fractional_scale_v1_listener kFractionalScaleListener =
        {
            .preferred_scale = &OnPreferredScale,
        };
    fractional_scale_.reset(wp_fractional_scale_manager_v1_get_fractional_scale(
        connection_->fractional_scale_manager_v1(), surface_.get()));
    wp_fractional_scale_v1_add_listener(fractional_scale_.get(),
                                        &kFractionalScaleListener, this);
  }

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

  auto* surface_augmenter = connection_->surface_augmenter();
  if (surface_augmenter && root_window() &&
      root_window()->root_surface() != this) {
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

void WaylandSurface::SetRootWindow(WaylandWindow* window) {
  root_window_ = window;
}

void WaylandSurface::set_acquire_fence(gfx::GpuFenceHandle acquire_fence) {
  // WaylandBufferManagerGPU knows if the synchronization is not available and
  // must disallow clients to use explicit synchronization.
  DCHECK(!apply_state_immediately_);
  DCHECK(connection_->linux_explicit_synchronization_v1() ||
         connection_->UseImplicitSyncInterop());
  if (!acquire_fence.is_null()) {
    base::TimeTicks ticks;
    auto status =
        gfx::GpuFence::GetStatusChangeTime(acquire_fence.Peek(), &ticks);
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
  pending_state_.buffer = buffer_handle->buffer();
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
    if (!(surface_submission_in_pixel_coordinates_ ||
          use_viewporter_surface_scaling_)) {
      // It's safe to cast the result of GetWaylandScale to an integer here
      // because the buffer scale should always be integer when both surface
      // submission in pixel coordinates and viewporter surface scaling is
      // disabled.
      wl_surface_set_buffer_scale(
          surface_.get(), static_cast<int32_t>(GetWaylandScale(state_)));
    }
    if (root_window_)
      root_window_->PropagateBufferScale(scale);
  }
}

void WaylandSurface::set_opaque_region(
    std::optional<std::vector<gfx::Rect>> region_px) {
  pending_state_.opaque_region_px.clear();
  if (!root_window_)
    return;

  if (region_px)
    pending_state_.opaque_region_px = *region_px;

  if (apply_state_immediately_) {
    state_.opaque_region_px = pending_state_.opaque_region_px;
    wl_surface_set_opaque_region(
        surface_.get(),
        pending_state_.opaque_region_px.empty()
            ? nullptr
            : CreateAndAddRegion(pending_state_.opaque_region_px,
                                 GetWaylandScale(pending_state_))
                  .get());
  }
}

void WaylandSurface::set_input_region(
    std::optional<std::vector<gfx::Rect>> region_px) {
  pending_state_.input_region_px.clear();
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
        pending_state_.input_region_px.empty()
            ? nullptr
            : CreateAndAddRegion(pending_state_.input_region_px,
                                 GetWaylandScale(pending_state_))
                  .get());
  }
}

float WaylandSurface::GetWaylandScale(const State& state) {
  if (surface_submission_in_pixel_coordinates_) {
    return 1;
  }
  return wl::ClampScale(use_viewporter_surface_scaling_
                            ? state.buffer_scale_float
                            : std::ceil(state.buffer_scale_float));
}

bool WaylandSurface::IsViewportScaled(const State& state) {
  if (state.viewport_px.IsEmpty()) {
    return false;
  }
  gfx::SizeF src_size_px =
      wl::ApplyWaylandTransform(gfx::SizeF(state.buffer_size_px),
                                wl::ToWaylandTransform(state.buffer_transform));
  if (!state.crop.IsEmpty()) {
    const gfx::RectF crop_transformed = wl::ApplyWaylandTransform(
        state.crop, gfx::SizeF(1, 1),
        wl::ToWaylandTransform(state.buffer_transform));
    src_size_px = gfx::ScaleRect(crop_transformed, src_size_px.width(),
                                 src_size_px.height())
                      .size();
  }
  if (src_size_px != state.viewport_px) {
    return true;
  }
  return false;
}

wl::Object<wl_region> WaylandSurface::CreateAndAddRegion(
    const std::vector<gfx::Rect>& region_px,
    float buffer_scale) {
  DCHECK(root_window_);
  DCHECK(!surface_submission_in_pixel_coordinates_ || buffer_scale == 1.f);

  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));

  for (const auto& rect_px : region_px) {
    // On Lacros, the buffer scale should be 1, so no need to ignore error.
    gfx::Rect rect = gfx::ScaleToEnclosedRect(rect_px, 1.f / buffer_scale);
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

bool WaylandSurface::ApplyPendingState() {
  DCHECK(!apply_state_immediately_);
  bool needs_commit = false;

  if (pending_state_.buffer_id != state_.buffer_id) {
    // The logic in DamageBuffer currently relies on attachment coordinates of
    // (0, 0). If this changes, then the calculation in DamageBuffer will also
    // need to be updated.
    // Note: should the offset be non-zero, use wl_surface_offset() to set it.
    wl_surface_attach(surface_.get(), pending_state_.buffer, 0, 0);
    needs_commit = true;

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
              surface_sync, pending_state_.acquire_fence.Peek());
        }

        if (!next_explicit_release_request_.is_null()) {
          auto* linux_buffer_release =
              zwp_linux_surface_synchronization_v1_get_release(surface_sync);
          // This must be very unlikely to happen, but there is a bug for this.
          // Thus, add a check for this object to ensure it's not null. See
          // https://crbug.com/1382976
          LOG_IF(FATAL, !linux_buffer_release)
              << "Unable to get an explicit release object.";

          static constexpr zwp_linux_buffer_release_v1_listener
              kBufferReleaseListener = {
                  .fenced_release = &OnFencedRelease,
                  .immediate_release = &OnImmediateRelease,
              };
          zwp_linux_buffer_release_v1_add_listener(
              linux_buffer_release, &kBufferReleaseListener, this);

          linux_buffer_releases_.emplace(
              linux_buffer_release,
              ExplicitReleaseInfo(
                  wl::Object<zwp_linux_buffer_release_v1>(linux_buffer_release),
                  pending_state_.buffer,
                  std::move(next_explicit_release_request_)));
        }
      } else if (connection_->UseImplicitSyncInterop()) {
        if (!pending_state_.acquire_fence.is_null()) {
          connection_->buffer_manager_host()->InsertAcquireFence(
              pending_state_.buffer_id, pending_state_.acquire_fence.Peek());
        }
      }
    }
  }
  pending_state_.acquire_fence = gfx::GpuFenceHandle();

  // Setting Color Space of surface.
  // Should be called infrequently: only when color space is changing to a
  // a different one.
  if (pending_state_.color_space != state_.color_space) {
    zcr_color_management_surface_->SetColorSpace(pending_state_.color_space);
    needs_commit = true;
  }

  if (pending_state_.buffer_transform != state_.buffer_transform) {
    wl_output_transform wl_transform =
        wl::ToWaylandTransform(pending_state_.buffer_transform);
    wl_surface_set_buffer_transform(surface_.get(), wl_transform);
    needs_commit = true;
  }

  if (pending_state_.opacity != state_.opacity) {
    DCHECK(blending());
    zcr_blending_v1_set_alpha(blending(),
                              wl_fixed_from_double(pending_state_.opacity));
    needs_commit = true;
  }
  if (pending_state_.use_blending != state_.use_blending) {
    DCHECK(blending());
    zcr_blending_v1_set_blending(blending(),
                                 pending_state_.use_blending
                                     ? ZCR_BLENDING_V1_BLENDING_EQUATION_PREMULT
                                     : ZCR_BLENDING_V1_BLENDING_EQUATION_NONE);
    needs_commit = true;
  }

  if (pending_state_.priority_hint != state_.priority_hint) {
    DCHECK(overlay_priority_surface());
    overlay_prioritized_surface_set_overlay_priority(
        overlay_priority_surface(),
        TranslatePriority(pending_state_.priority_hint));
    needs_commit = true;
  }

  // Don't set input region when use_native_frame is enabled.
  if (pending_state_.input_region_px != state_.input_region_px ||
      GetWaylandScale(pending_state_) != GetWaylandScale(state_)) {
    // Sets input region for input events to allow go through and
    // for the compositor to ignore the parts of the input region that fall
    // outside of the surface.
    wl_surface_set_input_region(
        surface_.get(),
        pending_state_.input_region_px.empty()
            ? nullptr
            : CreateAndAddRegion(pending_state_.input_region_px,
                                 GetWaylandScale(pending_state_))
                  .get());
    needs_commit = true;
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
    needs_commit = true;
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
      needs_commit = true;

      wl_array_release(&color_data);
    }
  }

  if (pending_state_.rounded_clip_bounds != state_.rounded_clip_bounds) {
    DCHECK(get_augmented_surface());
    gfx::RRectF rounded_clip_bounds = pending_state_.rounded_clip_bounds;
    rounded_clip_bounds.Scale(1.f / GetWaylandScale(pending_state_));

    if (augmented_surface_get_version(get_augmented_surface()) >=
        AUGMENTED_SURFACE_SET_ROUNDED_CORNERS_CLIP_BOUNDS_SINCE_VERSION) {
      augmented_surface_set_rounded_corners_clip_bounds(
          get_augmented_surface(),
          wl_fixed_from_double(rounded_clip_bounds.rect().x()),
          wl_fixed_from_double(rounded_clip_bounds.rect().y()),
          wl_fixed_from_double(rounded_clip_bounds.rect().width()),
          wl_fixed_from_double(rounded_clip_bounds.rect().height()),
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
      needs_commit = true;
    } else if (augmented_surface_get_version(get_augmented_surface()) >=
               AUGMENTED_SURFACE_SET_ROUNDED_CLIP_BOUNDS_SINCE_VERSION) {
      // For debugging purposes, stdout uses of the old incarnation of this API.
      // In the future, this API will get a clean up pass.
      LOG(WARNING) << "The 'augmented_surface::set_rounded_clip_bounds' "
                      "request has been deprecated in favor of "
                      "augmented_surface::set_rounded_corners_clip_bounds.";
    }
  }

  if (pending_state_.clip_rect != state_.clip_rect) {
    DCHECK(get_augmented_surface());
    if (connection_->surface_augmenter()
            ->SupportsClipRectOnAugmentedSurface()) {
      std::optional<gfx::RectF> clip_rect = pending_state_.clip_rect;
      if (clip_rect) {
        clip_rect->Scale(1.f / GetWaylandScale(pending_state_));
        augmented_surface_set_clip_rect(
            get_augmented_surface(), wl_fixed_from_double(clip_rect->x()),
            wl_fixed_from_double(clip_rect->y()),
            wl_fixed_from_double(clip_rect->width()),
            wl_fixed_from_double(clip_rect->height()));
      } else {
        augmented_surface_set_clip_rect(get_augmented_surface(), kMinusOne,
                                        kMinusOne, kMinusOne, kMinusOne);
      }
      needs_commit = true;
    }
  }

  if (content_type_ &&
      (pending_state_.contains_video != state_.contains_video)) {
    wp_content_type_v1_set_content_type(content_type_.get(),
                                        pending_state_.contains_video
                                            ? WP_CONTENT_TYPE_V1_TYPE_VIDEO
                                            : WP_CONTENT_TYPE_V1_TYPE_NONE);
    needs_commit = true;
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

  gfx::RectF crop = pending_state_.crop;
  gfx::SizeF viewport_px = pending_state_.viewport_px;

  // If this is the root surface, no viewport scaling is requested, surface
  // submission in pixel coordinates is disabled and fractional_scale_v1 is in
  // use, then crop the buffer in accordance with the protocol specification to
  // ensure that a pixel on the window will correspond to a pixel on the
  // physical display.
  if (!surface_submission_in_pixel_coordinates_ &&
      connection_->fractional_scale_manager_v1() && root_window_ &&
      root_window_->root_surface() == this &&
      !IsViewportScaled(pending_state_)) {
    gfx::Size old_size_px =
        crop.IsEmpty()
            ? pending_state_.buffer_size_px
            : gfx::ToFlooredSize(
                  gfx::ScaleRect(crop, pending_state_.buffer_size_px.width(),
                                 pending_state_.buffer_size_px.height())
                      .size());
    gfx::Size new_size_dip = gfx::ScaleToFlooredSize(
        old_size_px, 1.f / pending_state_.buffer_scale_float);
    gfx::Size new_size_px = gfx::ScaleToRoundedSize(
        new_size_dip, pending_state_.buffer_scale_float);
    if (new_size_px != old_size_px) {
      crop.set_width(static_cast<float>(new_size_px.width()) /
                     static_cast<float>(pending_state_.buffer_size_px.width()));
      crop.set_height(
          static_cast<float>(new_size_px.height()) /
          static_cast<float>(pending_state_.buffer_size_px.height()));
      viewport_px = wl::ApplyWaylandTransform(
          gfx::SizeF(new_size_px),
          wl::ToWaylandTransform(pending_state_.buffer_transform));
    }
  }

  if (viewport_px.IsEmpty() && use_viewporter_surface_scaling_) {
    // Force usage of viewporter when needed for fractional scaling.
    viewport_px = bounds;
  }

  // When viewport_px is set, wp_viewport will scale the surface accordingly.
  // Thus, there is no need to downscale bounds as Wayland compositor
  // understands that.
  if (!viewport_px.IsEmpty() && viewport()) {
    // Unset buffer scale if wp_viewport.destination will be set.
    applying_surface_scale = 1;
  } else {
    applying_surface_scale = GetWaylandScale(pending_state_);
    bounds = gfx::ScaleSize(bounds, 1.f / GetWaylandScale(pending_state_));
  }
  if (!(surface_submission_in_pixel_coordinates_ ||
        use_viewporter_surface_scaling_) &&
      surface_scale_set_ != applying_surface_scale) {
    wl_surface_set_buffer_scale(surface_.get(), applying_surface_scale);
    surface_scale_set_ = applying_surface_scale;
    needs_commit = true;
  }
  DCHECK_GE(surface_scale_set_, 1);

  // If this is not a subsurface, propagate the buffer scale.
  if (root_window_ && root_window_->root_surface() == this)
    root_window_->PropagateBufferScale(pending_state_.buffer_scale_float);

  gfx::RectF viewport_src_dip;
  wl_fixed_t src_to_set[4] = {wl_fixed_from_int(-1), wl_fixed_from_int(-1),
                              wl_fixed_from_int(-1), wl_fixed_from_int(-1)};
  if (crop.IsEmpty()) {
    viewport_src_dip = gfx::RectF(bounds);
  } else {
    // TODO(crbug.com/359904707) Fix rounding errors which can lead to imprecise
    // values below.

    // viewport_src_dip needs to be in post-transform coordinates.
    gfx::RectF crop_transformed = wl::ApplyWaylandTransform(
        crop, gfx::SizeF(1, 1),
        wl::ToWaylandTransform(pending_state_.buffer_transform));
    viewport_src_dip =
        gfx::ScaleRect(crop_transformed, bounds.width(), bounds.height());
    DCHECK(viewport());

    // It not completely unexpected to stretch a texture more than the smallest
    // fixed point can represent. The two cases here are solid color buffers
    // (1x1 or 4x4) and something like 9-patch shadows (5x5) stretched to the
    // entire window width/height.
    {
      constexpr wl_fixed_t kViewportSizeMin = 1;
      const float kViewPortSizeMinFloat =
          static_cast<float>(wl_fixed_to_double(kViewportSizeMin));

      if (wl_fixed_from_double(viewport_src_dip.width()) <= 0) {
        viewport_src_dip.set_width(kViewPortSizeMinFloat);
        src_to_set[2] = kViewportSizeMin;
      } else {
        src_to_set[2] = wl_fixed_from_double(viewport_src_dip.width());
      }

      if (wl_fixed_from_double(viewport_src_dip.height()) <= 0) {
        viewport_src_dip.set_height(kViewPortSizeMinFloat);
        src_to_set[3] = kViewportSizeMin;
      } else {
        src_to_set[3] = wl_fixed_from_double(viewport_src_dip.height());
      }
    }

    if (wl_fixed_from_double(viewport_src_dip.x()) < 0 ||
        wl_fixed_from_double(viewport_src_dip.y()) < 0) {
      LOG(ERROR) << "Sending viewport src with width/height zero or negative "
                    "origin will result in wayland disconnection";
      // TODO(crbug.com/40839779): Resolve why this viewport size ends up being
      // zero and remove the fix below.
      LOG(ERROR) << "viewport_src_dip=" << viewport_src_dip.ToString()
                 << " crop=" << crop.ToString()
                 << " bounds=" << bounds.ToString()
                 << "  pending_state_.buffer_size_px="
                 << pending_state_.buffer_size_px.ToString();

      viewport_src_dip.set_x(std::max(viewport_src_dip.x(), 0.f));
      viewport_src_dip.set_y(std::max(viewport_src_dip.y(), 0.f));
    }

    src_to_set[0] = wl_fixed_from_double(viewport_src_dip.x()),
    src_to_set[1] = wl_fixed_from_double(viewport_src_dip.y());
  }
  // Apply crop (wp_viewport.set_source).
  if (viewport() && !base::ranges::equal(src_to_set, src_set_)) {
    wp_viewport_set_source(viewport(), src_to_set[0], src_to_set[1],
                           src_to_set[2], src_to_set[3]);
    memcpy(src_set_, src_to_set, 4 * sizeof(*src_to_set));
    needs_commit = true;
  }

  gfx::SizeF viewport_dst_dip =
      viewport_px.IsEmpty()
          ? viewport_src_dip.size()
          : gfx::ScaleSize(viewport_px, 1.f / GetWaylandScale(pending_state_));
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
      needs_commit = true;
    } else if (viewport()) {
      wp_viewport_set_destination(
          viewport(),
          dst_to_set[0] > 0.f ? base::ClampRound(viewport_dst_dip.width())
                              : static_cast<int>(dst_to_set[0]),
          dst_to_set[1] > 0.f ? base::ClampRound(viewport_dst_dip.height())
                              : static_cast<int>(dst_to_set[1]));
      needs_commit = true;
    }
    memcpy(dst_set_, dst_to_set, 2 * sizeof(*dst_to_set));
  }

  if (pending_state_.frame_trace_id >= 0) {
    bool is_frame_tracing_enabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED("viz,benchmark,graphics.pipeline",
                                       &is_frame_tracing_enabled);
    if (is_frame_tracing_enabled) {
      auto* augmented_surface = get_augmented_surface();
      if (augmented_surface &&
          augmented_surface_get_version(augmented_surface) >=
              AUGMENTED_SURFACE_SET_FRAME_TRACE_ID_SINCE_VERSION) {
        augmented_surface_set_frame_trace_id(
            augmented_surface, pending_state_.frame_trace_id >> 32,
            pending_state_.frame_trace_id & 0xffffffff);
      }
    }
    pending_state_.frame_trace_id = -1;
  }

  DCHECK_LE(pending_state_.damage_px.size(), 1u);
  if (pending_state_.damage_px.empty() ||
      pending_state_.damage_px.back().IsEmpty()) {
    pending_state_.damage_px.clear();
    state_ = pending_state_;
    return needs_commit;
  }

  DCHECK(pending_state_.buffer);

  // Lacros on Ash will always have a scale factory of 1, so damage will be
  // unchanged on Ash and no need to ignore error, but that won't always be true
  // on other compositors.
  gfx::Rect damage = ScaleToEnclosingRect(
      pending_state_.damage_px.back(), 1.f / GetWaylandScale(pending_state_));

  // TODO(fangzhoug): The newer wl_surface_damage_buffer API is not currently
  // supported by Ash. Damage is specified in viz::Display space, so if we want
  // to use that API in the future, some math will be required to transform the
  // damage into buffer space.
  wl_surface_damage(surface_.get(), damage.x(), damage.y(), damage.width(),
                    damage.height());
  needs_commit = true;

  pending_state_.damage_px.clear();
  state_ = pending_state_;
  return needs_commit;
}

void WaylandSurface::ForceImmediateStateApplication() {
  apply_state_immediately_ = true;
}

void WaylandSurface::EnableTrustedDamageIfPossible() {
  if (get_augmented_surface() &&
      augmented_surface_get_version(get_augmented_surface()) >=
          AUGMENTED_SURFACE_SET_TRUSTED_DAMAGE_SINCE_VERSION) {
    augmented_surface_set_trusted_damage(get_augmented_surface(), true);
  }
}

void WaylandSurface::ExplicitRelease(
    zwp_linux_buffer_release_v1* linux_buffer_release,
    base::ScopedFD fence) {
  auto iter = linux_buffer_releases_.find(linux_buffer_release);
  CHECK(iter != linux_buffer_releases_.end(), base::NotFatalUntil::M130);
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
  clip_rect = other.clip_rect;
  contains_video = other.contains_video;
  return *this;
}

// static
void WaylandSurface::OnEnter(void* data,
                             wl_surface* surface,
                             wl_output* output) {
  auto* self = static_cast<WaylandSurface*>(data);
  DCHECK(self);

  // The compositor can send a null output. See https://crbug.com/1332540.
  if (!output) {
    LOG(ERROR) << "NULL output received, cannot enter it!";
    return;
  }

  auto* wayland_output =
      static_cast<WaylandOutput*>(wl_output_get_user_data(output));

  DCHECK_NE(self->connection_->wayland_output_manager()->GetOutput(
                wayland_output->output_id()),
            nullptr);

  if (auto it = base::ranges::find(self->entered_outputs_,
                                   wayland_output->output_id());
      it == self->entered_outputs_.end()) {
    self->entered_outputs_.emplace_back(wayland_output->output_id());
  }

  if (self->root_window_) {
    self->root_window_->OnEnteredOutput();
  }
}

// static
void WaylandSurface::OnLeave(void* data,
                             wl_surface* surface,
                             wl_output* output) {
  auto* self = static_cast<WaylandSurface*>(data);
  DCHECK(self);

  // The compositor can send a null output. See https://crbug.com/1332540.
  if (!output) {
    LOG(ERROR) << "NULL output received, cannot leave it!";
    return;
  }

  auto* wayland_output =
      static_cast<WaylandOutput*>(wl_output_get_user_data(output));
  self->RemoveEnteredOutput(wayland_output->output_id());
}

// static
void WaylandSurface::OnPreferredScale(void* data,
                                      wp_fractional_scale_v1* fractional_scale,
                                      uint32_t scale) {
  auto* self = static_cast<WaylandSurface*>(data);
  DCHECK(self);

  if (!self->connection_->UsePerSurfaceScaling()) {
    VLOG(1) << "Per-surface scaling is disabled.";
    return;
  }

  // Specified in fractional-scale-v1
  constexpr float kFractionalScaleDenominator = 120.0f;
  const float scale_factor =
      scale == 0 ? 1.0f : (scale / kFractionalScaleDenominator);

  self->preferred_scale_factor_ = scale_factor;
  if (self->root_window_) {
    self->root_window_->UpdateWindowScale(/*update_bounds=*/true);
  }
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
  if (!connection_->zcr_color_manager()) {
    return;
  }
  if (!color_space.IsValid() && pending_state_.contains_video) {
    // Not all video content contains colorspace information.
    // In this case, default to Rec709.
    // Maybe use Rec601 for SD video if it becomes an issue.
    color_space = gfx::ColorSpace::CreateREC709();
  }
  if (!color_space.IsValid()) {
    return;
  }

  auto wayland_zcr_color_space =
      connection_->zcr_color_manager()->GetColorSpace(color_space);
  if (wayland_zcr_color_space != nullptr)
    pending_state_.color_space = wayland_zcr_color_space;
}

// static
void WaylandSurface::OnFencedRelease(
    void* data,
    zwp_linux_buffer_release_v1* buffer_release,
    int32_t fence) {
  auto* self = static_cast<WaylandSurface*>(data);
  auto fd = base::ScopedFD(fence);
  self->ExplicitRelease(buffer_release, std::move(fd));
}

// static
void WaylandSurface::OnImmediateRelease(
    void* data,
    zwp_linux_buffer_release_v1* buffer_release) {
  auto* self = static_cast<WaylandSurface*>(data);
  self->ExplicitRelease(buffer_release, base::ScopedFD());
}

}  // namespace ui
