// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Defines the possible stereo modes for an XR surface entity. */
@IntDef({
    XrSurfaceEntityStereoMode.MONO,
    XrSurfaceEntityStereoMode.MULTIVIEW_LEFT_PRIMARY,
    XrSurfaceEntityStereoMode.MULTIVIEW_RIGHT_PRIMARY,
    XrSurfaceEntityStereoMode.SIDE_BY_SIDE,
    XrSurfaceEntityStereoMode.TOP_BOTTOM
})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface XrSurfaceEntityStereoMode {
    /** Monoscopic (2D) mode. */
    int MONO = 0;

    /** Multiview mode with the left eye as primary. */
    int MULTIVIEW_LEFT_PRIMARY = 1;

    /** Multiview mode with the right eye as primary. */
    int MULTIVIEW_RIGHT_PRIMARY = 2;

    /** Side-by-side stereoscopic mode. */
    int SIDE_BY_SIDE = 3;

    /** Top-bottom stereoscopic mode. */
    int TOP_BOTTOM = 4;
}
