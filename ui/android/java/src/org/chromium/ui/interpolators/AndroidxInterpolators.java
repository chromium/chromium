// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.interpolators;

import androidx.core.animation.Interpolator;
import androidx.core.animation.PathInterpolator;

import org.chromium.build.annotations.NullMarked;

/** Reference to one of each standard interpolator to avoid allocations. */
@NullMarked
public class AndroidxInterpolators {
    public static final Interpolator STANDARD_INTERPOLATOR = new PathInterpolator(0.2f, 0f, 0f, 1f);
}
