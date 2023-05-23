// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/hdr_static_metadata.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace display {
class DisplaySnapshot;
struct GammaRampRGBEntry;
}  // namespace display

namespace ui {

class DrmDevice;
class HardwareDisplayControllerInfo;

class DrmDisplay {
 public:
  class PrivacyScreenProperty {
   public:
    explicit PrivacyScreenProperty(const scoped_refptr<DrmDevice>& drm,
                                   drmModeConnector* connector);
    PrivacyScreenProperty(const PrivacyScreenProperty&) = delete;
    PrivacyScreenProperty& operator=(const PrivacyScreenProperty&) = delete;

    ~PrivacyScreenProperty();

    bool SetPrivacyScreenProperty(bool enabled);

   private:
    display::PrivacyScreenState GetPrivacyScreenState() const;
    bool ValidateCurrentStateAgainst(bool enabled) const;
    drmModePropertyRes* GetReadPrivacyScreenProperty() const;
    drmModePropertyRes* GetWritePrivacyScreenProperty() const;

    const scoped_refptr<DrmDevice> drm_;
    raw_ptr<drmModeConnector, ExperimentalAsh> connector_ =
        nullptr;  // not owned.

    display::PrivacyScreenState property_last_ =
        display::kPrivacyScreenStateLast;
    ScopedDrmPropertyPtr privacy_screen_hw_state_;
    ScopedDrmPropertyPtr privacy_screen_sw_state_;
    ScopedDrmPropertyPtr privacy_screen_legacy_;
  };

  // Note that some of |info|'s references ownership will be handed to this
  // DrmDisplay instance.
  explicit DrmDisplay(const scoped_refptr<DrmDevice>& drm,
                      HardwareDisplayControllerInfo* info,
                      const display::DisplaySnapshot& display_snapshot);

  DrmDisplay(const DrmDisplay&) = delete;
  DrmDisplay& operator=(const DrmDisplay&) = delete;

  ~DrmDisplay();

  int64_t display_id() const { return display_id_; }
  int64_t base_connector_id() const { return base_connector_id_; }
  scoped_refptr<DrmDevice> drm() const { return drm_; }
  uint32_t crtc() const { return crtc_; }
  uint32_t connector() const;
  const std::vector<drmModeModeInfo>& modes() const { return modes_; }
  const gfx::Point& origin() { return origin_; }

  void SetOrigin(const gfx::Point origin) { origin_ = origin; }
  bool SetHdcpKeyProp(const std::string& key);
  bool GetHDCPState(display::HDCPState* state,
                    display::ContentProtectionMethod* protection_method);
  bool SetHDCPState(display::HDCPState state,
                    display::ContentProtectionMethod protection_method);
  void SetColorMatrix(const std::vector<float>& color_matrix);
  void SetBackgroundColor(const uint64_t background_color);
  void SetGammaCorrection(
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut);
  bool SetPrivacyScreen(bool enabled);
  bool SetHdrOutputMetadata(const gfx::ColorSpace color_space);
  bool SetHDR10Mode();
  void SetColorSpace(const gfx::ColorSpace& color_space);

  void set_is_hdr_capable_for_testing(bool value) { is_hdr_capable_ = value; }

 private:
  void CommitGammaCorrection(
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut);
  gfx::HDRStaticMetadata::Eotf GetEotf(
      const gfx::ColorSpace::TransferID transfer_id);

  const int64_t display_id_;
  const int64_t base_connector_id_;
  const scoped_refptr<DrmDevice> drm_;
  const uint32_t crtc_;
  const ScopedDrmConnectorPtr connector_;
  std::vector<drmModeModeInfo> modes_;
  gfx::Point origin_;
  bool is_hdr_capable_ = false;
  gfx::ColorSpace current_color_space_;
  absl::optional<gfx::HDRStaticMetadata> hdr_static_metadata_;
  std::unique_ptr<PrivacyScreenProperty> privacy_screen_property_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_
