// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/hdr_static_metadata.h"
#include "ui/ozone/platform/drm/common/hardware_display_controller_info.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/common/tile_property.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace display {
class DisplaySnapshot;
struct ColorTemperatureAdjustment;
struct ColorCalibration;
struct GammaAdjustment;
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
    raw_ptr<drmModeConnector> connector_ = nullptr;  // not owned.

    display::PrivacyScreenState property_last_ =
        display::kPrivacyScreenStateLast;
    ScopedDrmPropertyPtr privacy_screen_hw_state_;
    ScopedDrmPropertyPtr privacy_screen_sw_state_;
    ScopedDrmPropertyPtr privacy_screen_legacy_;
  };

  struct CrtcConnectorPair {
    CrtcConnectorPair(uint32_t crtc_id,
                      ScopedDrmConnectorPtr connector,
                      std::optional<gfx::Point> tile_location);
    CrtcConnectorPair(const CrtcConnectorPair& other) = delete;
    CrtcConnectorPair& operator=(const CrtcConnectorPair& other) = delete;
    CrtcConnectorPair(CrtcConnectorPair&& other) noexcept;
    CrtcConnectorPair& operator=(CrtcConnectorPair&& other) noexcept;
    ~CrtcConnectorPair();

    uint32_t crtc_id;
    ScopedDrmConnectorPtr connector;
    std::optional<gfx::Point> tile_location;
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
  uint32_t GetPrimaryCrtcId() const {
    return primary_crtc_connector_pair_->crtc_id;
  }
  uint32_t GetPrimaryConnectorId() const;
  const std::vector<CrtcConnectorPair>& crtc_connector_pairs() const;
  const std::vector<drmModeModeInfo>& modes() const { return modes_; }
  const gfx::Point& origin() { return origin_; }
  const std::optional<uint16_t>& vsync_rate_min_from_edid() const {
    return vsync_rate_min_from_edid_;
  }

  bool ContainsCrtc(uint32_t crtc_id) const;

  void SetOrigin(const gfx::Point origin) { origin_ = origin; }
  bool SetHdcpKeyProp(const std::string& key);
  bool GetHDCPState(display::HDCPState* state,
                    display::ContentProtectionMethod* protection_method);
  bool SetHDCPState(display::HDCPState state,
                    display::ContentProtectionMethod protection_method);
  void SetColorTemperatureAdjustment(
      const display::ColorTemperatureAdjustment& cta);
  void SetColorCalibration(const display::ColorCalibration& calibration);
  void SetGammaAdjustment(const display::GammaAdjustment& adjustment);
  void SetBackgroundColor(const uint64_t background_color);
  bool SetPrivacyScreen(bool enabled);
  bool SetHdrOutputMetadata(const gfx::ColorSpace color_space);
  bool SetColorspaceProperty(const gfx::ColorSpace color_space);
  bool IsVrrCapable() const;

  // Replace CRTCs in |crtc_connector_pairs_| according to mapping provided by
  // |current_to_new_crtc_ids|. Must replace all CRTCs in
  // |crtc_connector_pairs_| in a single call. New CRTC IDs must be unique. All
  // new CRTCs should be checked to be compatible with the connectors before the
  // call.
  bool ReplaceCrtcs(
      const base::flat_map<uint32_t /*current_crtc*/, uint32_t /*new_crtc*/>&
          current_to_new_crtc_ids);

  std::optional<TileProperty> GetTileProperty() const;
  const CrtcConnectorPair* GetCrtcConnectorPairForConnectorId(
      uint32_t connector_id) const;

 private:
  gfx::HDRStaticMetadata::Eotf GetEotf(
      const gfx::ColorSpace::TransferID transfer_id);
  bool ClearHdrOutputMetadata();

  const int64_t display_id_;
  const int64_t base_connector_id_;
  const scoped_refptr<DrmDevice> drm_;
  std::vector<CrtcConnectorPair> crtc_connector_pairs_;
  // Main CRTC and connector used to identify the display and apply certain
  // display-specific properties. If there is only one CrtcConnectorPair pair,
  // then that pair would be the primary. Note that
  // |primary_crtc_connector_pair_| is a reference to |crtc_connector_pairs_|,
  // and thus must be initialized AFTER |crtc_connector_pairs_|.
  const raw_ref<const CrtcConnectorPair> primary_crtc_connector_pair_;
  std::vector<drmModeModeInfo> modes_;
  gfx::Point origin_;
  std::optional<gfx::HDRStaticMetadata> hdr_static_metadata_;
  std::unique_ptr<PrivacyScreenProperty> privacy_screen_property_;
  std::optional<uint16_t> vsync_rate_min_from_edid_;
  std::optional<TileProperty> tile_property_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_
