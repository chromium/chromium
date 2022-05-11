// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_

#include <cstdint>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

struct zwp_linux_buffer_release_v1;
struct zcr_blending_v1;

namespace ui {

class WaylandConnection;
class WaylandOutput;
class WaylandWindow;
class WaylandBufferHandle;

// Wrapper of a wl_surface, owned by a WaylandWindow or a WlSubsurface.
class WaylandSurface {
 public:
  using ExplicitReleaseCallback =
      base::RepeatingCallback<void(wl_buffer*, base::ScopedFD)>;

  WaylandSurface(WaylandConnection* connection, WaylandWindow* ro_window);
  WaylandSurface(const WaylandSurface&) = delete;
  WaylandSurface& operator=(const WaylandSurface&) = delete;
  ~WaylandSurface();

  WaylandWindow* root_window() const { return root_window_; }
  overlay_prioritized_surface* overlay_priority_surface() {
    return overlay_priority_surface_.get();
  }
  wl_surface* surface() const { return surface_.get(); }
  wp_viewport* viewport() const { return viewport_.get(); }
  zcr_blending_v1* blending() const { return blending_.get(); }

  uint32_t buffer_id() const { return state_.buffer_id; }
  int32_t buffer_scale() const { return state_.buffer_scale; }
  float opacity() const { return state_.opacity; }
  bool use_blending() const { return state_.use_blending; }

  const std::vector<uint32_t>& entered_outputs() const {
    return entered_outputs_;
  }

  bool has_explicit_release_callback() const {
    return !explicit_release_callback_.is_null();
  }
  void set_explicit_release_callback(ExplicitReleaseCallback callback) {
    explicit_release_callback_ = callback;
  }

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
  void SetAcquireFence(gfx::GpuFenceHandle acquire_fence);

  // Attaches the given wl_buffer to the underlying wl_surface at (0, 0).
  // Returns true if wl_surface.attach will be called in ApplyPendingStates().
  bool AttachBuffer(WaylandBufferHandle* buffer_handle);

  // Describes where the surface needs to be repainted according to
  // |buffer_pending_damage_region|, which should be in buffer coordinates (px).
  void UpdateBufferDamageRegion(const gfx::Rect& damage_px);

  // Sets an optional transformation for how the Wayland compositor interprets
  // the contents of the buffer attached to this surface.
  void SetBufferTransform(gfx::OverlayTransform transform);

  // Sets the |buffer_scale| (with respect to the scale factor used by the GPU
  // process) for the next submitted buffer. This helps Wayland compositor to
  // determine buffer size in dip (GPU operates in pixels. So, when buffers are
  // created, their requested size is in pixels).
  void SetSurfaceBufferScale(float scale);

  // Sets the region that is opaque on this surface in physical pixels. This is
  // expected to be called whenever the region that the surface span changes or
  // the opacity changes. Rects in |region_px| are specified surface-local, in
  // physical pixels.  If |region_px| is nullptr or empty, the opaque region is
  // reset to empty.
  void SetOpaqueRegion(const std::vector<gfx::Rect>* region_px);

  // Sets the input region on this surface in physical pixels.
  // The input region indicates which parts of the surface accept pointer and
  // touch input events. This is expected to be called from ToplevelWindow
  // whenever the region that the surface span changes or window state changes
  // when custom frame is used.  If |region_px| is nullptr, the input region is
  // reset to cover the entire wl_surface.
  void SetInputRegion(const gfx::Rect* region_px);

  // Set the source rectangle of the associated wl_surface.
  // See:
  // https://cgit.freedesktop.org/wayland/wayland-protocols/tree/stable/viewporter/viewporter.xml
  // If |src_rect| is empty, the source rectangle is unset.
  // Note this method does not send corresponding wayland requests until
  // attaching the next buffer.
  void SetViewportSource(const gfx::RectF& src_rect);

  // Sets the opacity of the wl_surface using zcr_blending_v1_set_alpha.
  // See: alpha-compositing-unstable-v1.xml
  void SetOpacity(const float opacity);

  // Sets the blending equation of the wl_surface using
  // zcr_blending_v1_set_blending. See: alpha-compositing-unstable-v1.xml
  void SetBlending(const bool use_blending);

  // Set the destination size of the associated wl_surface according to
  // |dest_size_px|, which should be in physical pixels.
  // Note this method sends corresponding wayland requests immediately because
  // it does not need a new buffer attach to take effect.
  void SetViewportDestination(const gfx::SizeF& dest_size_px);

  // Creates a wl_subsurface relating this surface and a parent surface,
  // |parent|. Callers take ownership of the wl_subsurface.
  wl::Object<wl_subsurface> CreateSubsurface(WaylandSurface* parent);

  // When display is removed, the WaylandOutput from `entered_outputs_` should
  // be removed.
  void RemoveEnteredOutput(uint32_t id);

  // Sets the priority hint for the overlay that is committed via this surface.
  void SetOverlayPriority(gfx::OverlayPriorityHint priority_hint);

