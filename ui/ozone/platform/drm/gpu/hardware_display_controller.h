// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_CONTROLLER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace gfx {
class Point;
}

namespace ui {

class CrtcController;
class DrmFramebuffer;
class DrmDumbBuffer;
class DrmDevice;

// The HDCOz will handle modesettings and scannout operations for hardware
// devices.
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
                            const gfx::Point& origin);
  ~HardwareDisplayController();

  // Performs the initial CRTC configuration. If successful, it will display the
  // framebuffer for |primary| with |mode|.
  bool Modeset(const DrmOverlayPlane& primary, drmModeModeInfo mode);

  // Performs a CRTC configuration re-using the modes from the CRTCs.
  bool Enable(const DrmOverlayPlane& primary);

  // Disables the CRTC.
  void Disable();

  // Schedules the |overlays|' framebuffers to be displayed on the next vsync
  // event. The event will be posted on the graphics card file descriptor |fd_|
  // and it can be read and processed by |drmHandleEvent|. That function can
  // define the callback for the page flip event. A generic data argument will
  // be presented to the callback. We use that argument to pass in the HDCO
  // object the event belongs to.
  //
  // Between this call and the callback, the framebuffers used in this call
  // should not be modified in any way as it would cause screen tearing if the
  // hardware performed the flip. Note that the frontbuffer should also not
  // be modified as it could still be displayed.
  //
  // Note that this function does not block. Also, this function should not be
  // called again before the page flip occurrs.
  void SchedulePageFlip(DrmOverlayPlaneList plane_list,
                        SwapCompletionOnceCallback submission_callback,
                        PresentationOnceCallback presentation_callback);

  // Returns true if the page flip with the |plane_list| would succeed. This
  // doesn't change any state.
  bool TestPageFlip(const DrmOverlayPlaneList& plane_list);

  // Return the supported modifiers for |fourcc_format| for this
  // controller.
  std::vector<uint64_t> GetFormatModifiers(uint32_t fourcc_format) const;

  // Return the supported modifiers for |fourcc_format| for this
  // controller to be used for modeset buffers. Currently, this only exists
  // because we can't provide valid AFBC buffers during modeset.
  // See https://crbug.com/852675
  // TODO: Remove this.
  std::vector<uint64_t> GetFormatModifiersForModesetting(
      uint32_t fourcc_format) const;

  // Moves the hardware cursor to |location|.
  void MoveCursor(const gfx::Point& location);

  // Set the hardware cursor to show the contents of |bitmap| at |location|.
  void SetCursor(SkBitmap bitmap);

  void AddCrtc(std::unique_ptr<CrtcController> controller);
  std::unique_ptr<CrtcController> RemoveCrtc(
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc);
  bool HasCrtc(const scoped_refptr<DrmDevice>& drm, uint32_t crtc) const;
  bool IsMirrored() const;
  bool IsDisabled() const;
  gfx::Size GetModeSize() const;

  gfx::Point origin() const { return origin_; }
  void set_origin(const gfx::Point& origin) { origin_ = origin; }

  base::TimeDelta GetRefreshInterval() const;
  base::TimeTicks GetTimeOfLastFlip() const;

  const std::vector<std::unique_ptr<CrtcController>>& crtc_controllers() const {
    return crtc_controllers_;
  }

  scoped_refptr<DrmDevice> GetDrmDevice() const;

  void OnPageFlipComplete(
      DrmOverlayPlaneList pending_planes,
      const gfx::PresentationFeedback& presentation_feedback);

 private:
  void OnModesetComplete(const DrmOverlayPlane& primary);
  bool ScheduleOrTestPageFlip(const DrmOverlayPlaneList& plane_list,
                              scoped_refptr<PageFlipRequest> page_flip_request,
                              std::unique_ptr<gfx::GpuFence>* out_fence);
  void AllocateCursorBuffers();
  DrmDumbBuffer* NextCursorBuffer();
  void UpdateCursorImage();
  void UpdateCursorLocation();
  void ResetCursor();
  void DisableCursor();

  HardwareDisplayPlaneList owned_hardware_planes_;

  // Stores the CRTC configuration. This is used to identify monitors and
  // configure them.
  std::vector<std::unique_ptr<CrtcController>> crtc_controllers_;

  // Location of the controller on the screen.
  gfx::Point origin_;

  scoped_refptr<PageFlipRequest> page_flip_request_;
  DrmOverlayPlaneList current_planes_;
  base::TimeTicks time_of_last_flip_;

  std::unique_ptr<DrmDumbBuffer> cursor_buffers_[2];
  gfx::Point cursor_location_;
  int cursor_frontbuffer_ = 0;
  DrmDumbBuffer* current_cursor_ = nullptr;

  bool is_disabled_;

  base::WeakPtrFactory<HardwareDisplayController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayController);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_CONTROLLER_H_
