// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Defines the possible shapes of an XR surface entity. */
@IntDef({XrSurfaceEntityShape.QUAD, XrSurfaceEntityShape.SPHERE, XrSurfaceEntityShape.HEMISPHERE})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface XrSurfaceEntityShape {
    /** A flat quad surface. */
    int QUAD = 0;

    /** A spherical surface. */
    int SPHERE = 1;

    /** A hemispherical surface. */
    int HEMISPHERE = 2;
}
