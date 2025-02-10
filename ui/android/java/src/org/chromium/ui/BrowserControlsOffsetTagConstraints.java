// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

/** Java counterpart to the native ui::android::BrowserControlsOffsetTagConstraints. */
@DoNotMock("This is a simple value object.")
public final class BrowserControlsOffsetTagConstraints {
    private final OffsetTagConstraints mTopControlsConstraints;
    private final OffsetTagConstraints mContentConstraints;
    private final OffsetTagConstraints mBottomControlsConstraints;

    public BrowserControlsOffsetTagConstraints(
            OffsetTagConstraints topControlsConstraints,
            OffsetTagConstraints contentConstraints,
            OffsetTagConstraints bottomControlsConstraints) {
        mTopControlsConstraints = topControlsConstraints;
        mContentConstraints = contentConstraints;
        mBottomControlsConstraints = bottomControlsConstraints;
    }

    @CalledByNative
    public OffsetTagConstraints getTopControlsConstraints() {
        return mTopControlsConstraints;
    }

    @CalledByNative
    public OffsetTagConstraints getContentConstraints() {
        return mContentConstraints;
    }

    @CalledByNative
    public OffsetTagConstraints getBottomControlsConstraints() {
        return mBottomControlsConstraints;
    }
}
