// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.interpolators;

import android.view.animation.AccelerateInterpolator;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.view.animation.LinearInterpolator;
import android.view.animation.OvershootInterpolator;

import androidx.core.view.animation.PathInterpolatorCompat;
import androidx.interpolator.view.animation.FastOutExtraSlowInInterpolator;
import androidx.interpolator.view.animation.FastOutLinearInInterpolator;
import androidx.interpolator.view.animation.FastOutSlowInInterpolator;
import androidx.interpolator.view.animation.LinearOutSlowInInterpolator;

/** Reference to one of each standard interpolator to avoid allocations. */
public class Interpolators {
    public static final Interpolator STANDARD_INTERPOLATOR =
            PathInterpolatorCompat.create(0.2f, 0f, 0f, 1f);
    public static final Interpolator STANDARD_ACCELERATE =
            PathInterpolatorCompat.create(0.3f, 0f, 1f, 1f);

    public static final AccelerateInterpolator ACCELERATE_INTERPOLATOR =
            new AccelerateInterpolator();
    public static final DecelerateInterpolator DECELERATE_INTERPOLATOR =
            new DecelerateInterpolator();

    public static final Interpolator EMPHASIZED = new FastOutExtraSlowInInterpolator();
    public static final Interpolator EMPHASIZED_ACCELERATE =
            PathInterpolatorCompat.create(0.3f, 0f, 0.8f, 0.15f);
    public static final Interpolator EMPHASIZED_DECELERATE =
            PathInterpolatorCompat.create(0.05f, 0.7f, 0.1f, 1f);

    public static final Interpolator LEGACY_ACCELERATE =
            PathInterpolatorCompat.create(0.4f, 0f, 1f, 1f);
    public static final Interpolator LEGACY_DECELERATE =
            PathInterpolatorCompat.create(0f, 0f, 0.2f, 1f);

    /** For fading out. Formerly FADE_OUT_CURVE. **/
    public static final FastOutLinearInInterpolator FAST_OUT_LINEAR_IN_INTERPOLATOR =
            new FastOutLinearInInterpolator();

    /** For general movement. Formerly TRANSFORM_CURVE. **/
    public static final FastOutSlowInInterpolator FAST_OUT_SLOW_IN_INTERPOLATOR =
            new FastOutSlowInInterpolator();

    /** For fading in. Formerly FADE_IN_CURVE. **/
    public static final LinearOutSlowInInterpolator LINEAR_OUT_SLOW_IN_INTERPOLATOR =
            new LinearOutSlowInInterpolator();

    public static final LinearInterpolator LINEAR_INTERPOLATOR = new LinearInterpolator();
    public static final OvershootInterpolator OVERSHOOT_INTERPOLATOR = new OvershootInterpolator();
}
