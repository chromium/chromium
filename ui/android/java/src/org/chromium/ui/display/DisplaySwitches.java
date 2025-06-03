// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import org.chromium.build.annotations.NullMarked;

/** Contains all of the command line switches that are specific to the display. */
@NullMarked
public abstract class DisplaySwitches {
    // Native switch - display_switches::kForceDeviceScaleFactor
    public static final String FORCE_DEVICE_SCALE_FACTOR = "force-device-scale-factor";
    public static final String AUTOMOTIVE_WEB_UI_SCALE_UP_ENABLED =
            "automotive-web-ui-scale-up-enabled";
    public static final String CLAMP_AUTOMOTIVE_SCALE_UP = "clamp-automotive-scale-up";
    public static final String XR_WEB_UI_SCALE_UP_ENABLED = "xr-web-ui-scale-up-enabled";

    // Prevent instantiation.
    private DisplaySwitches() {}
}
