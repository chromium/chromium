// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "ui/display/display_switches.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/icc_profile.h"

namespace display {
namespace {

// This variable tracks whether the forced device scale factor switch needs to
// be read from the command line, i.e. if it is set to -1 then the command line
// is checked.
int g_has_forced_device_scale_factor = -1;

// This variable caches the forced device scale factor value which is read off
// the command line. If the cache is invalidated by setting this variable to
// -1.0, we read the forced device scale factor again.
float g_forced_device_scale_factor = -1.0;

// An allowance error epsilon caused by fractional scale factor to produce
// expected DP display size.
constexpr float kDisplaySizeAllowanceEpsilon = 0.01f;

bool HasForceDeviceScaleFactorImpl() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceDeviceScaleFactor);
}

float GetForcedDeviceScaleFactorImpl() {
  double scale_in_double = 1.0;
  if (HasForceDeviceScaleFactorImpl()) {
    std::string value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceDeviceScaleFactor);
    if (!base::StringToDouble(value, &scale_in_double)) {
      LOG(ERROR) << "Failed to parse the default device scale factor:" << value;
      scale_in_double = 1.0;
    }
  }
  return static_cast<float>(scale_in_double);
}

const char* ToRotationString(display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return "0";
    case display::Display::ROTATE_90:
      return "90";
    case display::Display::ROTATE_180:
      return "180";
    case display::Display::ROTATE_270:
      return "270";
  }
  NOTREACHED_IN_MIGRATION();
  return "unknown";
}

}  // namespace

// static
float Display::GetForcedDeviceScaleFactor() {
  if (g_forced_device_scale_factor < 0)
    g_forced_device_scale_factor = GetForcedDeviceScaleFactorImpl();
  return g_forced_device_scale_factor;
}

// static
bool Display::HasForceDeviceScaleFactor() {
  if (g_has_forced_device_scale_factor == -1)
    g_has_forced_device_scale_factor = HasForceDeviceScaleFactorImpl();
  return !!g_has_forced_device_scale_factor;
}

// static
void Display::ResetForceDeviceScaleFactorForTesting() {
  g_has_forced_device_scale_factor = -1;
  g_forced_device_scale_factor = -1.0;
}

// static
void Display::SetForceDeviceScaleFactor(double dsf) {
  // Reset any previously set values and unset the flag.
  g_has_forced_device_scale_factor = -1;
  g_forced_device_scale_factor = -1.0;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceDeviceScaleFactor, base::StringPrintf("%.2f", dsf));
}

// static
gfx::ColorSpace Display::GetForcedRasterColorProfile() {
  DCHECK(HasForceRasterColorProfile());
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceRasterColorProfile);
  return ForcedColorProfileStringToColorSpace(value);
}

// static
bool Display::HasForceRasterColorProfile() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceRasterColorProfile);
}

// static
bool Display::HasEnsureForcedColorProfile() {
  static bool has_ensure_forced_color_profile =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnsureForcedColorProfile);
  return has_ensure_forced_color_profile;
}

// static
display::Display::Rotation Display::DegreesToRotation(int degrees) {
  if (degrees == 0)
    return display::Display::ROTATE_0;
  if (degrees == 90)
    return display::Display::ROTATE_90;
  if (degrees == 180)
    return display::Display::ROTATE_180;
  if (degrees == 270)
    return display::Display::ROTATE_270;
  NOTREACHED_IN_MIGRATION();
  return display::Display::ROTATE_0;
}

