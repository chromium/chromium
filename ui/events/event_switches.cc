// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/events/event_switches.h"

namespace switches {

// Enable compensation for unstable pinch zoom. Some touch screens display
// significant amount of wobble when moving a finger in a straight line. This
// makes two finger scroll trigger an oscillating pinch zoom. See
// crbug.com/394380 for details.
const char kCompensateForUnstablePinchZoom[] =
    "compensate-for-unstable-pinch-zoom";

// Overrides touch slop distance for gesture detection. The touch slop distance
// is the maximum distance from the starting point of a touch sequence that a
// gesture can travel before it can no longer be considered a tap. Scroll
// gestures can only begin after this distance has been travelled. The switch
// value is a floating point number that is interpreted as a distance in pixels.
const char kTouchSlopDistance[] = "touch-slop-distance";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Tells chrome to interpret events from these devices as touch events. Only
// available with XInput 2 (i.e. X server 1.8 or above). The id's of the
// devices can be retrieved from 'xinput list'.
const char kTouchDevices[] = "touch-devices";

// Tells chrome to interpret events from these devices as pen events. Only
// available with XInput 2 (i.e. X server 1.8 or above). The id's of the
// devices can be retrieved from 'xinput list'.
const char kPenDevices[] = "pen-devices";
#endif

#if BUILDFLAG(IS_OZONE)
// Tells Chrome to do edge touch filtering. Useful for convertible tablet.
const char kEdgeTouchFiltering[] = "edge-touch-filtering";

// Disable CancelAllTouches() function for the implementation on cancel single
// touches.
const char kDisableCancelAllTouches[] = "disable-cancel-all-touches";

// Enables logic to detect microphone mute switch device state, which disables
// internal audio input when toggled.
constexpr char kEnableMicrophoneMuteSwitchDeviceSwitch[] =
    "enable-microphone-mute-switch-device";

#endif

}  // namespace switches
