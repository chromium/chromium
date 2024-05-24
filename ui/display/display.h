// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_H_
#define UI_DISPLAY_DISPLAY_H_

#include <stdint.h>

#include <optional>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/display/display_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect.h"

namespace display {

namespace mojom {
class DisplayDataView;
}

// This class typically, but does not always, correspond to a physical display
// connected to the system. A fake Display may exist on a headless system, or a
// Display may correspond to a remote, virtual display.
//
// Note: The screen and display currently uses pixel coordinate
// system. For platforms that support DIP (density independent pixel),
// |bounds()| and |work_area| will return values in DIP coordinate
// system, not in backing pixels.
class DISPLAY_EXPORT Display final {
 public:
  // Screen Rotation in clock-wise degrees.
  // This enum corresponds to DisplayRotationDefaultProto::Rotation in
  // components/policy/proto/chrome_device_policy.proto.
  enum Rotation {
    ROTATE_0 = 0,
    ROTATE_90,
    ROTATE_180,
    ROTATE_270,
  };

  // The display rotation can have multiple causes for change. A user can set a
  // preference. On devices with accelerometers, they can change the rotation.
  // RotationSource allows for the tracking of a Rotation per source of the
  // change. ACTIVE is the current rotation of the display. Rotation changes not
  // due to an accelerometer, nor the user, are to use this source directly.
  // UNKNOWN is when no rotation source has been provided.
  enum class RotationSource {
    ACCELEROMETER = 0,
    ACTIVE,
    USER,
    UNKNOWN,
  };

  // Touch support for the display.
  enum class TouchSupport {
    UNKNOWN,
    AVAILABLE,
    UNAVAILABLE,
  };

  // Accelerometer support for the display.
  enum class AccelerometerSupport {
    UNKNOWN,
    AVAILABLE,
    UNAVAILABLE,
  };

  // Creates a display with kInvalidDisplayId as default.
  Display();
  explicit Display(int64_t id);
  Display(int64_t id, const gfx::Rect& bounds);
  Display(const Display& other);
  ~Display();

  // Returns a valid display with default parameters and ID set to
  // |kDefaultDisplayId| which is used when there's no actual display connected
  // to the device.
  static Display GetDefaultDisplay();

  // Returns the forced device scale factor, which is given by
  // "--force-device-scale-factor".
  static float GetForcedDeviceScaleFactor();

  // Indicates if a device scale factor is being explicitly enforced from the
  // command line via "--force-device-scale-factor".
  static bool HasForceDeviceScaleFactor();

  // Returns the forced raster color profile, which is given by
  // "--force-raster-color-profile".
  static gfx::ColorSpace GetForcedRasterColorProfile();

  // Indicates if a raster color profile is being explicitly enforced from the
  // command line via "--force-raster-color-profile".
  static bool HasForceRasterColorProfile();

  // Indicates if the display color profile being forced should be ensured to
  // be in use by the operating system as well.
  static bool HasEnsureForcedColorProfile();

  // Resets the caches used to determine if a device scale factor is being
  // forced from the command line via "--force-device-scale-factor", and thus
  // ensures that the command line is reevaluated.
  static void ResetForceDeviceScaleFactorForTesting();

  // Resets the cache and sets a new force device scale factor.
  static void SetForceDeviceScaleFactor(double dsf);

  // Converts the given angle to its corresponding Rotation. The angle is in
  // degrees, and the only valid values are 0, 90, 180, and 270.
  // TODO(crbug.com/41387359): we should never need to convert degrees to a
  // Rotation if we were to Rotations internally and only converted to numeric
  // values when required.
  static Rotation DegreesToRotation(int degrees);

  // This is the analog to DegreesToRotation and converts a Rotation to a
  // numeric representation.
  static int RotationToDegrees(Rotation rotation);

  // Returns true if |degrees| is compatible with DegreesToRotation. I.e., that
  // it is 0, 90, 180, or 270.
  static bool IsValidRotation(int degrees);

  // Sets/Gets unique identifier associated with the display.
  // -1 means invalid display and it doesn't not exit.
  int64_t id() const { return id_; }
  void set_id(int64_t id) { id_ = id; }

