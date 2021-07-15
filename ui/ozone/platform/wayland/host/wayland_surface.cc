// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <viewporter-client-protocol.h>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

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

WaylandSurface::~WaylandSurface() = default;

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

  return true;
}

void WaylandSurface::UnsetRootWindow() {
  DCHECK(surface_);
  root_window_ = nullptr;
}

void WaylandSurface::SetAcquireFence(const gfx::GpuFenceHandle& acquire_fence) {
  // WaylandBufferManagerGPU knows if the synchronization is not available and
  // must disallow clients to use explicit synchronization.
  DCHECK(connection_->linux_explicit_synchronization_v1());
  zwp_linux_surface_synchronization_v1_set_acquire_fence(
      GetSurfaceSync(), acquire_fence.owned_fd.get());
}

void WaylandSurface::AttachBuffer(wl_buffer* buffer) {
  // The logic in DamageBuffer currently relies on attachment coordinates of
  // (0, 0). If this changes, then the calculation in DamageBuffer will also
  // need to be updated.
  wl_surface_attach(surface_.get(), buffer, 0, 0);
  buffer_attached_since_last_commit_ = buffer;
  connection_->ScheduleFlush();
}

void WaylandSurface::UpdateBufferDamageRegion(
    const gfx::Rect& pending_damage_region,
    const gfx::Size& buffer_size) {
  // Buffer-local coordinates are in pixels, surface coordinates are in DIP.
  // The coordinate transformations from buffer pixel coordinates up to
  // the surface-local coordinates happen in the following order:
  //   1. buffer_transform (wl_surface.set_buffer_transform)
  //   2. buffer_scale (wl_surface.set_buffer_scale)
  //   3. crop and scale (wp_viewport.set*)
  // Apply buffer_transform (wl_surface.set_buffer_transform).
  gfx::Size bounds = wl::ApplyWaylandTransform(
      buffer_size, wl::ToWaylandTransform(buffer_transform_));

  // When crop_rect is set, wp_viewport will crop and scale the surface
  // accordingly. Thus, there is no need to downscale bounds as Wayland
  // compositor understands that.
  // TODO(msisov): it'd be better to decide to set source, destination or buffer
  // scale at commit time and avoid these kind of conditions.
  if (crop_rect_.IsEmpty()) {
    bounds = gfx::ScaleToCeiledSize(bounds, 1.f / buffer_scale_);
  } else {
    // Unset buffer scale if wp_viewport is set.
    SetSurfaceBufferScale(1);
  }
  // Apply crop (wp_viewport.set_source).
  gfx::Rect viewport_src = gfx::Rect(bounds);
  if (!crop_rect_.IsEmpty()) {
    viewport_src = gfx::ToEnclosedRect(
        gfx::ScaleRect(crop_rect_, bounds.width(), bounds.height()));
    if (viewport()) {
      wp_viewport_set_source(viewport(), wl_fixed_from_int(viewport_src.x()),
                             wl_fixed_from_int(viewport_src.y()),
                             wl_fixed_from_int(viewport_src.width()),
                             wl_fixed_from_int(viewport_src.height()));
    }
  }
  // Apply viewport scale (wp_viewport.set_destination).
  gfx::Size viewport_dst = bounds;
  if (!display_size_dip_.IsEmpty()) {
    viewport_dst = display_size_dip_;
  }

  if (connection_->compositor_version() >=
      WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
    // wl_surface_damage_buffer relies on compositor API version 4. See
    // https://bit.ly/2u00lv6 for details.
    // We don't need to apply any scaling because pending_damage_region is
    // already in buffer coordinates.
    wl_surface_damage_buffer(
        surface_.get(), pending_damage_region.x(), pending_damage_region.y(),
        pending_damage_region.width(), pending_damage_region.height());
  } else {
    // Calculate the damage region in surface coordinates.
    // The calculation for damage region relies on the assumption: The buffer is
    // always attached at surface location (0, 0).
    // It's possible to write logic that accounts for attaching buffer at other
    // locations, but it's currently unnecessary.

    // Apply buffer_transform (wl_surface.set_buffer_transform).
    gfx::Rect damage =
        wl::ApplyWaylandTransform(pending_damage_region, buffer_size,
                                  wl::ToWaylandTransform(buffer_transform_));
    // Apply buffer_scale (wl_surface.set_buffer_scale).
    damage = gfx::ScaleToEnclosingRect(damage, 1.f / buffer_scale_);
    // Adjust coordinates to |viewport_src| (wp_viewport.set_source).
    damage = wl::TranslateBoundsToParentCoordinates(damage, viewport_src);
    // Apply viewport scale (wp_viewport.set_destination).
    damage = gfx::ScaleToEnclosingRect(
        damage, static_cast<float>(viewport_dst.width()) / viewport_src.width(),
        static_cast<float>(viewport_dst.height()) / viewport_src.height());

    wl_surface_damage(surface_.get(), damage.x(), damage.y(), damage.width(),
                      damage.height());
  }

  connection_->ScheduleFlush();
}

