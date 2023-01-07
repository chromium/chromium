// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

/**
 * Contains all of the command line switches that are specific to the display.
 */
public abstract class DisplaySwitches {
    // Native switch - display_switches::kForceDeviceScaleFactor
    public static final String FORCE_DEVICE_SCALE_FACTOR = "force-device-scale-factor";

    // Prevent instantiation.
    private DisplaySwitches() {}
}
