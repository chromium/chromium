// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/display/types/display_color_management.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/crtc_commit_request.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/public/hardware_capabilities.h"

namespace gfx {
class Rect;
struct GpuFenceHandle;
}  // namespace gfx

namespace ui {

class CrtcController;
class HardwareDisplayPlane;

// This contains the list of planes controlled by one HDC on a given DRM fd.
// It is owned by the HDC and filled by the CrtcController.
struct HardwareDisplayPlaneList {
  HardwareDisplayPlaneList();
  ~HardwareDisplayPlaneList();

  // This is the list of planes to be committed this time.
  // This field is not vector<raw_ptr<...>> due to interaction with third_party
  // api.
  RAW_PTR_EXCLUSION std::vector<HardwareDisplayPlane*> plane_list;
  // This is the list of planes that was committed last time.
  // This field is not vector<raw_ptr<...>> due to interaction with third_party
  // api.
  RAW_PTR_EXCLUSION std::vector<HardwareDisplayPlane*> old_plane_list;

  struct PageFlipInfo {
    PageFlipInfo(uint32_t crtc_id, uint32_t framebuffer);
    PageFlipInfo(const PageFlipInfo& other);
    ~PageFlipInfo();

    uint32_t crtc_id;
    uint32_t framebuffer;
  };
  // In the case of non-atomic operation, this info will be used for
  // pageflipping.
  std::vector<PageFlipInfo> legacy_page_flips;

  // Adds trace records to |context|.
  void WriteIntoTrace(perfetto::TracedValue context) const;
};

class HardwareDisplayPlaneManager {
 public:
  struct CrtcProperties {
    CrtcProperties();
    CrtcProperties(const CrtcProperties& other);
    ~CrtcProperties();
    // Unique identifier for the CRTC. This must be greater than 0 to be valid.
    uint32_t id;
    // Keeps track of the CRTC state. If a surface has been bound, then the
    // value is set to true. Otherwise it is false.
    DrmWrapper::Property active;
    DrmWrapper::Property mode_id;
    // Optional properties.
    DrmWrapper::Property ctm;
    DrmWrapper::Property gamma_lut;
    DrmWrapper::Property gamma_lut_size;
    DrmWrapper::Property degamma_lut;
    DrmWrapper::Property degamma_lut_size;
    DrmWrapper::Property out_fence_ptr;
    DrmWrapper::Property background_color;
    DrmWrapper::Property vrr_enabled;
  };

  struct CrtcState {
    CrtcState();
    ~CrtcState();
    CrtcState(const CrtcState&) = delete;
    CrtcState& operator=(const CrtcState&) = delete;
    CrtcState(CrtcState&&);

    drmModeModeInfo mode = {};
    std::vector<scoped_refptr<DrmFramebuffer>> modeset_framebuffers;

    CrtcProperties properties = {};

    // The parameters most recently set from the browser. These are used to
    // compute the CTM, GAMMA, and DEGAMMA blobs.
    display::ColorTemperatureAdjustment color_temperature_adjustment;
    display::ColorCalibration color_calibration;
    display::GammaAdjustment gamma_adjustment;

    // The color space of all input planes. This assumes that all planes have
    // the same color space, because all existing devices only support one
    // color management configuration for all planes.
    SkColorSpacePrimaries planes_primaries = SkNamedPrimariesExt::kSRGB;

    // The color space of the output. All planes must be transformed to this
    // space, using the hardware color management parameters (DEGAMMA, CTM,
    // and GAMMA, where available).
    SkColorSpacePrimaries output_primaries = SkNamedPrimariesExt::kSRGB;

    // Cached blobs for the properties to commit in CommitCrtcProperties.
    // * If a property is `std::nullopt`, then it should be left unchanged.
    // * If a property is `nullptr` then it should be set to 0.
    // * If a property is a blob, then it should be set to that blob.
    std::optional<ScopedDrmPropertyBlob> pending_ctm_blob;
    std::optional<ScopedDrmPropertyBlob> pending_gamma_lut_blob;
    std::optional<ScopedDrmPropertyBlob> pending_degamma_lut_blob;
  };

  explicit HardwareDisplayPlaneManager(DrmDevice* drm);

  HardwareDisplayPlaneManager(const HardwareDisplayPlaneManager&) = delete;
  HardwareDisplayPlaneManager& operator=(const HardwareDisplayPlaneManager&) =
      delete;

  virtual ~HardwareDisplayPlaneManager();

