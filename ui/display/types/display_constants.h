// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_CONSTANTS_H_
#define UI_DISPLAY_TYPES_DISPLAY_CONSTANTS_H_

#include <stdint.h>

#include <array>

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "ui/display/types/display_types_export.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace display {

// 1 inch in mm.
constexpr float kInchInMm = 25.4f;

// Display ID that represents an invalid display. Often used as a default value
// before display IDs are known.
constexpr int64_t kInvalidDisplayId = -1;

// Display ID that represents a valid display to be used when there's no actual
// display connected.
constexpr int64_t kDefaultDisplayId = 0xFF;

// Display ID for a virtual display assigned to a unified desktop.
constexpr int64_t kUnifiedDisplayId = -10;

// Invalid year of manufacture of the display.
constexpr int32_t kInvalidYearOfManufacture = -1;

// Used to determine if the two scale factor values are considered the same.
// TODO(crbug.com/40255259): Remove this when the scale factor precision issue
// in lacros is fixed.
constexpr float kDeviceScaleFactorErrorTolerance = 0.01f;

// The minimum HDR headroom for an HDR capable display. On macOS, when a
// display's brightness is set to maximum, it can report that there is no
// HDR headroom via maximumExtendedDynamicRangeColorComponentValue being 1.
// On Windows, when the SDR slider is at its maximum, it is possible for the
// reported SDR white level to be brighter than the maximum brightness of the
// display. These situations can create appearance that a display is rapidly
// fluctuating between being HDR capable and HDR incapable. To avoid this
// confusion, set this as the minimum maximum relative luminance for HDR
// capable displays.
constexpr float kMinHDRCapableMaxLuminanceRelative = 1.0625;
// Set SDR content to 75% of display brightness so SDR colors look good
// and there is no perceived brightness change during SDR-HDR.
constexpr float kSDRJoint = 0.75;

// Set the HDR level multiplier to 4x so that the bright areas of the videos
// are not overexposed, and maintain local contrast.
constexpr float kHDRLevel = 4.0;

// Used to describe the state of a multi-display configuration.
enum MultipleDisplayState {
  MULTIPLE_DISPLAY_STATE_INVALID,
  MULTIPLE_DISPLAY_STATE_HEADLESS,
  MULTIPLE_DISPLAY_STATE_SINGLE,
  MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,    // 2+ displays in mirror mode.
  MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,  // 2+ displays in extended mode.
};

// Video output types.
enum DisplayConnectionType {
  DISPLAY_CONNECTION_TYPE_NONE = 0,
  DISPLAY_CONNECTION_TYPE_UNKNOWN = 1 << 0,
  DISPLAY_CONNECTION_TYPE_INTERNAL = 1 << 1,
  DISPLAY_CONNECTION_TYPE_VGA = 1 << 2,
  DISPLAY_CONNECTION_TYPE_HDMI = 1 << 3,
  DISPLAY_CONNECTION_TYPE_DVI = 1 << 4,
  DISPLAY_CONNECTION_TYPE_DISPLAYPORT = 1 << 5,
  DISPLAY_CONNECTION_TYPE_NETWORK = 1 << 6,

  // Update this when adding a new type.
  DISPLAY_CONNECTION_TYPE_LAST = DISPLAY_CONNECTION_TYPE_NETWORK
};

// Content protection methods applied on video output.
enum ContentProtectionMethod {
  CONTENT_PROTECTION_METHOD_NONE = 0,
  CONTENT_PROTECTION_METHOD_HDCP = 1 << 0,
  // TYPE_0 for HDCP is the default, so make them equivalent.
  CONTENT_PROTECTION_METHOD_HDCP_TYPE_0 = CONTENT_PROTECTION_METHOD_HDCP,
  CONTENT_PROTECTION_METHOD_HDCP_TYPE_1 = 1 << 1,
};

// Bitmask of all the different HDCP types.
constexpr uint32_t kContentProtectionMethodHdcpAll =
    CONTENT_PROTECTION_METHOD_HDCP_TYPE_0 |
    CONTENT_PROTECTION_METHOD_HDCP_TYPE_1;

