// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_SNAPSHOT_H_
#define UI_DISPLAY_TYPES_DISPLAY_SNAPSHOT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_static_metadata.h"

namespace display {

// This class represents the state of a display at one point in time. Platforms
// may extend this class in order to add platform specific configuration and
// identifiers required to configure this display.
class DISPLAY_TYPES_EXPORT DisplaySnapshot {
 public:
  using DisplayModeList = std::vector<std::unique_ptr<const DisplayMode>>;

  struct ColorInfo {
    // The color space of the display.
    // TODO(crbug.com/40945652): This should be derived from other
    // members.
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

    // Primaries and gamma indicated by the EDID.
    SkColorSpacePrimaries edid_primaries = SkNamedPrimariesExt::kSRGB;
    float edid_gamma = 2.2;

    // HDR static metadata, if available.
    std::optional<gfx::HDRStaticMetadata> hdr_static_metadata;

    // True if the display's color management is capable of applying color
    // temperature adjustment. If not, then color temperature adjustment
    // must be performed in software.
    bool supports_color_temperature_adjustment = false;

    // The number of bits per color channel.
    uint32_t bits_per_channel = 8u;
  };

  DisplaySnapshot(int64_t display_id,
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
                  bool has_content_protection_key,
                  const ColorInfo& color_info,
                  std::string display_name,
                  const base::FilePath& sys_path,
                  DisplayModeList modes,
                  PanelOrientation panel_orientation,
                  const std::vector<uint8_t>& edid,
                  const DisplayMode* current_mode,
                  const DisplayMode* native_mode,
                  int64_t product_code,
                  int32_t year_of_manufacture,
                  const gfx::Size& maximum_cursor_size,
                  VariableRefreshRateState variable_refresh_rate_state,
                  const DrmFormatsAndModifiers& drm_formats_and_modifiers_);

  DisplaySnapshot(const DisplaySnapshot&) = delete;
  DisplaySnapshot& operator=(const DisplaySnapshot&) = delete;

  virtual ~DisplaySnapshot();

  int64_t display_id() const { return display_id_; }

  // port_display_id() and edid_display_id() are required for
  // backward-compatibility and will eventually be removed once the migration to
  // EDID-based display IDs is completed. See http://b/193060019.
  int64_t port_display_id() const { return port_display_id_; }
  int64_t edid_display_id() const { return edid_display_id_; }

  uint16_t connector_index() const { return connector_index_; }
  const gfx::Point& origin() const { return origin_; }
  void set_origin(const gfx::Point& origin) { origin_ = origin; }
  const gfx::Size& physical_size() const { return physical_size_; }
  DisplayConnectionType type() const { return type_; }
  uint64_t base_connector_id() const { return base_connector_id_; }
  const std::vector<uint64_t>& path_topology() const { return path_topology_; }
  bool is_aspect_preserving_scaling() const {
    return is_aspect_preserving_scaling_;
  }
  bool has_overscan() const { return has_overscan_; }
  PrivacyScreenState privacy_screen_state() const {
    return privacy_screen_state_;
  }
  bool has_content_protection_key() const {
    return has_content_protection_key_;
  }
  bool has_color_correction_matrix() const {
    return color_info_.supports_color_temperature_adjustment;
  }
  const ColorInfo& color_info() const { return color_info_; }
  const gfx::ColorSpace& color_space() const { return color_info_.color_space; }
  uint32_t bits_per_channel() const { return color_info_.bits_per_channel; }
  const std::optional<gfx::HDRStaticMetadata>& hdr_static_metadata() const {
    return color_info_.hdr_static_metadata;
  }
  const std::string& display_name() const { return display_name_; }
  const base::FilePath& sys_path() const { return sys_path_; }
  const DisplayModeList& modes() const { return modes_; }
  PanelOrientation panel_orientation() const { return panel_orientation_; }
  const std::vector<uint8_t>& edid() const { return edid_; }
  const DisplayMode* current_mode() const { return current_mode_; }
  void set_current_mode(const DisplayMode* mode);
  const DisplayMode* native_mode() const { return native_mode_; }
  int64_t product_code() const { return product_code_; }
  int32_t year_of_manufacture() const { return year_of_manufacture_; }
  const gfx::Size& maximum_cursor_size() const { return maximum_cursor_size_; }
  VariableRefreshRateState variable_refresh_rate_state() const {
    return variable_refresh_rate_state_;
  }
  void set_variable_refresh_rate_state(
      VariableRefreshRateState variable_refresh_rate_state) {
    variable_refresh_rate_state_ = variable_refresh_rate_state;
  }
  const DrmFormatsAndModifiers& GetDRMFormatsAndModifiers() const {
    return drm_formats_and_modifiers_;
  }

  // Clones display state.
  std::unique_ptr<DisplaySnapshot> Clone() const;

