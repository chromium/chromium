// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_MANAGED_DISPLAY_INFO_H_
#define UI_DISPLAY_MANAGER_MANAGED_DISPLAY_INFO_H_

#include <stdint.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace display {

// A map between display mode size and zoom factor, used to preserve
// the zoom factor when the user changes the display mode.
using DisplaySizeToZoomFactorMap =
    std::map</*size_as_string=*/std::string, /*zoom_factor=*/float>;

// A class that represents the display's mode info.
class DISPLAY_MANAGER_EXPORT ManagedDisplayMode {
 public:
  ManagedDisplayMode();
  explicit ManagedDisplayMode(const gfx::Size& size);

  ManagedDisplayMode(const gfx::Size& size,
                     float refresh_rate,
                     bool is_interlaced,
                     bool native);

  ManagedDisplayMode(const gfx::Size& size,
                     float refresh_rate,
                     bool is_interlaced,
                     bool native,
                     float device_scale_factor);

  ~ManagedDisplayMode();
  ManagedDisplayMode(const ManagedDisplayMode& other);
  ManagedDisplayMode& operator=(const ManagedDisplayMode& other);
  bool operator==(const ManagedDisplayMode& other) const;

  // Returns the size in DIP which is visible to the user.
  gfx::Size GetSizeInDIP() const;

  // Returns true if |other| has same size and scale factors.
  bool IsEquivalent(const ManagedDisplayMode& other) const;

  const gfx::Size& size() const { return size_; }
  bool is_interlaced() const { return is_interlaced_; }
  float refresh_rate() const { return refresh_rate_; }

  bool native() const { return native_; }

  // Missing from ui::ManagedDisplayMode
  float device_scale_factor() const { return device_scale_factor_; }

  std::string ToString() const;

 private:
  gfx::Size size_;              // Physical pixel size of the display.
  float refresh_rate_ = 0.0f;   // Refresh rate of the display, in Hz.
  bool is_interlaced_ = false;  // True if mode is interlaced.
  bool native_ = false;         // True if mode is native mode of the display.
  float device_scale_factor_ = 1.0f;  // The device scale factor of the mode.
};

inline bool operator!=(const ManagedDisplayMode& lhs,
                       const ManagedDisplayMode& rhs) {
  return !(lhs == rhs);
}

// ManagedDisplayInfo contains metadata for each display. This is used to create
// |Display| as well as to maintain extra infomation to manage displays in ash
// environment. This class is intentionally made copiable.
class DISPLAY_MANAGER_EXPORT ManagedDisplayInfo {
 public:
  using ManagedDisplayModeList = std::vector<ManagedDisplayMode>;

