// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"

struct wp_content_type_v1;
struct wp_fractional_scale_v1;
struct zwp_linux_buffer_release_v1;
struct zcr_blending_v1;

namespace ui {

class WaylandConnection;
class WaylandOutput;
class WaylandWindow;
class WaylandBufferHandle;
class WaylandZcrColorManagementSurface;
class WaylandZAuraSurface;

// Wrapper of a wl_surface, owned by a WaylandWindow or a WlSubsurface.
class WaylandSurface {
 public:
  using ExplicitReleaseCallback =
      base::OnceCallback<void(wl_buffer*, base::ScopedFD)>;

  WaylandSurface(WaylandConnection* connection, WaylandWindow* root_window);
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
  float opacity() const { return state_.opacity; }
  bool use_blending() const { return state_.use_blending; }

  const std::vector<uint32_t>& entered_outputs() const {
    return entered_outputs_;
  }

  // Creates a zaura_surface extension object for this surface if it does not
  // already exist and is supported by the server. Returns the surface
  // extension.
  WaylandZAuraSurface* CreateZAuraSurface();

  // Resets the zaura_surface extension object for this surface if it exists.
  // This may be done for the purposes of unmapping the toplevel surface when
  // hiding the window (see crrev.com/c/3628350/comment/b6315793_85f0b073).
  void ResetZAuraSurface();

  WaylandZAuraSurface* zaura_surface() { return zaura_surface_.get(); }

  // Requests an explicit release for the next commit.
  void RequestExplicitRelease(ExplicitReleaseCallback callback);

  // Returns an id that identifies the |wl_surface_|.
  uint32_t get_surface_id() const { return surface_ ? surface_.id() : 0u; }

  // Returns a gfx::AcceleratedWidget that identifies the WaylandWindow that
  // this WaylandSurface belongs to.
  gfx::AcceleratedWidget get_widget() const;

  // Initializes the WaylandSurface and returns true iff success.
  // This may return false if a wl_surface could not be created, for example.
  bool Initialize();

  // Unsets |root_window_|. This is intended to be used in special cases, where
  // the underlying wl_surface must be kept alive with no root window associated
  // (e.g: window/tab dragging sessions).
  void UnsetRootWindow();
  void SetRootWindow(WaylandWindow* window);

  // Attaches the given wl_buffer to the underlying wl_surface at (0, 0).
  // Returns true if wl_surface.attach will be called in ApplyPendingStates().
  bool AttachBuffer(WaylandBufferHandle* buffer_handle);

  // Describes where the surface needs to be repainted according to
  // |buffer_pending_damage_region|, which should be in buffer coordinates (px).
  void UpdateBufferDamageRegion(const gfx::Rect& damage_px);

  // Sets a non-null in-fence, must be combined with an AttachBuffer() and a
  // Commit().
  void set_acquire_fence(gfx::GpuFenceHandle acquire_fence);

  // Sets an optional transformation for how the Wayland compositor interprets
  // the contents of the buffer attached to this surface.
  void set_buffer_transform(gfx::OverlayTransform transform) {
    DCHECK(!apply_state_immediately_);
    DCHECK(transform != gfx::OVERLAY_TRANSFORM_INVALID);
    pending_state_.buffer_transform = transform;
    return;
  }

  // Sets the |buffer_scale| (with respect to the scale factor used by the GPU
  // process) for the next submitted buffer. This helps Wayland compositor to
  // determine buffer size in dip (GPU operates in pixels. So, when buffers are
  // created, their requested size is in pixels).
  void set_surface_buffer_scale(float scale);

  // Sets the region that is opaque on this surface in physical pixels. This is
  // expected to be called whenever the region that the surface span changes or
  // the opacity changes. Rects in |region_px| are specified surface-local, in
  // physical pixels.  If |region_px| is nullopt or empty, the opaque region is
  // reset to empty.
  void set_opaque_region(std::optional<std::vector<gfx::Rect>> region_px);

  // Sets the input region on this surface in physical pixels.
  // The input region indicates which parts of the surface accept pointer and
  // touch input events. This is expected to be called from ToplevelWindow
  // whenever the region that the surface span changes or window state changes
  // when custom frame is used.  If |region_px| is nullptr, the input region is
  // reset to cover the entire wl_surface.
  void set_input_region(std::optional<std::vector<gfx::Rect>> region_px);