// static
int Display::RotationToDegrees(display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return 0;
    case display::Display::ROTATE_90:
      return 90;
    case display::Display::ROTATE_180:
      return 180;
    case display::Display::ROTATE_270:
      return 270;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

// static
bool Display::IsValidRotation(int degrees) {
  return degrees == 0 || degrees == 90 || degrees == 180 || degrees == 270;
}

Display::Display() : Display(kInvalidDisplayId) {}

Display::Display(int64_t id) : Display(id, gfx::Rect()) {}

Display::Display(int64_t id, const gfx::Rect& bounds)
    : id_(id),
      bounds_(bounds),
      work_area_(bounds),
      device_scale_factor_(GetForcedDeviceScaleFactor()) {
  SetDisplayColorSpacesRef(GetDefaultDisplayColorSpacesRef());
#if defined(USE_AURA)
  if (!bounds.IsEmpty())
    SetScaleAndBounds(device_scale_factor_, bounds);
#endif
}

Display::Display(const Display& other) = default;

Display::~Display() {}

// static
Display Display::GetDefaultDisplay() {
  return Display(kDefaultDisplayId, gfx::Rect(0, 0, 1920, 1080));
}

int Display::RotationAsDegree() const {
  switch (rotation_) {
    case ROTATE_0:
      return 0;
    case ROTATE_90:
      return 90;
    case ROTATE_180:
      return 180;
    case ROTATE_270:
      return 270;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

const gfx::DisplayColorSpaces& Display::GetColorSpaces() const {
  return color_spaces_->color_spaces();
}

void Display::SetColorSpaces(const gfx::DisplayColorSpaces& color_spaces) {
  SetDisplayColorSpacesRef(new DisplayColorSpacesRef(color_spaces));
}

void Display::SetRotationAsDegree(int rotation) {
  switch (rotation) {
    case 0:
      rotation_ = ROTATE_0;
      break;
    case 90:
      rotation_ = ROTATE_90;
      break;
    case 180:
      rotation_ = ROTATE_180;
      break;
    case 270:
      rotation_ = ROTATE_270;
      break;
    default:
      // We should not reach that but we will just ignore the call if we do.
      NOTREACHED_IN_MIGRATION();
  }
}

int Display::PanelRotationAsDegree() const {
  return RotationToDegrees(panel_rotation());
}

gfx::Rect Display::GetLocalWorkArea() const {
  gfx::Rect local_work_area(size());
  local_work_area.Inset(GetWorkAreaInsets());
  return local_work_area;
}

gfx::Insets Display::GetWorkAreaInsets() const {
  return gfx::Insets::TLBR(work_area_.y() - bounds_.y(),
                           work_area_.x() - bounds_.x(),
                           bounds_.bottom() - work_area_.bottom(),
                           bounds_.right() - work_area_.right());
}

void Display::SetScaleAndBounds(float device_scale_factor,
                                const gfx::Rect& bounds_in_pixel) {
  gfx::Insets insets = bounds_.InsetsFrom(work_area_);
  SetScale(device_scale_factor);

  gfx::RectF f(bounds_in_pixel);
  f.InvScale(device_scale_factor_);
  bounds_ = gfx::ToEnclosedRectIgnoringError(f, kDisplaySizeAllowanceEpsilon);
  size_in_pixels_ = bounds_in_pixel.size();
  native_origin_ = bounds_in_pixel.origin();
  UpdateWorkAreaFromInsets(insets);
}

void Display::SetScale(float device_scale_factor) {
  if (!HasForceDeviceScaleFactor()) {
#if BUILDFLAG(IS_APPLE)
    // Unless an explicit scale factor was provided for testing, ensure the
    // scale is integral.
    device_scale_factor = static_cast<int>(device_scale_factor);
#endif
    device_scale_factor_ = device_scale_factor;
  }
  device_scale_factor_ = std::max(0.5f, device_scale_factor_);
}

void Display::SetSize(const gfx::Size& size_in_pixel) {
  gfx::Point origin = bounds_.origin();
#if defined(USE_AURA)
  origin = gfx::ScaleToFlooredPoint(origin, device_scale_factor_);
#endif
  SetScaleAndBounds(device_scale_factor_, gfx::Rect(origin, size_in_pixel));
}

void Display::UpdateWorkAreaFromInsets(const gfx::Insets& insets) {
  work_area_ = bounds_;
  work_area_.Inset(insets);
}

gfx::Size Display::GetSizeInPixel() const {
  if (!size_in_pixels_.IsEmpty()) {
    return size_in_pixels_;
  }
  return gfx::ScaleToFlooredSize(size(), device_scale_factor_);
}

std::string Display::ToString() const {
  return base::StringPrintf(
      "Display[%lld] bounds=[%s], workarea=[%s], scale=%g, rotation=%s, "
      "panel_rotation=%s %s %s",
      static_cast<long long int>(id_), bounds_.ToString().c_str(),
      work_area_.ToString().c_str(), device_scale_factor_,
      ToRotationString(rotation_), ToRotationString(panel_rotation()),
      IsInternal() ? "internal" : "external",
      detected() ? "detected" : "not-detected");
}

bool Display::IsInternal() const {
  return is_valid() && display::IsInternalDisplayId(id_);
}

// static
int64_t Display::InternalDisplayId() {
  auto& ids = GetInternalDisplayIds();
  DCHECK_EQ(1u, ids.size());
  return ids.size() ? *ids.begin() : kInvalidDisplayId;
}

bool Display::operator==(const Display& rhs) const {
  return EqualExceptForHdrHeadroom(*this, rhs) &&
         (color_spaces_ == rhs.color_spaces_ ||
          GetColorSpaces() == rhs.GetColorSpaces());
}

// static
bool Display::EqualExceptForHdrHeadroom(const Display& lhs,
                                        const Display& rhs) {
  return lhs.id_ == rhs.id_ && lhs.bounds_ == rhs.bounds_ &&
         lhs.size_in_pixels_ == rhs.size_in_pixels_ &&
         lhs.native_origin_ == rhs.native_origin_ &&
         lhs.detected_ == rhs.detected_ && lhs.work_area_ == rhs.work_area_ &&
         lhs.device_scale_factor_ == rhs.device_scale_factor_ &&
         lhs.rotation_ == rhs.rotation_ &&
         lhs.touch_support_ == rhs.touch_support_ &&
         lhs.accelerometer_support_ == rhs.accelerometer_support_ &&
         lhs.maximum_cursor_size_ == rhs.maximum_cursor_size_ &&
         (lhs.color_spaces_ == rhs.color_spaces_ ||
          gfx::DisplayColorSpaces::EqualExceptForHdrHeadroom(
              lhs.GetColorSpaces(), rhs.GetColorSpaces())) &&
         lhs.color_depth_ == rhs.color_depth_ &&
         lhs.depth_per_component_ == rhs.depth_per_component_ &&
         lhs.is_monochrome_ == rhs.is_monochrome_ &&
         lhs.display_frequency_ == rhs.display_frequency_ &&
         lhs.label_ == rhs.label_;
}

void Display::SetDisplayColorSpacesRef(
    scoped_refptr<const DisplayColorSpacesRef> color_spaces) {
  color_spaces_ = std::move(color_spaces);
  if (color_spaces_->color_spaces().SupportsHDR()) {
    color_depth_ = kHDR10BitsPerPixel;
    depth_per_component_ = kHDR10BitsPerComponent;
  } else {
    color_depth_ = kDefaultBitsPerPixel;
    depth_per_component_ = kDefaultBitsPerComponent;
  }
}

scoped_refptr<const Display::DisplayColorSpacesRef>
Display::GetDefaultDisplayColorSpacesRef() {
  // On Android we need to ensure the platform supports a color profile before
  // using it. Using a not supported profile can result in fatal errors in the
  // GPU process.
  static const base::NoDestructor<scoped_refptr<const DisplayColorSpacesRef>>
      default_color_spaces_ref([] {
        auto color_space = gfx::ColorSpace::CreateSRGB();
#if !BUILDFLAG(IS_ANDROID)
        if (HasForceDisplayColorProfile()) {
          color_space = GetForcedDisplayColorProfile();
        }
#endif
        return new DisplayColorSpacesRef(gfx::DisplayColorSpaces(color_space));
      }());
  return *default_color_spaces_ref;
}

}  // namespace display