  // Sets the rounded clip bounds for this surface.
  void SetRoundedClipBounds(const gfx::RRectF& rounded_clip_bounds);

  // Validates the |pending_state_| and generates the corresponding requests.
  // Then copy |pending_states_| to |states_|.
  void ApplyPendingState();

  // Commits the underlying wl_surface, triggers a wayland connection flush if
  // |flush| is true.
  void Commit(bool flush = true);

  // Workaround used by GLSurfaceWayland when libgbm is not available. Causes
  // SetSurfaceBufferScale() SetOpaqueRegion(), and SetInputRegion() to take
  // effect immediately.
  void SetApplyStateImmediately();

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandWindowTest,
                           DoesNotCreateSurfaceSyncOnCommitWithoutBuffers);
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

  struct State {
    State();
    State(const State& other) = delete;
    State& operator=(State& other);
    ~State();

    std::vector<gfx::Rect> damage_px;
    std::vector<gfx::Rect> opaque_region_px;
    absl::optional<gfx::Rect> input_region_px = absl::nullopt;

    // The acquire gpu fence to associate with the surface buffer.
    gfx::GpuFenceHandle acquire_fence;

    uint32_t buffer_id = 0;
    // Note that this wl_buffer ptr is never cleared, even when the
    // buffer_handle owning this wl_buffer is destroyed. Accessing this field
    // should ensure wl_buffer exists by calling
    // WaylandBufferManagerHost::EnsureBufferHandle(buffer_id).
    wl_buffer* buffer = nullptr;
    gfx::Size buffer_size_px;

    // Current scale factor of a next attached buffer used by the GPU process.
    int32_t buffer_scale = 1;

    // Transformation for how the compositor interprets the contents of the
    // buffer.
    gfx::OverlayTransform buffer_transform = gfx::OVERLAY_TRANSFORM_NONE;

    // Following fields are used to help determine the damage_region in
    // surface-local coordinates if wl_surface_damage_buffer() is not available.
    // Normalized bounds of the buffer to be displayed in |viewport_px|.
    // If empty, no cropping is applied.
    gfx::RectF crop = {0.f, 0.f};

    // Current size of the destination of the viewport in physical pixels.
    // Wayland compositor will scale the (cropped) buffer content to fit the
    // |viewport_px|.
    // If empty, no scaling is applied.
    gfx::SizeF viewport_px = {0, 0};

    // The opacity of the wl_surface used to call zcr_blending_v1_set_alpha.
    float opacity = 1.f;

    // The blending equation of the wl_surface used to call
    // zcr_blending_v1_set_blending.
    bool use_blending = true;

    gfx::RRectF rounded_clip_bounds;
    gfx::OverlayPriorityHint priority_hint = gfx::OverlayPriorityHint::kRegular;
  };

  // Tracks the last sent src and dst values across wayland protocol s.t. we
  // skip resending them when possible.
  wl_fixed_t src_set_[4] = {wl_fixed_from_int(-1), wl_fixed_from_int(-1),
                            wl_fixed_from_int(-1), wl_fixed_from_int(-1)};
  float dst_set_[2] = {-1.f, -1.f};
  // Tracks the last sent surface_scale value s.t. we skip resending.
  int32_t surface_scale_set_ = 1;

  wl::Object<wl_region> CreateAndAddRegion(
      const std::vector<gfx::Rect>& region_px,
      int32_t buffer_scale);

  // wl_surface states that are stored in Wayland client. It moves to |state_|
  // on ApplyPendingState().
  State pending_state_;

  // wl_surface states that are either active or will be active once Commit() is
  // called.
  State state_;

  bool SurfaceSubmissionInPixelCoordinates() const;

  // Creates (if not created) the synchronization surface and returns a pointer
  // to it.
  zwp_linux_surface_synchronization_v1* GetSurfaceSync();
  augmented_surface* GetAugmentedSurface();

  WaylandConnection* const connection_;
  WaylandWindow* root_window_ = nullptr;
  bool apply_state_immediately_ = false;
  wl::Object<wl_surface> surface_;
  wl::Object<wp_viewport> viewport_;
  wl::Object<zcr_blending_v1> blending_;
  wl::Object<zwp_linux_surface_synchronization_v1> surface_sync_;
  wl::Object<overlay_prioritized_surface> overlay_priority_surface_;
  wl::Object<augmented_surface> augmented_surface_;
  base::flat_map<zwp_linux_buffer_release_v1*, ExplicitReleaseInfo>
      linux_buffer_releases_;
  ExplicitReleaseCallback explicit_release_callback_;

  // For top level window, stores outputs that the window is currently rendered
  // at.
  //
  // Not used by popups.  When sub-menus are hidden and shown again, Wayland
  // 'repositions' them to wrong outputs by sending them leave and enter
  // events so their list of entered outputs becomes meaningless after they have
  // been hidden at least once.  To determine which output the popup belongs to,
  // we ask its parent.
  std::vector<uint32_t> entered_outputs_;

  void ExplicitRelease(struct zwp_linux_buffer_release_v1* linux_buffer_release,
                       base::ScopedFD fence);

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