// HDCP protection state.
enum HDCPState {
  HDCP_STATE_UNDESIRED,
  HDCP_STATE_DESIRED,
  HDCP_STATE_ENABLED,

  // Update this when adding a new type.
  HDCP_STATE_LAST = HDCP_STATE_ENABLED
};

// The orientation of the panel in respect to the natural device orientation.
enum PanelOrientation {
  kNormal = 0,
  kBottomUp = 1,
  kLeftUp = 2,
  kRightUp = 3,
  kLast = kRightUp
};

// The existence, or lack thereof, and state of an ePrivacy screen.
enum PrivacyScreenState {
  kDisabled = 0,
  kEnabled = 1,
  // |kDisabledLocked| and |kEnabledLocked| are states in which the hardware is
  // force-disabled or forced-enabled by a physical external switch. When
  // privacy screen is set to one of these states, an attempt to set it to a
  // non-conforming state (e.g. enable it when it's Disabled-locked) should
  // fail.
  kDisabledLocked = 2,
  kEnabledLocked = 3,
  kNotSupported = 4,
  kPrivacyScreenLegacyStateLast = kDisabledLocked,
  kPrivacyScreenStateLast = kNotSupported,
};

// Whether a configuration should be seamless or full. Full configuration may
// result in visible artifacts such as blanking to achieve the specified
// configuration. Seamless configuration requests will fail if the system cannot
// achieve it without visible artifacts.
enum ConfigurationType {
  kConfigurationTypeFull,
  kConfigurationTypeSeamless,
};

// A flag to allow ui/display and ozone to adjust the behavior of display
// configurations.
enum class ModesetFlag {
  // At least one of kTestModeset and kCommitModeset must be set.
  kTestModeset,
  kCommitModeset,
  // When |kSeamlessModeset| is set, the commit (or test) will succeed only if
  // the submitted configuration can be completed without visual artifacts such
  // as blanking.
  kSeamlessModeset,

  kMinValue = kTestModeset,
  kMaxValue = kSeamlessModeset,
};

// A bitmask of flags as defined in display::ModesetFlag.
using ModesetFlags =
    base::EnumSet<ModesetFlag, ModesetFlag::kMinValue, ModesetFlag::kMaxValue>;

// Enum of possible states for variable refresh rates pertaining to a display.
enum class VariableRefreshRateState {
  kVrrDisabled = 0,
  kVrrEnabled = 1,
  kVrrNotCapable = 2,
  kVrrLast = kVrrNotCapable,
};

// Defines the float values closest to repeating decimal scale factors.
constexpr float kDsf_1_777 = 1.77777779102325439453125f;
constexpr float kDsf_2_252 = 2.2522523403167724609375f;
constexpr float kDsf_2_666 = 2.6666667461395263671875f;
constexpr float kDsf_1_8 = 1.80000007152557373046875f;
constexpr char kDsfStr_1_777[] = "1.77777779102325439453125";
constexpr char kDsfStr_2_252[] = "2.2522523403167724609375";
constexpr char kDsfStr_2_666[] = "2.6666667461395263671875";
constexpr char kDsfStr_1_8[] = "1.80000007152557373046875";

// The total number of display zoom factors to enumerate.
constexpr int kNumOfZoomFactors = 9;

// A pair representing the list of zoom values for a given minimum display
// resolution width.
using ZoomListBucket = std::pair<int, std::array<float, kNumOfZoomFactors>>;

// A pair representing the list of zoom values for a given minimum default dsf.
using ZoomListBucketDsf =
    std::pair<float, std::array<float, kNumOfZoomFactors>>;

// For displays with a device scale factor of unity, we use a static list of
// initialized zoom values. For a given resolution width of a display, we can
// find its associated list of zoom values by simply finding the last bucket
// with a width less than the given resolution width.
// Ex. A resolution width of 1024, we will use the bucket with the width of 960.
constexpr std::array<ZoomListBucket, 8> kZoomListBuckets{{
    {0, {0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f}},
    {720, {0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f, 1.05f, 1.10f}},
    {800, {0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f, 1.05f, 1.10f, 1.15f}},
    {960, {0.90f, 0.95f, 1.f, 1.05f, 1.10f, 1.15f, 1.20f, 1.25f, 1.30f}},
    {1280, {0.90f, 1.f, 1.05f, 1.10f, 1.15f, 1.20f, 1.25f, 1.30f, 1.50f}},
    {1920, {1.f, 1.10f, 1.15f, 1.20f, 1.30f, 1.40f, 1.50f, 1.75f, 2.00f}},
    {3840, {1.f, 1.10f, 1.20f, 1.40f, 1.60f, 1.80f, 2.00f, 2.20f, 2.40f}},
    {5120, {1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f, 3.00f}},
}};