  // This parses information from the drm driver, adding any new planes
  // or crtcs found.
  bool Initialize();

  // |commit_request| contains all the necessary information to build the
  // atomic/legacy request. It acts as a thin wrapper that looks like the atomic
  // request. It then gets converted into an atomic request for DRM atomic and
  // has all the parameters for a legacy request.
  // TODO(markyacoub): Consolidate this Commit() with the overloaded page flip
  // Commit() down below.
  virtual bool Commit(CommitRequest commit_request, uint32_t flags) = 0;

  // Probe the mode for the CRTC to |mode| based on the current configuration
  // of display hardware.
  virtual bool TestSeamlessMode(int32_t crtc_id,
                                const drmModeModeInfo& mode) = 0;

  // Clears old frame state out. Must be called before any AssignOverlayPlanes
  // calls.
  void BeginFrame(HardwareDisplayPlaneList* plane_list);

  // Sets the input color space for all planes. This assumes that all planes on
  // a CRTC have the same color space.
  void SetColorSpaceForAllPlanes(uint32_t crtc_id,
                                 const SkColorSpacePrimaries& primaries);

  // Sets the output color space for the given CRTC.
  void SetOutputColorSpace(uint32_t crtc_id,
                           const SkColorSpacePrimaries& primaries);

  // Sets the color temperature adjustment for a given CRTC.
  void SetColorTemperatureAdjustment(
      uint32_t crtc_id,
      const display::ColorTemperatureAdjustment& cta);

  // Sets the color calibration information for a given CRTC.
  void SetColorCalibration(uint32_t crtc_id,
                           const display::ColorCalibration& calibration);

  // Sets the gamma adjustment for a given CRTC.
  void SetGammaAdjustment(uint32_t crtc_id,
                          const display::GammaAdjustment& adjustment);

  // Sets the background color on the CRTC object with ID |crtc_id|.
  void SetBackgroundColor(uint32_t crtc_id, const uint64_t background_color);

  // Assign hardware planes from the |planes_| list to |overlay_list| entries,
  // recording the plane IDs in the |plane_list|. Only planes compatible with
  // |crtc_id| will be used. |overlay_list| must be sorted bottom-to-top.
  virtual bool AssignOverlayPlanes(
      HardwareDisplayPlaneList* plane_list,
      const DrmOverlayPlaneList& overlay_list,
      uint32_t crtc_id,
      std::optional<gfx::Point> crtc_offset = std::nullopt);

  // Commit the plane states in |plane_list|.
  // If |page_flip_request| is null, this tests the plane configuration without
  // submitting it.
  // The fence returned in |out_fence| will signal when the currently scanned
  // out buffers are replaced, and not when the buffers are scheduled with
  // |page_flip_request|. Note that the returned fence may be a nullptr
  // if the system doesn't support out fences.
  virtual bool Commit(HardwareDisplayPlaneList* plane_list,
                      scoped_refptr<PageFlipRequest> page_flip_request,
                      gfx::GpuFenceHandle* release_fence) = 0;

  // Disable all the overlay planes previously submitted and now stored in
  // plane_list->old_plane_list.
  virtual bool DisableOverlayPlanes(HardwareDisplayPlaneList* plane_list) = 0;

  // Check that the primary plane is valid for this
  // PlaneManager. Specifically, legacy can't support primary planes
  // that don't have the same size as the current mode of the crtc.
  virtual bool ValidatePrimarySize(const DrmOverlayPlane& primary,
                                   const drmModeModeInfo& mode) = 0;

  // Get the set of CRTC IDs from a plane's possible CRTCs bitmap
  base::flat_set<uint32_t> CrtcMaskToCrtcIds(uint32_t crtc_mask) const;

  const std::vector<std::unique_ptr<HardwareDisplayPlane>>& planes() const {
    return planes_;
  }

  // Request a callback to be called when the planes are ready to be displayed.
  // The callback will be invoked in the caller's execution context (same
  // sequence or thread).
  virtual void RequestPlanesReadyCallback(
      DrmOverlayPlaneList planes,
      base::OnceCallback<void(DrmOverlayPlaneList planes)> callback) = 0;

  // Returns all formats which can be scanned out by this PlaneManager.
  const std::vector<uint32_t>& GetSupportedFormats() const;

  std::vector<uint64_t> GetFormatModifiers(uint32_t crtc_id,
                                           uint32_t format) const;

