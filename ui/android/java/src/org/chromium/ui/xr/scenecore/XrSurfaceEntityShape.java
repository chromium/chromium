// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Defines the possible shapes of an XR surface entity. */
@IntDef({
    XrSurfaceEntityShape.QUAD,
    XrSurfaceEntityShape.SPHERE,
    XrSurfaceEntityShape.HEMISPHERE,
    XrSurfaceEntityShape.CUSTOM,
    XrSurfaceEntityShape.SEAMLESS_SPHERE
})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface XrSurfaceEntityShape {
    /** A flat quad surface. */
    int QUAD = 0;

    /** A spherical surface. */
    int SPHERE = 1;

    /** A hemispherical surface. */
    int HEMISPHERE = 2;

    /** A custom mesh surface. */
    int CUSTOM = 3;

    /**
     * A custom sphere mesh surface with seamless UV mapping for 360° immersive video.
     *
     * <p>Includes inward texture coordinate padding to prevent bilinear filtering seam artifacts
     * along texture edges.
     */
    int SEAMLESS_SPHERE = 4;

    /** Helper utilities for {@link XrSurfaceEntityShape}. */
    final class Utils {
        private Utils() {}

        /** Returns true if the shape is curved. */
        public static boolean isCurved(@XrSurfaceEntityShape int shape) {
            return shape == SPHERE || shape == HEMISPHERE || shape == SEAMLESS_SPHERE;
        }
    }
}