  // Set the crop uv of the attached wl_buffer.
  // Unlike wp_viewport.set_source, this crops the buffer prior to
  // |buffer_transform| being applied to the buffer, it will be transformed s.t.
  // wp_viewport.source is called with correct params.
  // See:
  // https://cgit.freedesktop.org/wayland/wayland-protocols/tree/stable/viewporter/viewporter.xml
  // If |crop| is empty, the source rectangle is unset.
  // Note this method does not send corresponding wayland requests until
  // attaching the next buffer.
  void set_buffer_crop(const gfx::RectF& crop) {
    DCHECK(!apply_state_immediately_);
    pending_state_.crop = crop == gfx::RectF{1.f, 1.f} ? gfx::RectF() : crop;
  }

  // Sets the opacity of the wl_surface using zcr_blending_v1_set_alpha.
  // See: alpha-compositing-unstable-v1.xml
  void set_opacity(const float opacity) {
    DCHECK(!apply_state_immediately_);
    if (blending())
      pending_state_.opacity = opacity;
  }

  // Sets the blending equation of the wl_surface using
  // zcr_blending_v1_set_blending. See: alpha-compositing-unstable-v1.xml
  void set_blending(const bool use_blending) {
    DCHECK(!apply_state_immediately_);
    if (blending())
      pending_state_.use_blending = use_blending;
  }

  // Set the destination size of the associated wl_surface according to
  // |dest_size_px|, which should be in physical pixels.
  // Note this method sends corresponding wayland requests immediately because
  // it does not need a new buffer attach to take effect.
  void set_viewport_destination(const gfx::SizeF& dest_size_px) {
    DCHECK(!apply_state_immediately_);
    pending_state_.viewport_px = dest_size_px;
  }

  // Sets the priority hint for the overlay that is committed via this surface.
  void set_overlay_priority(gfx::OverlayPriorityHint priority_hint) {
    if (overlay_priority_surface())
      pending_state_.priority_hint = priority_hint;
  }

  // Sets the rounded clip bounds for this surface.
  void set_rounded_clip_bounds(const gfx::RRectF& rounded_clip_bounds) {
    if (get_augmented_surface())
      pending_state_.rounded_clip_bounds = rounded_clip_bounds;
  }

  // Sets the background color for this surface, which will be blended with the
  // wl_buffer contents during the compositing step on the Wayland compositor
  // side.
  void set_background_color(std::optional<SkColor4f> background_color) {
    if (get_augmented_surface())
      pending_state_.background_color = background_color;
  }

  // Sets the clip rect for this surface.
  void set_clip_rect(std::optional<gfx::RectF> clip_rect) {
    if (get_augmented_surface()) {
      pending_state_.clip_rect = clip_rect;
    }
  }

  // Sets whether this surface contains a video.
  void set_contains_video(bool contains_video) {
    pending_state_.contains_video = contains_video;
  }

  void set_frame_trace_id(int64_t frame_trace_id) {
    pending_state_.frame_trace_id = frame_trace_id;
  }

  // Creates a wl_subsurface relating this surface and a parent surface,
  // |parent|. Callers take ownership of the wl_subsurface.
  wl::Object<wl_subsurface> CreateSubsurface(WaylandSurface* parent);

  // When display is removed, the WaylandOutput from `entered_outputs_` should
  // be removed.
  void RemoveEnteredOutput(uint32_t id);

  // Set surface ColorSpace
  void set_color_space(gfx::ColorSpace color_space);

  // Validates the |pending_state_| and generates the corresponding requests.
  // Then copy |pending_states_| to |states_|.
  // Returns whether or not changes require a commit to the wl_surface.
  bool ApplyPendingState();

  // Commits the underlying wl_surface, triggers a wayland connection flush if
  // |flush| is true.
  void Commit(bool flush = true);

  // Workaround used by GLSurfaceWayland when libgbm is not available. Causes
  // SetSurfaceBufferScale() SetOpaqueRegion(), and SetInputRegion() to take
  // effect immediately.
  void ForceImmediateStateApplication();

