// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_CONSTANTS_H_
#define UI_DISPLAY_TYPES_DISPLAY_CONSTANTS_H_

#include <stdint.h>

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

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_CONSTANTS_H_
