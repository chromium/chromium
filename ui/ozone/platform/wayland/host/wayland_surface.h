// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_

#include <cstdint>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

struct zwp_linux_buffer_release_v1;

namespace ui {

class WaylandConnection;
class WaylandOutput;
class WaylandWindow;

// Wrapper of a wl_surface, owned by a WaylandWindow or a WlSubsurface.
class WaylandSurface {
 public:
  using ExplicitReleaseCallback =
      base::RepeatingCallback<void(wl_buffer*, absl::optional<int32_t>)>;

  WaylandSurface(WaylandConnection* connection, WaylandWindow* ro_window);
  WaylandSurface(const WaylandSurface&) = delete;
  WaylandSurface& operator=(const WaylandSurface&) = delete;
  ~WaylandSurface();

  WaylandWindow* root_window() const { return root_window_; }
  wl_surface* surface() const { return surface_.get(); }
  wp_viewport* viewport() const { return viewport_.get(); }

  const std::vector<WaylandOutput*>& entered_outputs() const {
    return entered_outputs_;
  }

  void set_explicit_release_callback(ExplicitReleaseCallback callback) {
    explicit_release_callback_ = callback;
  }

  int32_t buffer_scale() const { return buffer_scale_; }

  // Returns an id that identifies the |wl_surface_|.
  uint32_t GetSurfaceId() const;
  // Returns a gfx::AcceleratedWidget that identifies the WaylandWindow that
  // this WaylandSurface belongs to.
  gfx::AcceleratedWidget GetWidget() const;

  // Initializes the WaylandSurface and returns true iff success.
  // This may return false if a wl_surface could not be created, for example.
  bool Initialize();

  // Unsets |root_window_|. This is intended to be used in special cases, where
  // the underlying wl_surface must be kept alive with no root window associated
  // (e.g: window/tab dragging sessions).
  void UnsetRootWindow();

  // Sets a non-null in-fence, must be combined with an AttachBuffer() and a
  // Commit().
  void SetAcquireFence(const gfx::GpuFenceHandle& acquire_fence);

  // Attaches the given wl_buffer to the underlying wl_surface at (0, 0).
  void AttachBuffer(wl_buffer* buffer);

  // Describes where the surface needs to be repainted according to
  // |buffer_pending_damage_region|, which should be in buffer coordinates (px).
  void UpdateBufferDamageRegion(const gfx::Rect& buffer_pending_damage_region,
                                const gfx::Size& buffer_size);

  // Commits the underlying wl_surface.
  void Commit();

  // Sets an optional transformation for how the Wayland compositor interprets
  // the contents of the buffer attached to this surface.
  void SetBufferTransform(gfx::OverlayTransform transform);

  // Sets the |buffer_scale| (with respect to the scale factor used by the GPU
  // process) for the next submitted buffer. This helps Wayland compositor to
  // determine buffer size in dip (GPU operates in pixels. So, when buffers are
  // created, their requested size is in pixels).
  void SetSurfaceBufferScale(int32_t scale);

  // Sets the region that is opaque on this surface in physical pixels. This is
  // expected to be called whenever the region that the surface span changes or
  // the opacity changes. |region_px| is specified surface-local, in physical
  // pixels.
  void SetOpaqueRegion(const gfx::Rect& region_px);

  // Sets the input region on this surface in physical pixels.
  // The input region indicates which parts of the surface accept pointer and
  // touch input events. This is expected to be called from ToplevelWindow
  // whenever the region that the surface span changes or window state changes
  // when custom frame is used.
  void SetInputRegion(const gfx::Rect& region_px);

  // Set the source rectangle of the associated wl_surface.
  // See:
  // https://cgit.freedesktop.org/wayland/wayland-protocols/tree/stable/viewporter/viewporter.xml
  // If |src_rect| is empty, the source rectangle is unset.
  // Note this method does not send corresponding wayland requests until
  // attaching the next buffer.
  void SetViewportSource(const gfx::RectF& src_rect);