void WaylandSurface::Commit() {
  auto* surface_sync = GetSurfaceSync();
  if (surface_sync && buffer_attached_since_last_commit_) {
    auto* linux_buffer_release =
        zwp_linux_surface_synchronization_v1_get_release(surface_sync);

    static struct zwp_linux_buffer_release_v1_listener release_listener = {
        &WaylandSurface::FencedRelease,
        &WaylandSurface::ImmediateRelease,
    };
    zwp_linux_buffer_release_v1_add_listener(linux_buffer_release,
                                             &release_listener, this);

    linux_buffer_releases_.emplace(
        linux_buffer_release,
        ExplicitReleaseInfo(
            wl::Object<zwp_linux_buffer_release_v1>(linux_buffer_release),
            buffer_attached_since_last_commit_));
  }
  wl_surface_commit(surface_.get());
  buffer_attached_since_last_commit_ = nullptr;
  connection_->ScheduleFlush();
}

void WaylandSurface::SetBufferTransform(gfx::OverlayTransform transform) {
  DCHECK(transform != gfx::OVERLAY_TRANSFORM_INVALID);
  if (buffer_transform_ == transform)
    return;

  buffer_transform_ = transform;
  wl_output_transform wl_transform = wl::ToWaylandTransform(buffer_transform_);
  wl_surface_set_buffer_transform(surface_.get(), wl_transform);
}

void WaylandSurface::SetSurfaceBufferScale(int32_t scale) {
  wl_surface_set_buffer_scale(surface_.get(), scale);
  buffer_scale_ = scale;
  connection_->ScheduleFlush();
}

void WaylandSurface::SetOpaqueRegion(const gfx::Rect& region_px) {
  // It's important to set opaque region for opaque windows (provides
  // optimization hint for the Wayland compositor).
  if (!root_window_ || !root_window_->IsOpaqueWindow())
    return;

  wl_surface_set_opaque_region(surface_.get(),
                               CreateAndAddRegion(region_px).get());

  connection_->ScheduleFlush();
}

void WaylandSurface::SetInputRegion(const gfx::Rect& region_px) {
  // Don't set input region when use_native_frame is enabled.
  if (!root_window_ || root_window_->ShouldUseNativeFrame())
    return;

  // Sets input region for input events to allow go through and
  // for the compositor to ignore the parts of the input region that fall
  // outside of the surface.
  wl_surface_set_input_region(surface_.get(),
                              CreateAndAddRegion(region_px).get());

  connection_->ScheduleFlush();
}