  // Creates a ManagedDisplayInfo from string spec. 100+200-1440x800 creates
  // display
  // whose size is 1440x800 at the location (100, 200) in host coordinates.
  // The format is
  //
  // [origin-]widthxheight[*device_scale_factor][#resolutions list]
  //     [/<properties>][@zoom-factor][~rounded-display-radius]
  //
  // where [] are optional:
  // - |origin| is given in x+y- format.
  // - |device_scale_factor| is either 2 or 1 (or empty).
  // - |properties| can combination of:
  //     - 'o', which adds default overscan insets (5%)
  //     - 'h', which adds an HDR color space
  //     - one rotation property, either:
  //       - 'r' is 90 degree clock-wise (to the 'r'ight)
  //       - 'u' is 180 degrees ('u'pside-down)
  //       - 'l' is 270 degrees (to the 'l'eft).
  // - |zoom-factor| is floating value, e.g. @1.5 or @1.25.
  // - |resolution list| is the list of size that is given in
  //   |width x height [% refresh_rate]| separated by '|'.
  // - |panel_corners_radii| is a list of integer values separated by '|'
  //   that specifies the radius of each corner of display's panel with format:
  //     upper_left|upper_right|lower_right|lower_left
  //   If only one radius is specified, |radius|, it is the radius for all four
  //   corners.
  // A couple of examples:
  // "100x100"
  //      100x100 window at 0,0 origin. 1x device scale factor. no overscan.
  //      no rotation. 1.0 zoom factor. no rounded panel.
  // "100x100~16|16|10|10"
  //      100x100 window at 0,0 origin. 1x device scale factor. no overscan.
  //      no rotation. 1.0 zoom factor. display with rounded
  //      panel of radii (16,16,10,10).
  // "5+5-300x200~18"
  //      300x200 window at 5,5 origin. 2x device scale factor.
  //      no overscan, no rotation. 1.0 zoom factor. display with rounded
  //      panel of radii (18,18,18,18).
  // "5+5-300x200*2"
  //      300x200 window at 5,5 origin. 2x device scale factor.
  //      no overscan, no rotation. 1.0 zoom factor. no rounded display.
  // "300x200/ol"
  //      300x200 window at 0,0 origin. 1x device scale factor.
  //      with 5% overscan. rotated to left (90 degree counter clockwise).
  //      1.0 zoom factor. no rounded panel.
  // "10+20-300x200/u@1.5"
  //      300x200 window at 10,20 origin. 1x device scale factor.
  //      no overscan. flipped upside-down (180 degree) and 1.5 zoom factor.
  //      no rounded display.
  // "200x100#300x200|200x100%59.0|100x100%60"
  //      200x100 window at 0,0 origin, with 3 possible resolutions,
  //      300x200, 200x100 at 59 Hz, and 100x100 at 60 Hz.
  static ManagedDisplayInfo CreateFromSpec(const std::string& spec);

  // Creates a ManagedDisplayInfo from string spec using given |id|.
  static ManagedDisplayInfo CreateFromSpecWithID(const std::string& spec,
                                                 int64_t id);

  ManagedDisplayInfo();
  ManagedDisplayInfo(int64_t id, const std::string& name, bool has_overscan);
  ManagedDisplayInfo(const ManagedDisplayInfo& other);
  ~ManagedDisplayInfo();

  int64_t id() const { return id_; }
  void set_display_id(int64_t id) { id_ = id; }

  int64_t port_display_id() const { return port_display_id_; }
  void set_port_display_id(int64_t id) { port_display_id_ = id; }

  int64_t edid_display_id() const { return edid_display_id_; }
  void set_edid_display_id(int64_t id) { edid_display_id_ = id; }

  uint16_t connector_index() const { return connector_index_; }
  void set_connector_index(uint16_t index) { connector_index_ = index; }

  // The name of the display.
  const std::string& name() const { return name_; }

  // The path to the display device in the sysfs filesystem.
  void set_sys_path(const base::FilePath& sys_path) { sys_path_ = sys_path; }
  const base::FilePath& sys_path() const { return sys_path_; }

  // True if the display EDID has the overscan flag. This does not create the
  // actual overscan automatically, but used in the message.
  void set_has_overscan(bool has_overscan) { has_overscan_ = has_overscan; }
  bool has_overscan() const { return has_overscan_; }

  void set_touch_support(Display::TouchSupport support) {
    touch_support_ = support;
  }
  Display::TouchSupport touch_support() const { return touch_support_; }

  void set_connection_type(DisplayConnectionType type) {
    connection_type_ = type;
  }
  DisplayConnectionType connection_type() const { return connection_type_; }

  void set_physical_size(const gfx::Size& size) { physical_size_ = size; }
  const gfx::Size& physical_size() const { return physical_size_; }

  // Gets/Sets the device scale factor of the display.
  // TODO(oshima): Rename this to |default_device_scale_factor|.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  float zoom_factor() const { return zoom_factor_; }
  void set_zoom_factor(float zoom_factor) { zoom_factor_ = zoom_factor; }

  const DisplaySizeToZoomFactorMap& zoom_factor_map() const {
    return zoom_factor_map_;
  }