  // Set the destination size of the associated wl_surface according to
  // |dest_size_px|, which should be in physical pixels.
  // Note this method sends corresponding wayland requests immediately because
  // it does not need a new buffer attach to take effect.
  void SetViewportDestination(const gfx::Size& dest_size_px);

  // Creates a wl_subsurface relating this surface and a parent surface,
  // |parent|. Callers take ownership of the wl_subsurface.
  wl::Object<wl_subsurface> CreateSubsurface(WaylandSurface* parent);

 private:
  // Holds information about each explicit synchronization buffer release.
  struct ExplicitReleaseInfo {
    ExplicitReleaseInfo(
        wl::Object<zwp_linux_buffer_release_v1>&& linux_buffer_release,
        wl_buffer* buffer);
    ~ExplicitReleaseInfo();

    ExplicitReleaseInfo(const ExplicitReleaseInfo&) = delete;
    ExplicitReleaseInfo& operator=(const ExplicitReleaseInfo&) = delete;

    ExplicitReleaseInfo(ExplicitReleaseInfo&&);
    ExplicitReleaseInfo& operator=(ExplicitReleaseInfo&&);

    wl::Object<zwp_linux_buffer_release_v1> linux_buffer_release;
    // The buffer associated with this explicit release.
    wl_buffer* buffer;
  };

  wl::Object<wl_region> CreateAndAddRegion(const gfx::Rect& region_px);

  // Creates (if not created) the synchronization surface and returns a pointer
  // to it.
  zwp_linux_surface_synchronization_v1* GetSurfaceSync();

  WaylandConnection* const connection_;
  WaylandWindow* root_window_ = nullptr;
  wl::Object<wl_surface> surface_;
  wl::Object<wp_viewport> viewport_;
  wl::Object<zwp_linux_surface_synchronization_v1> surface_sync_;
  base::flat_map<zwp_linux_buffer_release_v1*, ExplicitReleaseInfo>
      linux_buffer_releases_;
  ExplicitReleaseCallback explicit_release_callback_;
  wl_buffer* buffer_attached_since_last_commit_ = nullptr;

  // For top level window, stores outputs that the window is currently rendered
  // at.
  //
  // Not used by popups.  When sub-menus are hidden and shown again, Wayland
  // 'repositions' them to wrong outputs by sending them leave and enter
  // events so their list of entered outputs becomes meaningless after they have
  // been hidden at least once.  To determine which output the popup belongs to,
  // we ask its parent.
  std::vector<WaylandOutput*> entered_outputs_;

  // Transformation for how the compositor interprets the contents of the
  // buffer.
  gfx::OverlayTransform buffer_transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  // Current scale factor of a next attached buffer used by the GPU process.
  int32_t buffer_scale_ = 1;

  // Following fields are used to help determine the damage_region in
  // surface-local coordinates if wl_surface_damage_buffer() is not available.
  // Normalized bounds of the buffer to be displayed in |display_size_px_|.
  // If empty, no cropping is applied.
  gfx::RectF crop_rect_ = gfx::RectF();

  // Current size of the destination of the viewport in DIP. Wayland compositor
  // will scale the (cropped) buffer content to fit the |display_size_dip_|.
  // If empty, no scaling is applied.
  gfx::Size display_size_dip_ = gfx::Size();

  void ExplicitRelease(struct zwp_linux_buffer_release_v1* linux_buffer_release,
                       absl::optional<int32_t> fence);

  // wl_surface_listener
  static void Enter(void* data,
                    struct wl_surface* wl_surface,
                    struct wl_output* output);
  static void Leave(void* data,
                    struct wl_surface* wl_surface,
                    struct wl_output* output);

  // zwp_linux_buffer_release_v1_listener
  static void FencedRelease(
      void* data,
      struct zwp_linux_buffer_release_v1* linux_buffer_release,
      int32_t fence);
  static void ImmediateRelease(
      void* data,
      struct zwp_linux_buffer_release_v1* linux_buffer_release);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
