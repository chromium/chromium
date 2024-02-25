// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;

/** Simple no-op default interface that allows subclasses to only implement methods as needed. */
public interface EmptyAnimationListener extends AnimationListener {
    @Override
    default void onAnimationStart(Animation animation) {}

    @Override
    default void onAnimationEnd(Animation animation) {}

    @Override
    default void onAnimationRepeat(Animation animation) {}
}