  void AddZoomFactorForSize(const std::string& size, float zoom_factor);

  float refresh_rate() const { return refresh_rate_; }
  void set_refresh_rate(float refresh_rate) { refresh_rate_ = refresh_rate; }
  bool is_interlaced() const { return is_interlaced_; }
  void set_is_interlaced(bool is_interlaced) { is_interlaced_ = is_interlaced; }

  // Gets/Sets the device DPI of the display.
  float device_dpi() const { return device_dpi_; }
  void set_device_dpi(float dpi) { device_dpi_ = dpi; }

  PanelOrientation panel_orientation() const { return panel_orientation_; }
  void set_panel_orientation(PanelOrientation panel_orientation) {
    panel_orientation_ = panel_orientation;
  }

  // The native bounds for the display. The size of this can be
  // different from the |size_in_pixel| when overscan insets are set.
  const gfx::Rect& bounds_in_native() const { return bounds_in_native_; }

  // The size for the display in pixels with the rotation taking into
  // account.
  const gfx::Size& size_in_pixel() const { return size_in_pixel_; }

  // The original size for the display in pixel, without rotation, but
  // |panel_orientation_| taking into account.
  gfx::Size GetSizeInPixelWithPanelOrientation() const;

  // The overscan insets for the display in DIP.
  const gfx::Insets& overscan_insets_in_dip() const {
    return overscan_insets_in_dip_;
  }

  // Sets the rotation for the given |source|. Setting a new rotation will also
  // have it become the active rotation.
  void SetRotation(Display::Rotation rotation, Display::RotationSource source);

  // Returns the currently active rotation for this display.
  Display::Rotation GetActiveRotation() const;

  // Returns the currently active rotation for this display with the panel
  // orientation adjustment applied.
  Display::Rotation GetLogicalActiveRotation() const;

  // Returns the source which set the active rotation for this display.
  Display::RotationSource active_rotation_source() const {
    return active_rotation_source_;
  }

  bool detected() const { return detected_; }
  void set_detected(bool detected) { detected_ = detected; }

  // Returns the rotation set by a given |source|.
  Display::Rotation GetRotation(Display::RotationSource source) const;

  // Returns the ui scale and device scale factor actually used to create
  // display that chrome sees. This is |device_scale_factor| x |zoom_factor_|.
  // TODO(oshima): Rename to |GetDeviceScaleFactor()|.
  float GetEffectiveDeviceScaleFactor() const;

  // Updates the zoom factor so that the effective dpi matches to the
  // recommended default dpi.
  void UpdateZoomFactorToMatchTargetDPI();

  // Copy the display info except for fields that can be modified by a user
  // (|rotation_|). |rotation_| is copied when the |another_info| isn't native
  // one.
  void Copy(const ManagedDisplayInfo& another_info);

  // Update the |bounds_in_native_| and |size_in_pixel_| using
  // given |bounds_in_native|.
  void SetBounds(const gfx::Rect& bounds_in_native);

  // Update the |bounds_in_native| according to the current overscan
  // and rotation settings.
  void UpdateDisplaySize();

  // Sets/Clears the overscan insets.
  void SetOverscanInsets(const gfx::Insets& insets_in_dip);
  gfx::Insets GetOverscanInsetsInPixel() const;

  // Snapshot ColorSpace is only valid for Ash Chrome.
  void SetSnapshotColorSpace(const gfx::ColorSpace& snapshot_color);
  gfx::ColorSpace GetSnapshotColorSpace() const;

  // Sets/Gets the flag to clear overscan insets.
  bool clear_overscan_insets() const { return clear_overscan_insets_; }
  void set_clear_overscan_insets(bool clear) { clear_overscan_insets_ = clear; }

  void set_native(bool native) { native_ = native; }
  bool native() const { return native_; }

