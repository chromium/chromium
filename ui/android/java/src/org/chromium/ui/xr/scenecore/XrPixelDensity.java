// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import org.chromium.build.annotations.NullMarked;

/**
 * A lightweight representation of XR pixel density (pixels per meter).
 *
 * <p>Used to convert between 2D screen/pixel dimensions and real-world 3D spatial meter units.
 */
@NullMarked
public interface XrPixelDensity {
    /** Returns the density-independent pixels per meter (DP per meter). */
    float getDpPerMeter();

    /** Returns the raw pixel density in pixels per meter. */
    float getPixelsPerMeter();

    /**
     * Converts a length in DP to meters.
     *
     * @param dp The length in DP.
     * @return The length in meters.
     */
    float convertDpToMeters(float dp);

    /**
     * Converts a length in meters to DP.
     *
     * @param meters The length in meters.
     * @return The length in DP.
     */
    float convertMetersToDp(float meters);

    /**
     * Converts a length in pixels to meters.
     *
     * @param pixels The length in pixels.
     * @return The length in meters.
     */
    float convertPixelsToMeters(float pixels);

    /**
     * Converts a length in meters to pixels.
     *
     * @param meters The length in meters.
     * @return The length in pixels.
     */
    float convertMetersToPixels(float meters);
}
