// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DIRECT_COMPOSITION_SUPPORT_H_
#define UI_GL_DIRECT_COMPOSITION_SUPPORT_H_

#include <d3d11.h>
#include <dcomp.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/no_destructor.h"
#include "base/observer_list_threadsafe.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/mojom/dxgi_info.mojom.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gpu_switching_observer.h"

namespace gl {

class GLDisplayEGL;

GL_EXPORT void InitializeDirectComposition(GLDisplayEGL* display);
GL_EXPORT void ShutdownDirectComposition();

// Retrieves the global direct composition device. InitializeDirectComposition
// must be called on GPU process startup before the device is retrieved, and
// ShutdownDirectComposition must be called at process shutdown.
GL_EXPORT IDCompositionDevice2* GetDirectCompositionDevice();

// Returns true if direct composition is supported.  We prefer to use direct
// composition even without hardware overlays, because it allows us to bypass
// blitting by DWM to the window redirection surface by using a flip mode swap
// chain.  Overridden with --disable_direct_composition=1.
GL_EXPORT bool DirectCompositionSupported();

// Returns true if video overlays are supported and should be used. Overridden
// with --enable_direct_composition_video_overlays=1 and
// --disable_direct_composition_video_overlays=1. This function is thread safe.
GL_EXPORT bool DirectCompositionOverlaysSupported();

// Returns true if hardware overlays are supported. This function is thread
// safe.
GL_EXPORT bool DirectCompositionHardwareOverlaysSupported();

// After this is called, overlay support is disabled during the current GPU
// process' lifetime.
GL_EXPORT void DisableDirectCompositionOverlays();

// Returns true if zero copy decode swap chain is supported.
GL_EXPORT bool DirectCompositionDecodeSwapChainSupported();

// Returns true if scaled hardware overlays are supported.
GL_EXPORT bool DirectCompositionScaledOverlaysSupported();

// Returns preferred overlay format set when detecting overlay support.
GL_EXPORT DXGI_FORMAT GetDirectCompositionSDROverlayFormat();

// Returns overlay support flags for the given format.
// Caller should check for DXGI_OVERLAY_SUPPORT_FLAG_DIRECT and
// DXGI_OVERLAY_SUPPORT_FLAG_SCALING bits.
// This function is thread safe.
GL_EXPORT UINT GetDirectCompositionOverlaySupportFlags(DXGI_FORMAT format);

// Returns true if swap chain tearing flag is supported.
GL_EXPORT bool DXGISwapChainTearingSupported();

// Returns true if tearing should be used for dcomp root and video swap chains.
GL_EXPORT bool DirectCompositionSwapChainTearingEnabled();

// Returns true if waitable swap chain should be used to reduce display latency.
GL_EXPORT bool DXGIWaitableSwapChainEnabled();

// Returns the value passed to SetMaximumFrameLatency for waitable swap chains.
GL_EXPORT UINT GetDXGIWaitableSwapChainMaxQueuedFrames();

// Returns true if there is an HDR capable display connected.
GL_EXPORT bool DirectCompositionSystemHDREnabled();

// Returns the collected DXGI information.
GL_EXPORT gfx::mojom::DXGIInfoPtr GetDirectCompositionHDRMonitorDXGIInfo();

// Set direct composition swap chain failure so that direct composition is
// marked as unsupported from now on.
GL_EXPORT void SetDirectCompositionSwapChainFailed();

struct DirectCompositionOverlayWorkarounds {
  // Whether software video overlays i.e. swap chains used without hardware
  // overlay/MPO support are used or not.
  bool disable_sw_video_overlays = false;

  // Whether decode swap chains i.e. zero copy swap chains created from video
  // decoder textures are used or not.
  bool disable_decode_swap_chain = false;

  // On Intel GPUs where YUV overlays are supported, BGRA8 overlays are
  // supported as well but IDXGIOutput3::CheckOverlaySupport() returns
  // unsupported. So allow manually enabling BGRA8 overlay support.
  bool enable_bgra8_overlays_with_yuv_overlay_support = false;

  // Forces to enable NV12 overlay support regardless of the query results from
  // IDXGIOutput3::CheckOverlaySupport().
  bool force_nv12_overlay_support = false;

  // Forces to enable RGBA101010A2 overlay support regardless of the query
  // results from IDXGIOutput3::CheckOverlaySupport().
  bool force_rgb10a2_overlay_support = false;

  // Enable NV12 overlay support only when
  // DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 is supported.
  bool check_ycbcr_studio_g22_left_p709_for_nv12_support = false;
};
GL_EXPORT void SetDirectCompositionOverlayWorkarounds(
    const DirectCompositionOverlayWorkarounds& workarounds);

// Returns monitor size.
GL_EXPORT gfx::Size GetDirectCompositionPrimaryMonitorSize();

// Get the current number of all visible display monitors on the desktop.
GL_EXPORT int GetDirectCompositionNumMonitors();

// Testing helpers.
GL_EXPORT void SetDirectCompositionScaledOverlaysSupportedForTesting(
    bool value);
GL_EXPORT void SetDirectCompositionOverlayFormatUsedForTesting(
    DXGI_FORMAT format);
GL_EXPORT void SetDirectCompositionMonitorInfoForTesting(
    int num_monitors,
    const gfx::Size& primary_monitor_size);

class GL_EXPORT DirectCompositionOverlayCapsObserver
    : public base::CheckedObserver {
 public:
  virtual void OnOverlayCapsChanged() = 0;

 protected:
  DirectCompositionOverlayCapsObserver() = default;
  ~DirectCompositionOverlayCapsObserver() override = default;
};

// Upon receiving display notifications from ui::GpuSwitchingManager,
// DirectCompositionOverlayCapsMonitor updates its overlay caps with the new
// display setting and notifies DirectCompositionOverlayCapsObserver for the
// overlay cap change.
class GL_EXPORT DirectCompositionOverlayCapsMonitor
    : public ui::GpuSwitchingObserver {
 public:
  DirectCompositionOverlayCapsMonitor(
      const DirectCompositionOverlayCapsMonitor&) = delete;
  DirectCompositionOverlayCapsMonitor& operator=(
      const DirectCompositionOverlayCapsMonitor&) = delete;

  static DirectCompositionOverlayCapsMonitor* GetInstance();

  // DirectCompositionOverlayCapsMonitor is running on GpuMain thread.
  // AddObserver()/RemoveObserver() are thread safe.
  void AddObserver(DirectCompositionOverlayCapsObserver* observer);
  void RemoveObserver(DirectCompositionOverlayCapsObserver* observer);

  // Called when the overlay caps have changed.
  void NotifyOverlayCapsChanged();

  // Implements GpuSwitchingObserver.
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override;
  void OnDisplayAdded() override;
  void OnDisplayRemoved() override;
  void OnDisplayMetricsChanged() override;

 private:
  friend class base::NoDestructor<DirectCompositionOverlayCapsMonitor>;

  DirectCompositionOverlayCapsMonitor();
  ~DirectCompositionOverlayCapsMonitor() override;

  scoped_refptr<
      base::ObserverListThreadSafe<DirectCompositionOverlayCapsObserver>>
      observer_list_;
};
}  // namespace gl

#endif  // UI_GL_DIRECT_COMPOSITION_SUPPORT_H_
