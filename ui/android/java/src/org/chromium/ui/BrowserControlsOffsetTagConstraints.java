// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/** Java counterpart to the native ui::android::BrowserControlsOffsetTagConstraints. */
@DoNotMock("This is a simple value object.")
@NullMarked
public final class BrowserControlsOffsetTagConstraints {
    private final @Nullable OffsetTagConstraints mTopControlsConstraints;
    private final @Nullable OffsetTagConstraints mContentConstraints;
    private final @Nullable OffsetTagConstraints mBottomControlsConstraints;

    public BrowserControlsOffsetTagConstraints(
            @Nullable OffsetTagConstraints topControlsConstraints,
            @Nullable OffsetTagConstraints contentConstraints,
            @Nullable OffsetTagConstraints bottomControlsConstraints) {
        mTopControlsConstraints = topControlsConstraints;
        mContentConstraints = contentConstraints;
        mBottomControlsConstraints = bottomControlsConstraints;
    }

    // TODO(peilinwang) remove this after fixing invalid constraints.
    public void assertAndFixConstraints(String callsite) {
        if (mTopControlsConstraints != null && !mTopControlsConstraints.isValid()) {
            assert false
                    : callsite + "Top constraints invalid: " + mTopControlsConstraints.toString();
            mTopControlsConstraints.reset();
        }

        if (mContentConstraints != null && !mContentConstraints.isValid()) {
            assert false
                    : callsite + "Content constraints invalid: " + mContentConstraints.toString();
            mContentConstraints.reset();
        }

        if (mBottomControlsConstraints != null && !mBottomControlsConstraints.isValid()) {
            assert false
                    : callsite
                            + "Bottom constraints invalid: "
                            + mBottomControlsConstraints.toString();
            mBottomControlsConstraints.reset();
        }
    }

    @CalledByNative
    public @Nullable OffsetTagConstraints getTopControlsConstraints() {
        return mTopControlsConstraints;
    }

    @CalledByNative
    public @Nullable OffsetTagConstraints getContentConstraints() {
        return mContentConstraints;
    }

    @CalledByNative
    public @Nullable OffsetTagConstraints getBottomControlsConstraints() {
        return mBottomControlsConstraints;
    }

    @Override
    public boolean equals(@Nullable Object other) {
        return other instanceof BrowserControlsOffsetTagConstraints that
                && Objects.equals(mTopControlsConstraints, that.getTopControlsConstraints())
                && Objects.equals(mContentConstraints, that.getContentConstraints())
                && Objects.equals(mBottomControlsConstraints, that.getBottomControlsConstraints());
    }
}