  // Asks the Wayland compositor to enable or disable the keyboard shortcuts
  // inhibition for this surface. i.e: to receive key events even if they match
  // compositor accelerators, e.g: Alt+Tab, etc.
  void SetKeyboardShortcutsInhibition(bool enabled);

  // Set the trusted damage flag on this surface to be active, if the surface
  // augmenter protocol is available. This only needs to be set on the root
  // surface for a window.
  void EnableTrustedDamageIfPossible();

  std::optional<float> preferred_scale_factor() const {
    return preferred_scale_factor_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandWindowTest,
                           DoesNotCreateSurfaceSyncOnCommitWithoutBuffers);
  FRIEND_TEST_ALL_PREFIXES(PerSurfaceScaleWaylandWindowTest,
                           UiScale_HandleFontScaleChange);
  FRIEND_TEST_ALL_PREFIXES(PerSurfaceScaleWaylandWindowTest,
                           UiScale_HandleServerTriggeredBoundsChange);
  FRIEND_TEST_ALL_PREFIXES(PerSurfaceScaleWaylandWindowTest,
                           UiScale_InitScaleAndBounds);
  FRIEND_TEST_ALL_PREFIXES(PerSurfaceScaleWaylandWindowTest,
                           UiScale_HandlePopupGeometry);
  // Holds information about each explicit synchronization buffer release.
  struct ExplicitReleaseInfo {
    ExplicitReleaseInfo(
        wl::Object<zwp_linux_buffer_release_v1>&& linux_buffer_release,
        wl_buffer* buffer,
        ExplicitReleaseCallback explicit_release_callback);
    ~ExplicitReleaseInfo();

    ExplicitReleaseInfo(const ExplicitReleaseInfo&) = delete;
    ExplicitReleaseInfo& operator=(const ExplicitReleaseInfo&) = delete;

    ExplicitReleaseInfo(ExplicitReleaseInfo&&);
    ExplicitReleaseInfo& operator=(ExplicitReleaseInfo&&);

    wl::Object<zwp_linux_buffer_release_v1> linux_buffer_release;
    // The buffer associated with this explicit release.
    raw_ptr<wl_buffer, AcrossTasksDanglingUntriaged> buffer;
    // The associated release callback with this request.
    ExplicitReleaseCallback explicit_release_callback;
  };

  struct State {
    State();
    State(const State& other) = delete;
    State& operator=(const State& other);
    ~State();

    std::vector<gfx::Rect> damage_px;
    std::vector<gfx::Rect> opaque_region_px;
    std::vector<gfx::Rect> input_region_px;

    // The current color space of the surface.
    scoped_refptr<WaylandZcrColorSpace> color_space = nullptr;

    // The acquire gpu fence to associate with the surface buffer.
    gfx::GpuFenceHandle acquire_fence;

    uint32_t buffer_id = 0;
    // Note that this wl_buffer ptr is never cleared, even when the
    // buffer_handle owning this wl_buffer is destroyed. Accessing this field
    // should ensure wl_buffer exists by calling
    // WaylandBufferManagerHost::EnsureBufferHandle(buffer_id).
    raw_ptr<wl_buffer, AcrossTasksDanglingUntriaged> buffer = nullptr;
    gfx::Size buffer_size_px;

    // The buffer scale refers to the ratio between the buffer size and the
    // window size. This allows support for high-DPI displays.
    float buffer_scale_float = 1;

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

    // Optional background color for this surface. This information
    // can be used by Wayland compositor to correctly display delegated textures
    // which require background color applied.
    std::optional<SkColor4f> background_color;

    // Optional clip rect for this surface on surface space coordinates.
    std::optional<gfx::RectF> clip_rect;

    // Whether or not this surface contains video, for wp_content_type_v1.
    bool contains_video = false;

    // Trace ID used to associate tracing data of the wayland client and server
    // side for frame submission tracking. It is received from the GPU process
    // and sent to the wayland server.
    int64_t frame_trace_id = -1;
  };

  // The wayland scale refers to the scale factor between the buffer coordinates
  // and Wayland surface coordinates. When SurfaceSubmissionInPixelCoordinates
  // is true, this is always 1. Otherwise, this is buffer_scale_float unless the
  // value is less than 1. In that case 1 is returned. Additionally, if
  // viewporter surface scaling is disabled, the value will be rounded up to the
  // next integer.
  float GetWaylandScale(const State& state);