// Displays with a default device scale factor have a static list of initialized
// zoom values that includes a zoom level to go to the native resolution of the
// display. Ensure that the list of DSFs are in sync with the list of default
// device scale factors in display_change_observer.cc.
constexpr std::array<ZoomListBucketDsf, 9> kZoomListBucketsForDsf{{
    {1.2f, {0.7f, 0.8f, 1.0f / 1.2f, 0.9f, 0.95f, 1.0f, 1.1f, 1.2f, 1.3f}},
    {1.25f, {0.7f, 1.f / 1.25f, 0.85f, 0.9f, 0.95f, 1.f, 1.1f, 1.2f, 1.3f}},
    {1.6f, {1.f / 1.6f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 1.f, 1.15f, 1.3f}},
    {kDsf_1_777,
     {1.f / kDsf_1_777, 0.65f, 0.75f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f}},
    {kDsf_1_8,
     {1.f / kDsf_1_8, 0.65f, 0.75f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f}},
    {2.f, {1.f / 2.f, 0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.25f, 1.5f}},
    {kDsf_2_252,
     {1.f / kDsf_2_252, 0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.15f, 1.3f, 1.5f}},
    {2.4f, {1.f / 2.4f, 0.5f, 0.6f, 0.8f, 0.9f, 1.f, 1.2f, 1.35f, 1.5f}},
    {kDsf_2_666,
     {1.f / kDsf_2_666, 0.5f, 0.6f, 0.8f, 0.9f, 1.f, 1.2f, 1.35f, 1.5f}},
}};

// Valid Displays
constexpr gfx::Size kWXGA_768{1366, 768};
constexpr gfx::Size kWXGA_800{1280, 800};
constexpr gfx::Size kHD_PLUS{1600, 900};
constexpr gfx::Size kFHD{1920, 1080};
constexpr gfx::Size kSHD{1280, 720};
constexpr gfx::Size kWUXGA{1920, 1200};
// Dru
constexpr gfx::Size kQXGA_P{1536, 2048};
constexpr gfx::Size kQHD{2560, 1440};
// Chell
constexpr gfx::Size kQHD_PLUS{3200, 1800};
constexpr gfx::Size k4K_UHD{3840, 2160};

// Chromebook special panels
constexpr gfx::Size kLux{2160, 1440};
constexpr gfx::Size kAkaliQHD{2256, 1504};
constexpr gfx::Size kLink{2560, 1700};
constexpr gfx::Size kEveDisplay{2400, 1600};
constexpr gfx::Size kNocturne{3000, 2000};

enum SizeErrorCheckType {
  kExact,    // Exact match.
  kEpsilon,  // Matches within epsilon.
  kSkip,     // Skip testing the error.
};

