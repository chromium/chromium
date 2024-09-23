// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_CONTROLLER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/tile_property.h"
#include "ui/ozone/platform/drm/gpu/crtc_commit_request.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"
#include "ui/ozone/platform/drm/gpu/page_flip_watchdog.h"
#include "ui/ozone/public/drm_modifiers_filter.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace gfx {
class Point;
struct GpuFenceHandle;
}  // namespace gfx

namespace ui {

class CrtcController;
class DrmFramebuffer;
class DrmDumbBuffer;
class DrmDevice;

// The HDC will handle modesetting and scanout operations for hardware devices.
//
// In the DRM world there are 3 components that need to be paired up to be able
// to display an image to the monitor: CRTC (cathode ray tube controller),
// encoder and connector. The CRTC determines which framebuffer to read, when
// to scanout and where to scanout. Encoders converts the stream from the CRTC
// to the appropriate format for the connector. The connector is the physical
// connection that monitors connect to.
//
// There is no 1:1:1 pairing for these components. It is possible for an encoder
// to be compatible to multiple CRTCs and each connector can be used with
// multiple encoders. In addition, it is possible to use one CRTC with multiple
// connectors such that we can display the same image on multiple monitors.
//
// For example, the following configuration shows 2 different screens being
// initialized separately.
// -------------      -------------
// | Connector |      | Connector |
// |   HDMI    |      |    VGA    |
// -------------      -------------
//       ^                  ^
//       |                  |
// -------------      -------------
// |  Encoder1  |     |  Encoder2 |
// -------------      -------------
//       ^                  ^
//       |                  |
// -------------      -------------
// |   CRTC1   |      |   CRTC2   |
// -------------      -------------
//
// In the following configuration 2 different screens are associated with the
// same CRTC, so on scanout the same framebuffer will be displayed on both
// monitors.
// -------------      -------------
// | Connector |      | Connector |
// |   HDMI    |      |    VGA    |
// -------------      -------------
//       ^                  ^
//       |                  |
// -------------      -------------
// |  Encoder1  |     |  Encoder2 |
// -------------      -------------
//       ^                  ^
//       |                  |
//      ----------------------
//      |        CRTC1       |
//      ----------------------
//
// Note that it is possible to have more connectors than CRTCs which means that
// only a subset of connectors can be active independently, showing different
// framebuffers. Though, in this case, it would be possible to have all
// connectors active if some use the same CRTC to mirror the display.
class HardwareDisplayController {
 public:
  HardwareDisplayController(std::unique_ptr<CrtcController> controller,
                            const gfx::Point& origin,
                            raw_ptr<DrmModifiersFilter> drm_modifiers_filter);

  HardwareDisplayController(const HardwareDisplayController&) = delete;
  HardwareDisplayController& operator=(const HardwareDisplayController&) =
      delete;

  ~HardwareDisplayController();

  // Gets the props required to modeset a CRTC with a |mode| onto
  // |commit_request|.
  void GetModesetProps(CommitRequest* commit_request,
                       const DrmOverlayPlaneList& modeset_planes,
                       const drmModeModeInfo& mode,
                       bool enable_vrr);
  // Gets the props required to enable/disable a CRTC onto |commit_request|.
  void GetEnableProps(CommitRequest* commit_request,
                      const DrmOverlayPlaneList& modeset_planes);
  void GetDisableProps(CommitRequest* commit_request);

  // Updates state of the controller after modeset/enable/disable is performed.
  void UpdateState(const CrtcCommitRequest& crtc_request);

  // Schedules the |overlays|' framebuffers to be displayed on the next vsync
  // event. The event will be posted on the graphics card file descriptor |fd_|
  // and it can be read and processed by |drmHandleEvent|. That function can
  // define the callback for the page flip event. A generic data argument will
  // be presented to the callback. We use that argument to pass in the HDC
  // object the event belongs to.
  //
  // Between this call and the callback, the framebuffers used in this call
  // should not be modified in any way as it would cause screen tearing if the
  // hardware performed the flip. Note that the frontbuffer should also not
  // be modified as it could still be displayed.
  //
  // Note that this function does not block. Also, this function should not be
  // called again before the page flip occurs.
  void SchedulePageFlip(DrmOverlayPlaneList plane_list,
                        SwapCompletionOnceCallback submission_callback,
                        PresentationOnceCallback presentation_callback);

  // Returns true if the page flip with the |plane_list| would succeed. This
  // doesn't change any state.
  bool TestPageFlip(const DrmOverlayPlaneList& plane_list);

  // Perform a test commit to |mode| on the CRTC to determine if it can be
  // configured without a modeset.
  bool TestSeamlessMode(int32_t crtc_id, const drmModeModeInfo& mode);

  // Return the supported modifiers for |fourcc_format| for this controller.
  std::vector<uint64_t> GetSupportedModifiers(uint32_t fourcc_format,
                                              bool is_modeset = false) const;

  std::vector<uint64_t> GetFormatModifiersForTestModeset(
      uint32_t fourcc_format);

  void UpdatePreferredModifierForFormat(gfx::BufferFormat buffer_format,
                                        uint64_t modifier);

