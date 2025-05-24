// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface for a custom {@link View} to implement that provides a signal to run some runnables
 * when the next layout of the view happens. See {@link RunOnNextLayoutDelegate} for a helper to
 * implement this functionality.
 *
 * <p>This utility is useful when views are being added to the view hierarchy for the purpose of an
 * animation. Starting the animation before a view is first laid out can lead to visual jank in the
 * form of missing frames prior to layout finishing. Additionally, if geometry information from the
 * view is required for the animation, such as a translate in/out or scale animation, it is
 * necessary to wait for layout (or manually measure) for this information to be ready to configure
 * the animation.
 */
@NullMarked
public interface RunOnNextLayout {
    /**
     * Queue a runnable on the next layout if there is one otherwise the runnable will be invoked
     * the next time the UI thread goes idle.
     *
     * @param runnable The {@link Runnable} to run.
     */
    void runOnNextLayout(Runnable runnable);

    /** Run any queued runnables immediately. */
    void runOnNextLayoutRunnables();
}