constexpr struct Data {
  const float diagonal_size;
  const gfx::Size resolution;
  const float expected_dsf;
  const gfx::Size expected_dp_size;
  const bool bad_range;
  const SizeErrorCheckType screenshot_size_error;
} display_configs[] = {
    // clang-format off
    // inch, resolution, DSF,        size in DP,  Bad range, size error
    {9.7f,   kQXGA_P,    2.0f,       {768, 1024}, false,     kExact},
    {10.f,   kWXGA_800,  1.25f,      {1024, 640}, false,     kExact},
    {10.1f,  kWXGA_800,  1.f,        kWXGA_800,   false,     kExact},
    {10.1f,  kFHD,       1.6,        {1200, 675}, true,      kExact},
    {10.1f,  kWUXGA,     kDsf_1_777, {1080, 675}, false,     kExact},
    {10.5f,  kWUXGA,     1.6f,       {1200, 750}, true,      kExact},
    {11.6f,  kWXGA_768,  1.f,        kWXGA_768,   false,     kExact},
    {11.6f,  kSHD,       1.f,        kSHD,        false,     kExact},
    {11.6f,  kFHD,       1.6f,       {1200, 675}, false,     kExact},
    {12.f,   kFHD,       1.6f,       {1200, 675}, false,     kExact},
    {12.1f,  kWXGA_800,  1.f,        kWXGA_800,   true,      kExact},
    {12.2f,  kWUXGA,     1.6f,       {1200, 750}, false,     kExact},
    {12.2f,  kFHD,       1.6f,       {1200, 675}, false,     kExact},
    {12.3f,  kQHD,       2.f,        {1280, 720}, false,     kExact},
    {13.0f,  kFHD,       1.25f,      {1536, 864}, true,      kExact},
    {13.1f,  k4K_UHD,    kDsf_2_666, {1440, 810}, false,     kExact},
    {13.3f,  kWXGA_768,  1.f,        kWXGA_768,   true,      kExact},
    {13.3f,  kFHD,       1.25f,      {1536, 864}, true,      kExact},
    {13.3f,  k4K_UHD,    kDsf_2_666, {1440, 810}, false,     kExact},
    {13.5f,  kFHD,       1.25f,      {1536, 864}, false,     kExact},
    {14.f,   kWXGA_768,  1.f,        kWXGA_768,   true,      kExact},
    {14.f,   kFHD,       1.25f,      {1536, 864}, false,     kExact},
    {14.f,   kWUXGA,     1.25f,      {1536, 960}, false,     kExact},
    {14.f,   k4K_UHD,    kDsf_2_666, {1440, 810}, false,     kExact},
    {15.6f,  kWXGA_768,  1.f,        kWXGA_768,   true,      kExact},
    {15.6f,  kWUXGA,     1.f,        kWUXGA,      false,     kExact},
    {15.6f,  kFHD,       1.f,        kFHD,        false,     kExact},
    {15.6f,  k4K_UHD,    2.4f,       {1600, 900}, false,     kEpsilon},
    {17.f,   kHD_PLUS,   1.f,        kHD_PLUS,    true,      kExact},
    {17.f,   kFHD,       1.0f,       {1920, 1080},false,     kExact},
    {17.3f,  kFHD,       1.0f,       {1920, 1080},false,     kExact},
    {18.51f, kWXGA_768,  1.0f,       kWXGA_768,   true,      kExact},

    // Non standard panels
    {11.0f,  kLux,       kDsf_1_8,   {1200, 800}, false,     kExact},
    {12.f,   {1366, 912},1.f,        {1366, 912}, false,     kExact},
    {12.3f,  kEveDisplay,2.0f,       {1200, 800}, false,     kExact},
    {12.85f, kLink,      2.0f,       {1280, 850}, false,     kExact},
    {12.3f,  kNocturne,  kDsf_2_252, {1332, 888}, false,     kEpsilon},
    {13.3f,  kQHD_PLUS,  2.f,        {1600, 900}, false,     kExact},
    {13.3f,  kAkaliQHD,  1.6f,       {1410, 940}, false,     kExact},
    {13.6f,  kAkaliQHD,  1.6f,       {1410, 940}, false,     kExact},

    // Chromebase
    {19.5,   kHD_PLUS,   1.f,        kHD_PLUS,    true,      kExact},
    {21.5f,  kFHD,       1.f,        kFHD,        true,      kExact},
    {23.8f,  kFHD,       1.f,        kFHD,        true,      kExact},

    // clang-format on
};

// A map of DRM formats and modifiers that are supported by the hardware planes
// of the display.
// See third_party/libdrm/src/include/drm/drm_fourcc.h for the canonical list of
// formats and modifiers
using DrmFormatsAndModifiers = base::flat_map<uint32_t, std::vector<uint64_t>>;

// Converts the display connection type from enum to string.
DISPLAY_TYPES_EXPORT std::string DisplayConnectionTypeString(
    DisplayConnectionType type);

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_CONSTANTS_H_