  void set_from_native_platform(bool from_native_platform) {
    from_native_platform_ = from_native_platform;
  }
  bool from_native_platform() const { return from_native_platform_; }

  const ManagedDisplayModeList& display_modes() const { return display_modes_; }
  // Sets the display mode list. The mode list will be sorted for the
  // display.
  void SetManagedDisplayModes(const ManagedDisplayModeList& display_modes);

  // Returns the native mode size. If a native mode is not present, return an
  // empty size.
  gfx::Size GetNativeModeSize() const;

  const gfx::DisplayColorSpaces& display_color_spaces() const {
    return display_color_spaces_;
  }
  void set_display_color_spaces(
      const gfx::DisplayColorSpaces& display_color_spaces) {
    display_color_spaces_ = display_color_spaces;
  }

  uint32_t bits_per_channel() const { return bits_per_channel_; }
  void set_bits_per_channel(uint32_t bits_per_channel) {
    bits_per_channel_ = bits_per_channel;
  }

  bool is_aspect_preserving_scaling() const {
    return is_aspect_preserving_scaling_;
  }

  void set_is_aspect_preserving_scaling(bool value) {
    is_aspect_preserving_scaling_ = value;
  }

  // Maximum cursor size in native pixels.
  const gfx::Size& maximum_cursor_size() const { return maximum_cursor_size_; }
  void set_maximum_cursor_size(const gfx::Size& size) {
    maximum_cursor_size_ = size;
  }

  const std::string& manufacturer_id() const { return manufacturer_id_; }
  void set_manufacturer_id(const std::string& id) { manufacturer_id_ = id; }

  const std::string& product_id() const { return product_id_; }
  void set_product_id(const std::string& id) { product_id_ = id; }

  int32_t year_of_manufacture() const { return year_of_manufacture_; }
  void set_year_of_manufacture(int32_t year) { year_of_manufacture_ = year; }

  const gfx::RoundedCornersF& panel_corners_radii() const {
    return panel_corners_radii_;
  }
  void set_panel_corners_radii(const gfx::RoundedCornersF radii) {
    panel_corners_radii_ = radii;
  }

  VariableRefreshRateState variable_refresh_rate_state() const {
    return variable_refresh_rate_state_;
  }
  void set_variable_refresh_rate_state(
      VariableRefreshRateState variable_refresh_rate_state) {
    variable_refresh_rate_state_ = variable_refresh_rate_state;
  }

  const std::optional<float>& vsync_rate_min() const { return vsync_rate_min_; }
  void set_vsync_rate_min(const std::optional<float>& vsync_rate_min) {
    vsync_rate_min_ = vsync_rate_min;
  }

  // Returns a string representation of the ManagedDisplayInfo, excluding
  // display modes.
  std::string ToString() const;

  // Returns a string representation of the ManagedDisplayInfo, including
  // display modes.
  std::string ToFullString() const;

  const DrmFormatsAndModifiers& GetDRMFormatsAndModifiers() const {
    return drm_formats_and_modifiers_;
  }

  void SetDRMFormatsAndModifiers(
      const DrmFormatsAndModifiers& drm_formats_and_modifiers) {
    drm_formats_and_modifiers_ = drm_formats_and_modifiers;
  }

 private:
  // Return the rotation with the panel orientation applied.
  Display::Rotation GetRotationWithPanelOrientation(
      Display::Rotation rotation) const;

  int64_t id_;
  // Legacy port-based display ID
  int64_t port_display_id_ = kInvalidDisplayId;
  // EDID-based display ID
  int64_t edid_display_id_ = kInvalidDisplayId;

  // A connector's index is a combination of:
  // 1) the display's index in DRM          bits 0-7
  // 2) the display's DRM's index in KMS    bits 8-15
  // e.g. - A 3rd display in a 2nd DRM would produce a connector index == 0x0102
  //        (since display index == 2 and DRM index == 1)
  uint16_t connector_index_ = 0u;