wl::Object<wl_region> WaylandSurface::CreateAndAddRegion(
    const gfx::Rect& region_px) {
  DCHECK(root_window_);

  wl::Object<wl_region> region(
      wl_compositor_create_region(connection_->compositor()));

  auto window_shape_in_dips = root_window_->GetWindowShape();

  // Only root_surface and primary_subsurface should use |window_shape_in_dips|.
  // Do not use non empty |window_shape_in_dips| if |region_px| is empty, i.e.
  // this surface is transluscent.
  bool is_primary_or_root =
      root_window_->root_surface() == this ||
      (root_window()->primary_subsurface() &&
       root_window()->primary_subsurface()->wayland_surface() == this);
  if (window_shape_in_dips.has_value() && !region_px.IsEmpty() &&
      is_primary_or_root) {
    for (const auto& rect : window_shape_in_dips.value())
      wl_region_add(region.get(), rect.x(), rect.y(), rect.width(),
                    rect.height());
  } else {
    gfx::Rect region_dip = gfx::ScaleToEnclosingRect(
        region_px, 1.f / root_window_->window_scale());
    wl_region_add(region.get(), region_dip.x(), region_dip.y(),
                  region_dip.width(), region_dip.height());
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

void WaylandSurface::SetViewportSource(const gfx::RectF& src_rect) {
  if (src_rect == crop_rect_)
    return;
  // |src_rect| {1.f, 1.f} does not apply cropping so set it to empty.
  if (src_rect.IsEmpty() || src_rect == gfx::RectF{1.f, 1.f}) {
    crop_rect_ = gfx::RectF();
    if (viewport()) {
      wp_viewport_set_source(viewport(), wl_fixed_from_int(-1),
                             wl_fixed_from_int(-1), wl_fixed_from_int(-1),
                             wl_fixed_from_int(-1));
    }
    return;
  }

  // wp_viewport_set_source() needs pixel inputs. Store |src_rect| and calculate
  // in UpdateBufferDamageRegion().
  crop_rect_ = src_rect;
}

void WaylandSurface::SetViewportDestination(const gfx::Size& dest_size_px) {
  if (dest_size_px == gfx::ScaleToRoundedSize(display_size_dip_, buffer_scale_))
    return;

  if (dest_size_px.IsEmpty()) {
    display_size_dip_ = gfx::Size();
    if (viewport()) {
      wp_viewport_set_destination(viewport(), -1, -1);
    }
    return;
  }
  display_size_dip_ = gfx::ScaleToCeiledSize(dest_size_px, 1.f / buffer_scale_);
  if (viewport()) {
    wp_viewport_set_destination(viewport(), display_size_dip_.width(),
                                display_size_dip_.height());
  }
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

void WaylandSurface::ExplicitRelease(
    struct zwp_linux_buffer_release_v1* linux_buffer_release,
    absl::optional<int32_t> fence) {
  auto iter = linux_buffer_releases_.find(linux_buffer_release);
  DCHECK(iter != linux_buffer_releases_.end());
  DCHECK(iter->second.buffer);
  if (!explicit_release_callback_.is_null())
    explicit_release_callback_.Run(iter->second.buffer, fence);
  linux_buffer_releases_.erase(iter);
}

// static
void WaylandSurface::Enter(void* data,
                           struct wl_surface* wl_surface,
                           struct wl_output* output) {
  auto* const surface = static_cast<WaylandSurface*>(data);
  DCHECK(surface);

  surface->entered_outputs_.emplace_back(
      static_cast<WaylandOutput*>(wl_output_get_user_data(output)));

  if (surface->root_window_)
    surface->root_window_->OnEnteredOutputIdAdded();
}

// static
void WaylandSurface::Leave(void* data,
                           struct wl_surface* wl_surface,
                           struct wl_output* output) {
  auto* const surface = static_cast<WaylandSurface*>(data);
  DCHECK(surface);

  auto entered_outputs_it_ = std::find(
      surface->entered_outputs_.begin(), surface->entered_outputs_.end(),
      static_cast<WaylandOutput*>(wl_output_get_user_data(output)));
  // Workaround: when a user switches physical output between two displays,
  // a surface does not necessarily receive enter events immediately or until
  // a user resizes/moves it.  This means that switching output between
  // displays in a single output mode results in leave events, but the surface
  // might not have received enter event before.  Thus, remove the id of the
  // output that the surface leaves only if it was stored before.
  if (entered_outputs_it_ != surface->entered_outputs_.end())
    surface->entered_outputs_.erase(entered_outputs_it_);

  if (surface->root_window_)
    surface->root_window_->OnEnteredOutputIdRemoved();
}

// static
void WaylandSurface::FencedRelease(
    void* data,
    struct zwp_linux_buffer_release_v1* linux_buffer_release,
    int32_t fence) {
  static_cast<WaylandSurface*>(data)->ExplicitRelease(linux_buffer_release,
                                                      fence);
}

// static
void WaylandSurface::ImmediateRelease(
    void* data,
    struct zwp_linux_buffer_release_v1* linux_buffer_release) {
  static_cast<WaylandSurface*>(data)->ExplicitRelease(linux_buffer_release,
                                                      absl::nullopt);
}

}  // namespace ui