  // Gets/Sets the display's bounds in Screen's coordinates.
  const gfx::Rect& bounds() const { return bounds_; }
  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }

  // Gets/Sets the display's work area in Screen's coordinates.
  const gfx::Rect& work_area() const { return work_area_; }
  void set_work_area(const gfx::Rect& work_area) { work_area_ = work_area; }

  // Output device's pixel scale factor. This specifies how much the
  // UI should be scaled when the actual output has more pixels than
  // standard displays (which is around 100~120dpi.) The potential return
  // values depend on each platforms.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  Rotation rotation() const { return rotation_; }
  void set_rotation(Rotation rotation) { rotation_ = rotation; }
  int RotationAsDegree() const;
  void SetRotationAsDegree(int rotation);

  // Panel's native rotation. This is same as |rotation()| in normal case.
  Rotation panel_rotation() const {
    return panel_rotation_ ? *panel_rotation_ : rotation_;
  }
  void set_panel_rotation(Rotation rotation) { panel_rotation_ = rotation; }
  int PanelRotationAsDegree() const;

  TouchSupport touch_support() const { return touch_support_; }
  void set_touch_support(TouchSupport support) { touch_support_ = support; }

  AccelerometerSupport accelerometer_support() const {
    return accelerometer_support_;
  }
  void set_accelerometer_support(AccelerometerSupport support) {
    accelerometer_support_ = support;
  }

  // Utility functions that just return the size of display and work area.
  const gfx::Size& size() const { return bounds_.size(); }
  const gfx::Size& work_area_size() const { return work_area_.size(); }

  // Returns the work area relative to this display's origin.
  gfx::Rect GetLocalWorkArea() const;

  // Returns the work area insets.
  gfx::Insets GetWorkAreaInsets() const;

  // Sets the device scale factor and display bounds in pixel. This
  // updates the work area using the same insets between old bounds and
  // work area.  This does not set the native origin based on `bounds_in_pixel`.
  void SetScaleAndBounds(float device_scale_factor,
                         const gfx::Rect& bounds_in_pixel);

  // Sets the device scale factor while respecting forced scale factor and other
  // constraints. Use this over set_device_scale_factor() unless you need to
  // forcefully overwrite the scale.
  void SetScale(float device_scale_factor);

  // Sets the display's size. This updates the work area using the same insets
  // between old bounds and work area.
  void SetSize(const gfx::Size& size_in_pixel);

  // Computes and updates the display's work are using
  // |work_area_insets| and the bounds.
  void UpdateWorkAreaFromInsets(const gfx::Insets& work_area_insets);

  // Returns the display's size in pixel coordinates.
  gfx::Size GetSizeInPixel() const;
  void set_size_in_pixels(const gfx::Size& size) { size_in_pixels_ = size; }

  // Returns the display's origin in pixel coordinates.  Only available on
  // windowing systems like X11 that position displays in pixel coordinates.
  gfx::Point native_origin() const { return native_origin_; }
  void set_native_origin(const gfx::Point& native_origin) {
    native_origin_ = native_origin;
  }

  // Returns a string representation of the display;
  std::string ToString() const;

  // True if the display contains valid display id.
  bool is_valid() const { return id_ != kInvalidDisplayId; }

  // True if the display corresponds to internal panel.
  bool IsInternal() const;

  // Returns true if the display is detected by the system. A display can
  // stay 'active' when all displays are disconnected from SW point of view,
  // because this can happen when the display went to sleep mode, or the
  // device went to sleep mode, and in that case, we do not want to change
  // the display configuration (so that it starts in the same state when
  // resumed). Use this if you want to check if the display is detected by the
  // system.
  bool detected() const { return detected_; }
  void set_detected(bool detected) { detected_ = detected; }

  // [Deprecated] Use `display::GetInternalDisplayIds()`.
  // Gets an id of display corresponding to internal panel.
  static int64_t InternalDisplayId();

  // Maximum cursor size in native pixels.
  const gfx::Size& maximum_cursor_size() const { return maximum_cursor_size_; }
  void set_maximum_cursor_size(const gfx::Size& size) {
    maximum_cursor_size_ = size;
  }

  // The color spaces used by the display.
  const gfx::DisplayColorSpaces& GetColorSpaces() const;
  void SetColorSpaces(const gfx::DisplayColorSpaces& color_spaces);

