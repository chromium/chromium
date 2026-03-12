// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import org.chromium.build.annotations.NullMarked;

/** A callback used to pass meta state information from the time of click. */
@NullMarked
@FunctionalInterface
public interface ClickWithMetaStateCallback {
    /**
     * Called when a click occurs.
     *
     * @param metaState The meta key state at the time.
     * @param buttonState The mouse button state.
     */
    void onClickWithMeta(int metaState, int buttonState);
}