  // Cache the most updated connectors found in DRM resources. This needs to be
  // called whenever a DRM hotplug event is received via UDEV.
  // Return a list of the valid Connector IDs that we got.
  base::flat_set<uint32_t> ResetConnectorsCacheAndGetValidIds(
      const ScopedDrmResourcesPtr& resources);

  // Get Immutable CRTC State.
  const CrtcState& GetCrtcStateForCrtcId(uint32_t crtc_id);

  // TODO(markyacoub): this seems hacky, this could be cleaned up a bit. Clarify
  // which resources needed to be tracked internally in
  // HardwareDisplayPlaneManager and which should be taken care of by the
  // caller.
  void ResetModesetStateForCrtc(uint32_t crtc_id);

  // Gets `HardwareCapabilities` based on planes available to the specified
  // CRTC. num_overlay_capable_planes counts both `DRM_PLANE_TYPE_PRIMARY` and
  // `DRM_PLANE_TYPE_OVERLAY` planes.
  HardwareCapabilities GetHardwareCapabilities(uint32_t crtc_id);

  // Get a bitmask of possible CRTCs for the connector with |connector_id|.
  // Returns 0 for invalid |connector_id|.
  uint32_t GetPossibleCrtcsBitmaskForConnector(uint32_t connector_id) const;

 protected:
  struct ConnectorProperties {
    uint32_t id;
    drmModeConnection connection;
    int count_modes;
    DrmWrapper::Property crtc_id;
    DrmWrapper::Property link_status;
    uint64_t possible_crtcs_bitmask;
  };

  bool InitializeCrtcState();

  void UpdateCrtcAndPlaneStatesAfterModeset(
      const CommitRequest& commit_request);

  // As the CRTC is being initialized, all connectors connected to it should
  // be disabled. This is a workaround for a bug on Hatch where Puff enables
  // a connector in dev mode before Chrome even starts. The kernel maps the HW
  // state at initial modeset (with a dangling connector attached to a CRTC).
  // When an Atomic Modeset is performed, it fails to modeset as the CRTC is
  // already attached to another dead connector. (Analysis: crbug/1067121#c5)
  // TODO(b/168154314): Remove this call when the bug is fixed.
  void DisableConnectedConnectorsToCrtcs(
      const ScopedDrmResourcesPtr& resources);

  virtual bool InitializePlanes() = 0;

  virtual bool SetPlaneData(HardwareDisplayPlaneList* plane_list,
                            HardwareDisplayPlane* hw_plane,
                            const DrmOverlayPlane& overlay,
                            uint32_t crtc_id,
                            std::optional<gfx::Point> crtc_offset,
                            const gfx::Rect& src_rect) = 0;

  virtual std::unique_ptr<HardwareDisplayPlane> CreatePlane(uint32_t plane_id);

  // Convert |crtc/connector_id| into an index, returning empty if the ID
  // couldn't be found.
  std::optional<int> LookupCrtcIndex(uint32_t crtc_id) const;
  std::optional<int> LookupConnectorIndex(uint32_t connector_id) const;

  // Get Mutable CRTC State.
  CrtcState& CrtcStateForCrtcId(uint32_t crtc_id);

  // Returns true if |plane| can support |overlay| and compatible with
  // |crtc_id|.
  virtual bool IsCompatible(HardwareDisplayPlane* plane,
                            const DrmOverlayPlane& overlay,
                            uint32_t crtc_id) const;

  // Resets |plane_list| setting all planes to unused.
  // Frees any temporary data structure in |plane_list| used for pageflipping.
  void ResetCurrentPlaneList(HardwareDisplayPlaneList* plane_list) const;
  // Restores |plane_list| planes |in_use| flag to what it was before
  // BeginFrame was called.
  // Frees any temporary data structure in |plane_list| used for pageflipping.
  void RestoreCurrentPlaneList(HardwareDisplayPlaneList* plane_list) const;

  // Populates scanout formats supported by all planes.
  void PopulateSupportedFormats();

  void UpdatePendingCrtcState(CrtcState& state);
  virtual bool CommitPendingCrtcState(CrtcState& state) = 0;

  // Object containing the connection to the graphics device and wraps the API
  // calls to control it. Not owned.
  const raw_ptr<DrmDevice> drm_;

  bool has_universal_planes_ = false;

  bool ctm_negative_values_broken_ = false;

  std::vector<std::unique_ptr<HardwareDisplayPlane>> planes_;
  std::vector<CrtcState> crtc_state_;
  std::vector<ConnectorProperties> connectors_props_;
  std::vector<uint32_t> supported_formats_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_H_