  // Return true if the display orientation is landscape.
  bool is_landscape() const { return bounds_.width() >= bounds_.height(); }

  // Default values for color_depth and depth_per_component.
  static constexpr int kDefaultBitsPerPixel = 24;
  static constexpr int kDefaultBitsPerComponent = 8;

  // The following values are abused by media query APIs to detect HDR
  // capability.
  static constexpr int kHDR10BitsPerPixel = 30;
  static constexpr int kHDR10BitsPerComponent = 10;

  // The number of bits per pixel. Used by media query APIs.
  int color_depth() const { return color_depth_; }
  void set_color_depth(int color_depth) {
    color_depth_ = color_depth;
  }

  // The number of bits per color component (all color components are assumed to
  // have the same number of bits). Used by media query APIs.
  int depth_per_component() const { return depth_per_component_; }
  void set_depth_per_component(int depth_per_component) {
    depth_per_component_ = depth_per_component;
  }

  // True if this is a monochrome display (e.g, for accessibility). Used by
  // media query APIs.
  bool is_monochrome() const { return is_monochrome_; }
  void set_is_monochrome(bool is_monochrome) { is_monochrome_ = is_monochrome; }

  // The display frequency of the monitor.
  float display_frequency() const { return display_frequency_; }
  void set_display_frequency(float display_frequency) {
    display_frequency_ = display_frequency;
  }

  uint32_t audio_formats() const { return audio_formats_; }
  void set_audio_formats(uint32_t audio_formats) {
    audio_formats_ = audio_formats;
  }

  // A user-friendly label, determined by the platform.
  const std::string& label() const { return label_; }
  void set_label(const std::string& label) { label_ = label; }

  bool operator==(const Display& rhs) const;
  bool operator!=(const Display& rhs) const { return !(*this == rhs); }
  static bool EqualExceptForHdrHeadroom(const Display& lhs, const Display& rhs);

 private:
  friend struct mojo::StructTraits<mojom::DisplayDataView, Display>;

  // A ref counted object to avoid copying DisplayColorSpaces.
  class DisplayColorSpacesRef
      : public base::RefCountedThreadSafe<DisplayColorSpacesRef> {
   public:
    DisplayColorSpacesRef() = default;
    explicit DisplayColorSpacesRef(const gfx::DisplayColorSpaces& color_spaces)
        : color_spaces_(color_spaces) {}
    DisplayColorSpacesRef(const DisplayColorSpacesRef& color_spaces) = delete;
    const DisplayColorSpacesRef& operator=(const DisplayColorSpacesRef) =
        delete;

    const gfx::DisplayColorSpaces& color_spaces() const {
      return color_spaces_;
    }

   private:
    friend class base::RefCountedThreadSafe<DisplayColorSpacesRef>;

    ~DisplayColorSpacesRef() = default;
    const gfx::DisplayColorSpaces color_spaces_;
  };

  void SetDisplayColorSpacesRef(
      scoped_refptr<const DisplayColorSpacesRef> color_spaces);

  // Returns the default value of the DisplayColorSpaces.
  static scoped_refptr<const DisplayColorSpacesRef>
  GetDefaultDisplayColorSpacesRef();

  int64_t id_ = kInvalidDisplayId;
  gfx::Rect bounds_;
  // If non-empty, then should be same as |bounds_|. Used to avoid rounding
  // errors.
  gfx::Size size_in_pixels_;
  gfx::Point native_origin_;
  gfx::Rect work_area_;
  float device_scale_factor_;
  Rotation rotation_ = ROTATE_0;
  std::optional<Rotation> panel_rotation_;
  TouchSupport touch_support_ = TouchSupport::UNKNOWN;
  AccelerometerSupport accelerometer_support_ = AccelerometerSupport::UNKNOWN;
  gfx::Size maximum_cursor_size_;
  scoped_refptr<const DisplayColorSpacesRef> color_spaces_;
  int color_depth_;
  int depth_per_component_;
  bool is_monochrome_ = false;
  bool detected_ = true;
  float display_frequency_ = 0;
  std::string label_;
  uint32_t audio_formats_ = 0;
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_H_
