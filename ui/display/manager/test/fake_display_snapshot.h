// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_FAKE_DISPLAY_SNAPSHOT_H_
#define UI_DISPLAY_MANAGER_TEST_FAKE_DISPLAY_SNAPSHOT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace display {

// A display snapshot that doesn't correspond to a physical display, used when
// running off device.
class FakeDisplaySnapshot : public DisplaySnapshot {
 public:
  class Builder {
   public:
    Builder();

    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    ~Builder();

    // Builds new FakeDisplaySnapshot. At the very minimum you must set id and
    // native display mode before building or it will fail.
    std::unique_ptr<FakeDisplaySnapshot> Build();

    Builder& SetId(int64_t id);
    Builder& SetPortDisplayId(int64_t id);
    Builder& SetEdidDisplayId(int64_t id);
    Builder& SetConnectorIndex(uint16_t index);
    // Adds display mode with |size| and set as native mode. If a display mode
    // with |size| already exists then it will be reused.
    Builder& SetNativeMode(const gfx::Size& size);
    // Adds display mode and set as native mode. If an existing display mode is
    // equivalent to |mode| it will be set as native mode instead.
    Builder& SetNativeMode(std::unique_ptr<DisplayMode> mode);
    // Adds display mode with |size| and set as current mode. If a display mode
    // with |size| already exists then it will be reused.
    Builder& SetCurrentMode(const gfx::Size& size);
    // Adds display mode and set as current mode. If an existing display mode is
    // equivalent to |mode| it will be set as current mode instead.
    Builder& SetCurrentMode(std::unique_ptr<DisplayMode> mode);
    // Adds display mode with |size| if necessary. If a display mode with |size|
    // already exists then it will be reused.
    Builder& AddMode(const gfx::Size& size);
    // Adds |mode| if necessary. If an existing display mode is equivalent to
    // |mode| it will not be added.
    Builder& AddMode(std::unique_ptr<DisplayMode> mode);
    Builder& SetOrigin(const gfx::Point& origin);
    Builder& SetType(DisplayConnectionType type);
    Builder& SetBaseConnectorId(uint64_t base_connector_id);
    Builder& SetPathTopology(const std::vector<uint64_t>& path_topology);
    Builder& SetIsAspectPreservingScaling(bool is_aspect_preserving_scaling);
    Builder& SetHasOverscan(bool has_overscan);
    Builder& SetHasColorCorrectionMatrix(bool val);
    Builder& SetName(const std::string& name);
    Builder& SetSysPath(const base::FilePath& sys_path);
    Builder& SetProductCode(int64_t product_code);
    Builder& SetMaximumCursorSize(const gfx::Size& maximum_cursor_size);
    // Sets physical_size so that the screen has the specified DPI using the
    // native resolution.
    Builder& SetDPI(int dpi);
    // Sets physical_size for low DPI display.
    Builder& SetLowDPI();
    // Sets physical_size for high DPI display.
    Builder& SetHighDPI();
    Builder& SetPrivacyScreen(PrivacyScreenState state);
    Builder& SetHasContentProtectionKey(bool has_content_protection_key);
    Builder& SetColorSpace(const gfx::ColorSpace& color_space);
    Builder& SetBitsPerChannel(uint32_t bits_per_channel);
    Builder& SetHDRStaticMetadata(
        const gfx::HDRStaticMetadata& hdr_static_metadata);
    Builder& SetVariableRefreshRateState(
        VariableRefreshRateState variable_refresh_rate_state);

   private:
    // Returns a display mode with |size|. If there is no existing mode, insert
    // a display mode with |size| first.
    const DisplayMode* AddOrFindDisplayMode(const gfx::Size& size);
    // Returns a display mode equivalent to |mode|. If there is no equivalent
    // display mode, insert |mode| first.
    const DisplayMode* AddOrFindDisplayMode(std::unique_ptr<DisplayMode> mode);

    int64_t id_ = kInvalidDisplayId;
    int64_t port_display_id_ = kInvalidDisplayId;
    int64_t edid_display_id_ = kInvalidDisplayId;
    uint16_t connector_index_ = 0u;
    gfx::Point origin_;
    float dpi_ = 96.0;
    DisplayConnectionType type_ = DISPLAY_CONNECTION_TYPE_UNKNOWN;
    uint64_t base_connector_id_ = 1u;
    std::vector<uint64_t> path_topology_ = {};
    bool is_aspect_preserving_scaling_ = false;
    bool has_overscan_ = false;
    PrivacyScreenState privacy_screen_state_ = kNotSupported;
    bool has_content_protection_key_ = false;
    std::string name_;
    base::FilePath sys_path_;
    int64_t product_code_ = DisplaySnapshot::kInvalidProductCode;
    gfx::Size maximum_cursor_size_ = gfx::Size(64, 64);
    DisplayModeList modes_;
    raw_ptr<const DisplayMode> current_mode_ = nullptr;
    raw_ptr<const DisplayMode> native_mode_ = nullptr;
    DisplaySnapshot::ColorInfo color_info_;
    VariableRefreshRateState variable_refresh_rate_state_ =
        VariableRefreshRateState::kVrrNotCapable;
  };

  FakeDisplaySnapshot(int64_t display_id,
                      int64_t port_display_id,
                      int64_t edid_display_id,
                      uint16_t connector_index,
                      const gfx::Point& origin,
                      const gfx::Size& physical_size,
                      DisplayConnectionType type,
                      uint64_t base_connector_id,
                      const std::vector<uint64_t>& path_topology,
                      bool is_aspect_preserving_scaling,
                      bool has_overscan,
                      PrivacyScreenState privacy_screen_state,
                      bool has_content_protection_key_,
                      std::string display_name,
                      const base::FilePath& sys_path,
                      DisplayModeList modes,
                      const DisplayMode* current_mode,
                      const DisplayMode* native_mode,
                      int64_t product_code,
                      const gfx::Size& maximum_cursor_size,
                      const DisplaySnapshot::ColorInfo& color_info,
                      VariableRefreshRateState variable_refresh_rate_state,
                      const DrmFormatsAndModifiers& drm_formats_and_modifiers);

  FakeDisplaySnapshot(const FakeDisplaySnapshot&) = delete;
  FakeDisplaySnapshot& operator=(const FakeDisplaySnapshot&) = delete;

  ~FakeDisplaySnapshot() override;

  // Creates a display snapshot from the provided |spec| string. Returns null if
  // |spec| is invalid. See fake_display_delegate.h for |spec| format
  // description.
  static std::unique_ptr<DisplaySnapshot> CreateFromSpec(
      int64_t id,
      const std::string& spec);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_TEST_FAKE_DISPLAY_SNAPSHOT_H_