  bool IsViewportScaled(const State& state);

  // Tracks the last sent src and dst values across wayland protocol s.t. we
  // skip resending them when possible.
  wl_fixed_t src_set_[4] = {wl_fixed_from_int(-1), wl_fixed_from_int(-1),
                            wl_fixed_from_int(-1), wl_fixed_from_int(-1)};
  float dst_set_[2] = {-1.f, -1.f};
  // Tracks the last sent surface_scale value s.t. we skip resending.
  // This is used by wl_surface_set_buffer_scale which only supports integer
  // scales.
  int32_t surface_scale_set_ = 1;

  wl::Object<wl_region> CreateAndAddRegion(
      const std::vector<gfx::Rect>& region_px,
      float buffer_scale);

  // wl_surface states that are stored in Wayland client. It moves to |state_|
  // on ApplyPendingState().
  State pending_state_;

  // wl_surface states that are either active or will be active once Commit() is
  // called.
  State state_;

  // Creates (if not created) the synchronization surface and returns a pointer
  // to it.
  zwp_linux_surface_synchronization_v1* GetOrCreateSurfaceSync();
  augmented_surface* get_augmented_surface() {
    return augmented_surface_.get();
  }

  const raw_ptr<WaylandConnection> connection_;
  raw_ptr<WaylandWindow> root_window_ = nullptr;
  bool apply_state_immediately_ = false;
  wl::Object<wl_surface> surface_;
  wl::Object<wp_viewport> viewport_;
  wl::Object<zcr_blending_v1> blending_;
  wl::Object<zwp_linux_surface_synchronization_v1> surface_sync_;
  wl::Object<overlay_prioritized_surface> overlay_priority_surface_;
  wl::Object<augmented_surface> augmented_surface_;
  wl::Object<wp_content_type_v1> content_type_;
  wl::Object<wp_fractional_scale_v1> fractional_scale_;
  std::unique_ptr<WaylandZcrColorManagementSurface>
      zcr_color_management_surface_;
  std::unique_ptr<WaylandZAuraSurface> zaura_surface_;
  base::flat_map<zwp_linux_buffer_release_v1*, ExplicitReleaseInfo>
      linux_buffer_releases_;
  ExplicitReleaseCallback next_explicit_release_request_;

  // A cached copy of connection->SurfaceSubmissionInPixelCoordinates(). While
  // it is technically possible to handle this value as mutable, in practice
  // it's constant.
  const bool surface_submission_in_pixel_coordinates_;

  // Same as above except it caches
  // connection->UseViewporterSurfaceScaling().
  const bool use_viewporter_surface_scaling_;

  // For top level window, stores outputs that the window is currently rendered
  // at.
  //
  // Not used by popups.  When sub-menus are hidden and shown again, Wayland
  // 'repositions' them to wrong outputs by sending them leave and enter
  // events so their list of entered outputs becomes meaningless after they have
  // been hidden at least once.  To determine which output the popup belongs to,
  // we ask its parent.
  std::vector<uint32_t> entered_outputs_;

  // Holds the preferred buffer factor for this surface, if any was received
  // through wp-fractional-scale-v1 protocol, when available.
  std::optional<float> preferred_scale_factor_;

  void ExplicitRelease(zwp_linux_buffer_release_v1* linux_buffer_release,
                       base::ScopedFD fence);

  // wl_surface_listener callbacks:
  static void OnEnter(void* data, wl_surface* surface, wl_output* output);
  static void OnLeave(void* data, wl_surface* surface, wl_output* output);

  // wp_fractional_scale_v1_listener callbacks:
  static void OnPreferredScale(void* data,
                               wp_fractional_scale_v1* fractional_scale,
                               uint32_t scale);

  // zwp_linux_buffer_release_v1_listener callbacks:
  static void OnFencedRelease(void* data,
                              zwp_linux_buffer_release_v1* buffer_release,
                              int32_t fence);
  static void OnImmediateRelease(void* data,
                                 zwp_linux_buffer_release_v1* buffer_release);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
