// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_CONSTANTS_H_
#define UI_DISPLAY_TYPES_DISPLAY_CONSTANTS_H_

#include <stdint.h>
#include <array>

#include "ui/gfx/geometry/size_conversions.h"

namespace display {

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
  kNotSupported = 2,
  kPrivacyScreenStateLast = kNotSupported,
};

// Defines the float values closest to repeating decimal scale factors.
constexpr float kDsf_1_777 = 1.77777779102325439453125f;
constexpr float kDsf_2_252 = 2.2522523403167724609375f;
constexpr float kDsf_2_666 = 2.6666667461395263671875f;

constexpr char kDsfStr_1_777[] = "1.77777779102325439453125";
constexpr char kDsfStr_2_252[] = "2.2522523403167724609375";
constexpr char kDsfStr_2_666[] = "2.6666667461395263671875";

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
constexpr std::array<ZoomListBucketDsf, 7> kZoomListBucketsForDsf{{
    {1.25f, {0.7f, 1.f / 1.25f, 0.85f, 0.9f, 0.95f, 1.f, 1.1f, 1.2f, 1.3f}},
    {1.6f, {1.f / 1.6f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 1.f, 1.15f, 1.3f}},
    {kDsf_1_777,
     {1.f / kDsf_1_777, 0.65f, 0.75f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f}},
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
constexpr gfx::Size kWUXGA{1920, 1200};
// Dru
constexpr gfx::Size kQXGA_P{1536, 2048};
constexpr gfx::Size kQHD{2560, 1440};
// Chell
constexpr gfx::Size kQHD_PLUS{3200, 1800};
constexpr gfx::Size kUHD{3840, 2160};

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
    {10.1f,  kWXGA_800,  1.f,        kWXGA_800,   false,     kExact},
    {12.1f,  kWXGA_800,  1.f,       kWXGA_800,   true,      kExact},
    {11.6f,  kWXGA_768,  1.f,        kWXGA_768,   false,     kExact},
    {13.3f,  kWXGA_768,  1.f,        kWXGA_768,   true,      kExact},
    {14.f,   kWXGA_768,  1.f,        kWXGA_768,   true,      kExact},
    {15.6f,  kWXGA_768,  1.f,        kWXGA_768,   true,      kExact},
    {9.7f,   kQXGA_P,    2.0f,       {768, 1024}, false,     kExact},
    {11.6f,  kFHD,       1.6f,       {1200, 675}, false,     kExact},
    {13.0f,  kFHD,       1.25f,      {1536, 864}, true,      kExact},
    {13.3f,  kFHD,       1.25f,      {1536, 864}, true,      kExact},
    {14.f,   kFHD,       1.25f,      {1536, 864}, false,     kExact},
    {10.1f,  kWUXGA,     kDsf_1_777, {1080, 675}, false,     kExact},
    {12.2f,  kWUXGA,     1.6f,       {1200, 750}, false,     kExact},
    {15.6f,  kWUXGA,     1.f,        kWUXGA,      false,     kExact},
    {12.3f,  kQHD,       2.f,        {1280, 720}, false,     kExact},

    // Non standard panels
    {11.0f,  kLux,       2.f,        {1080, 720}, false,     kExact},
    {12.02f, kLux,       1.6f,       {1350, 900}, true,      kExact},
    {13.3f,  kQHD_PLUS,  2.f,        {1600, 900}, false,     kSkip},
    {13.3f,  kAkaliQHD,  1.6f,       {1410, 940}, false,     kExact},
    {12.3f,  kEveDisplay,2.0f,       {1200, 800}, false,     kExact},
    {12.85f, kLink,      2.0f,       {1280, 850}, false,     kExact},
    {12.3f,  kNocturne,  kDsf_2_252, {1332, 888}, false,     kEpsilon},
    {13.1f,  kUHD,       kDsf_2_666, {1440, 810}, false,     kExact},
    {15.6f,  kUHD,       2.4f,       {1600, 900}, false,     kEpsilon},

    // Chromebase
    {19.5,   kHD_PLUS,   1.f,        kHD_PLUS,    true,      kExact},
    {21.5f,  kFHD,       1.f,        kFHD,        true,      kExact},
    {23.8f,  kFHD,       1.f,        kFHD,        true,      kExact},

    // clang-format on
};

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_CONSTANTS_H_