  std::string name_;
  std::string manufacturer_id_;
  std::string product_id_;
  int32_t year_of_manufacture_;
  base::FilePath sys_path_;
  bool has_overscan_;
  std::map<Display::RotationSource, Display::Rotation> rotations_;
  Display::RotationSource active_rotation_source_;
  Display::TouchSupport touch_support_;
  DisplayConnectionType connection_type_ = DISPLAY_CONNECTION_TYPE_UNKNOWN;
  gfx::Size physical_size_;

  // This specifies the device's pixel density. (For example, a display whose
  // DPI is higher than the threshold is considered to have device_scale_factor
  // = 2.0 on Chrome OS).  This is used by the graphics layer to choose and draw
  // appropriate images and scale layers properly.
  float device_scale_factor_;
  gfx::Rect bounds_in_native_;

  // This specifies the device's DPI.
  float device_dpi_;

  // Orientation of the panel relative to natural device orientation.
  display::PanelOrientation panel_orientation_;

  // The size of the display in use. The size can be different from the size
  // of |bounds_in_native_| if the display has overscan insets and/or rotation.
  gfx::Size size_in_pixel_;
  // TODO(oshima): Change this to store pixel.
  gfx::Insets overscan_insets_in_dip_;

  // The zoom level currently applied to the display. This value is appended
  // multiplicatively to the device scale factor to get the effecting scaling
  // for a display.
  float zoom_factor_;

  // A map between display resolution and the zoom level applied.
  DisplaySizeToZoomFactorMap zoom_factor_map_;

  // The value of the current display mode refresh rate.
  float refresh_rate_;

  // True if the current display mode is interlaced (i.e. the display's odd
  // and even lines are scanned alternately in two interwoven rasterized lines).
  bool is_interlaced_;

  // True if this comes from native platform (DisplayChangeObserver).
  bool from_native_platform_;

  // True if current mode is native mode of the display.
  bool native_;

  // True if the display is configured to preserve the aspect ratio. When the
  // display is configured in a non-native mode, only parts of the display will
  // be used such that the aspect ratio is preserved.
  bool is_aspect_preserving_scaling_;

  // True if the displays' overscan inset should be cleared. This is
  // to distinguish the empty overscan insets from native display info.
  bool clear_overscan_insets_;

  // True if the display is detected by the system. The system will keep at
  // least one display available even if all displays are disconnected, and this
  // field is set to false in such a case.
  bool detected_ = true;

  // The list of modes supported by this display.
  ManagedDisplayModeList display_modes_;

  // Maximum cursor size.
  gfx::Size maximum_cursor_size_;

  // Colorimetry information of the Display.
  gfx::DisplayColorSpaces display_color_spaces_;

  // Color Space information as generated from the display EDID.
  gfx::ColorSpace snapshot_color_space_;

  // Bit depth of every channel, extracted from its EDID, usually 8, but can be
  // 0 if EDID says so or if the EDID (retrieval) was faulty.
  uint32_t bits_per_channel_;

  // Radii of the corners of the physical panel of the display. The value is
  // specified through command-line via switch `display-properties`. The default
  // radii is (0, 0, 0, 0).
  gfx::RoundedCornersF panel_corners_radii_;

  DrmFormatsAndModifiers drm_formats_and_modifiers_;

  VariableRefreshRateState variable_refresh_rate_state_;
  std::optional<float> vsync_rate_min_;

  // If you add a new member, you need to update Copy().
};

// Creates a managed display info. Note that if a valid |bounds| is not
// supplied, the returned ManagedDisplayInfo never called UpdateDisplaySize(),
// which means that transformations, such as rotation, are not properly applied.
ManagedDisplayInfo DISPLAY_MANAGER_EXPORT
CreateDisplayInfo(int64_t id, const gfx::Rect& bounds = gfx::Rect());

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_MANAGED_DISPLAY_INFO_H_
