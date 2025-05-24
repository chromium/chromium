// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Java counterpart to the native viz::OffsetTagConstraints. */
@DoNotMock("This is a simple value object.")
@NullMarked
public final class OffsetTagConstraints {
    public float mMinX;
    public float mMinY;
    public float mMaxX;
    public float mMaxY;

    public OffsetTagConstraints(float minX, float maxX, float minY, float maxY) {
        mMinX = minX;
        mMaxX = maxX;
        mMinY = minY;
        mMaxY = maxY;
    }

    @Override
    public String toString() {
        return String.valueOf(mMinX)
                + " "
                + String.valueOf(mMaxX)
                + " "
                + String.valueOf(mMinY)
                + " "
                + String.valueOf(mMaxY);
    }

    public boolean isValid() {
        return mMinX <= 0
                && mMinY <= 0
                && mMaxX >= 0
                && mMaxY >= 0
                && mMinX <= mMaxX
                && mMinY <= mMaxY;
    }

    public void reset() {
        mMinX = 0;
        mMinY = 0;
        mMaxX = 0;
        mMaxY = 0;
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof OffsetTagConstraints)) {
            return false;
        }

        final OffsetTagConstraints other = (OffsetTagConstraints) o;
        return mMinX == other.minX()
                && mMaxX == other.maxX()
                && mMinY == other.minY()
                && mMaxY == other.maxY();
    }

    @CalledByNative
    public float minX() {
        return mMinX;
    }

    @CalledByNative
    public float maxX() {
        return mMaxX;
    }

    @CalledByNative
    public float minY() {
        return mMinY;
    }

    @CalledByNative
    public float maxY() {
        return mMaxY;
    }
}