  // Moves the hardware cursor to |location|.
  void MoveCursor(const gfx::Point& location);

  // Set the hardware cursor to show the contents of |bitmap| at |location|.
  void SetCursor(SkBitmap bitmap);

  void AddCrtc(std::unique_ptr<CrtcController> controller);
  std::unique_ptr<CrtcController> RemoveCrtc(
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc);
  bool HasCrtc(const scoped_refptr<DrmDevice>& drm, uint32_t crtc) const;
  // Returns true if the controllers are configured for hardware mirroring. Note
  // that controllers for tiled display will not support hardware mirroring.
  bool IsMirrored() const;
  // Returns true if any of the controllers are enabled.
  bool IsEnabled() const;
  bool IsTiled() const;
  gfx::Size GetModeSize() const;

  gfx::Point origin() const { return origin_; }
  void set_origin(const gfx::Point& origin) { origin_ = origin; }

  float GetRefreshRate() const;
  base::TimeDelta GetRefreshInterval() const;
  base::TimeTicks GetTimeOfLastFlip() const;

  const std::vector<std::unique_ptr<CrtcController>>& crtc_controllers() const {
    return crtc_controllers_;
  }

  scoped_refptr<DrmDevice> GetDrmDevice() const;

  void OnPageFlipComplete(
      int modeset_sequence,
      DrmOverlayPlaneList pending_planes,
      const gfx::PresentationFeedback& presentation_feedback);

  // Adds trace records to |context|.
  void WriteIntoTrace(perfetto::TracedValue context) const;

  size_t NumOfSupportedCursorSizesForTesting() const;
  gfx::Size CurrentCursorSizeForTesting() const;

  std::optional<TileProperty> GetTileProperty() const { return tile_property_; }

 private:
  // These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum PageFlipResult {
    // Indicates that the page flip was committed successfully.
    kSuccess = 0,
    // Indicates that the page flip failed because we could not assign
    // planes.
    kFailedPlaneAssignment = 1,
    // Indicates that we assigned planes but the DRM commit failed.
    kFailedCommit = 2,
    kMaxValue = kFailedCommit,
  };
  // Loops over |crtc_controllers_| and save their props into |commit_request|
  // to be enabled/modeset.
  void GetModesetPropsForCrtcs(CommitRequest* commit_request,
                               const DrmOverlayPlaneList& modeset_planes,
                               bool use_current_crtc_mode,
                               const drmModeModeInfo& mode,
                               std::optional<bool> enable_vrr);
  // Returns true if |controller| is a non-primary tiled controlelr and should
  // be disabled as part of the modeset with a non-tiled |mode|.
  bool ShouldDisableNonprimaryTileController(
      const CrtcController& controller,
      const drmModeModeInfo& mode,
      const bool use_current_crtc_mode) const;
  void OnModesetComplete(const DrmOverlayPlaneList& modeset_planes);
  PageFlipResult ScheduleOrTestPageFlip(
      const DrmOverlayPlaneList& plane_list,
      scoped_refptr<PageFlipRequest> page_flip_request,
      gfx::GpuFenceHandle* release_fence);
  void AllocateCursorBuffers();
  DrmDumbBuffer* NextCursorBuffer(const SkBitmap& image);
  void UpdateCursorImage();
  void UpdateCursorLocation();
  void ResetCursor();
  void DisableCursor();
  void InitSupportedCursorSizes();

  std::vector<uint64_t> GetFormatModifiers(uint32_t fourcc_format) const;

  HardwareDisplayPlaneList owned_hardware_planes_;

  // Stores the CRTC configuration. This is used to identify monitors and
  // configure them.
  std::vector<std::unique_ptr<CrtcController>> crtc_controllers_;

  // Location of the controller on the screen.
  gfx::Point origin_;

  scoped_refptr<PageFlipRequest> page_flip_request_;
  DrmOverlayPlaneList current_planes_;
  base::TimeTicks time_of_last_flip_;

  // Stores all the supported sizes for cursor plane.
  std::vector<gfx::Size> supported_cursor_sizes_;
  // |cursor_buffer_map_| stores active buffers for each
  // |supported_cursor_sizes_|.
  base::flat_map<gfx::Size,
                 std::vector<std::unique_ptr<DrmDumbBuffer>>,
                 CursorSizeComparator>
      cursor_buffer_map_;
  gfx::Point cursor_location_;
  raw_ptr<DrmDumbBuffer> current_cursor_ = nullptr;

  // Maps each fourcc_format to its preferred modifier which was generated
  // through modeset-test and updated in UpdatePreferredModifierForFormat().
  base::flat_map<uint32_t /*fourcc_format*/, uint64_t /*preferred_modifier*/>
      preferred_format_modifier_;

  // Used to crash the GPU process when unrecoverable failures occur.
  PageFlipWatchdog watchdog_;

  raw_ptr<DrmModifiersFilter> drm_modifiers_filter_;

  // If this object represents a tiled display, then the primary tile is denoted
  // by |tile_property_|.location.
  std::optional<TileProperty> tile_property_;

  base::WeakPtrFactory<HardwareDisplayController> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_CONTROLLER_H_