  // Returns a textual representation of this display state.
  std::string ToString() const;

  // Used when no |product_code_| known.
  static const int64_t kInvalidProductCode = -1;

  // Returns the buffer format to be used for the primary plane buffer.
  static gfx::BufferFormat PrimaryFormat();

  // Adds |connector_index_| to bits 33-48 of |edid_display_id_|. This function
  // is not plumbed via mojom to limit and control usage across processes.
  void AddIndexToDisplayId();

  // Returns whether the display is capable of enabling variable refresh rates.
  bool IsVrrCapable() const;

  // Returns whether the display has variable refresh rates enabled.
  bool IsVrrEnabled() const;

 private:
  // Display id for this output.
  const int64_t display_id_;
  // Port-based display ID.
  const int64_t port_display_id_;
  // EDID-based display ID.
  int64_t edid_display_id_;

  // Used by AddIndexToDisplayId() to resolve display ID collisions when two
  // (or more) displays produce identical IDs due to incomplete EDIDs.
  const uint16_t connector_index_;

  // Display's origin on the framebuffer.
  gfx::Point origin_;

  const gfx::Size physical_size_;

  const DisplayConnectionType type_;

  // The next two private members represent the connection path between the
  // source device and this display. Consider the following three-display setup:
  // +-------------+
  // | Source      |    +-------------+
  // | (Device)    |    | BranchX     |
  // |             |    | (MST)       |
  // |       [conn6]--->|       [port1]--->DisplayA
  // +-------------+    |             |
  //                    |             |    +-------------+
  //                    |             |    | BranchY     |
  //                    |             |    | (MST)       |
  //                    |       [port2]--->|       [port1]----->DisplayB
  //                    +-------------+    |             |
  //                                       |       [port2]----->DisplayC
  //                                       +-------------+
  // [conn6]: is the root of the topology tree (a.k.a. the base connector),
  // which maps to a physical connector on the device. This value can be used to
  // determine if two or more external displays are sharing the same physical
  // port.
  // Important: Do not confuse this value with a display's connector ID!
  // The base connector will be listed as disconnected when a branch device is
  // attached to it to signal that it is not available for use, while new
  // connector IDs are spawned for connected monitors down the path. A display's
  // connector ID will be equal to the base connector ID only when the display
  // is connected directly to the source device.
  // [BranchX|port1]: is an output port to which DisplayA is connected.
  // [BranchX|port2]: is an output port to which BranchY is connected.
  // The ports on BranchY follow the same logic. Notice that port numbers across
  // branch devices are NOT unique.
  //
  // Example 1: if |this| represents DisplayB:
  // |base_connector_id_| == 6
  // |path_topology_| == {2, 1}
  // |base_connector_id_| != |this| connector id.
  //
  // Example 2: if |this| represents a display that is connected directly to the
  // source device above:
  // |base_connector_id_| == 6
  // |path_topology_| == {}
  // |base_connector_id_| == |this| connector id.
  //
  // The path is in a failed/error state if |base_connector_id_| == 0. This
  // indicates that the display is connected to one or more branch devices, but
  // the path could not be parsed.
  const uint64_t base_connector_id_;
  const std::vector<uint64_t> path_topology_;

  const bool is_aspect_preserving_scaling_;

  const bool has_overscan_;

  const PrivacyScreenState privacy_screen_state_;

  const bool has_content_protection_key_;

  const ColorInfo color_info_;

  const std::string display_name_;

  const base::FilePath sys_path_;

  // List of modes which natively exist on the display (i.e. have been extracted
  // from the display's EDID blob).
  DisplayModeList modes_;
  // List of modes which do not natively exist on the display. Modes are added
  // to this list as-needed due to either panel fitting from other displays or
  // from creating virtual modes. Once added, modes are not removed from this
  // list for the lifetime of the snapshot.
  DisplayModeList nonnative_modes_;

  // The orientation of the panel in respect to the natural device orientation.
  PanelOrientation panel_orientation_;

  // The display's EDID. It can be empty if nothing extracted such as in the
  // case of a virtual display.
  std::vector<uint8_t> edid_;

  // Mode currently being used by the output.
  raw_ptr<const DisplayMode> current_mode_;

  // "Best" mode supported by the output.
  const raw_ptr<const DisplayMode> native_mode_;

  // Combination of manufacturer id and product id.
  const int64_t product_code_;

  const int32_t year_of_manufacture_;

  // Maximum supported cursor size on this display.
  const gfx::Size maximum_cursor_size_;

  // Whether VRR is enabled, disabled, or not capable on this display.
  VariableRefreshRateState variable_refresh_rate_state_;

  // A list of supported Linux DRM formats and corresponding lists of modifiers
  // for each one.
  const DrmFormatsAndModifiers drm_formats_and_modifiers_;
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_SNAPSHOT_H_
