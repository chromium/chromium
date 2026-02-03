// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import static org.chromium.build.NullUtil.assertNonNull;

import android.graphics.Bitmap;
import android.hardware.HardwareBuffer;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A container class for the result of a screenshot operation, which can be either a {@link Bitmap}
 * or a {@link HardwareBuffer}. This class is used to pass screenshot data between Java and C++
 * components.
 */
@JNINamespace("ui")
@NullMarked
public class CaptureResult {
    /** Defines the desired format for the screenshot result. */
    @IntDef({Destination.BITMAP, Destination.HARDWARE_BUFFER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Destination {
        /** The screenshot result should be a {@link Bitmap}. */
        int BITMAP = 0;

        /** The screenshot result should be a {@link HardwareBuffer}. */
        int HARDWARE_BUFFER = 1;
    }

    /**
     * Constructs a {@link CaptureResult} with a {@link Bitmap}.
     *
     * @param bitmap The captured bitmap.
     */
    public CaptureResult(Bitmap bitmap) {
        mDestination = Destination.BITMAP;
        mBitmap = bitmap;
        mHardwareBuffer = null;
        mReleaseCallback = null;
    }

    /**
     * Constructs a {@link CaptureResult} with a {@link HardwareBuffer} and a callback to release
     * its resources.
     *
     * @param hardwareBuffer The captured hardware buffer.
     * @param releaseCallback A {@link Runnable} to be executed when the hardware buffer can be
     *     released.
     */
    public CaptureResult(HardwareBuffer hardwareBuffer, Runnable releaseCallback) {
        mDestination = Destination.HARDWARE_BUFFER;
        mHardwareBuffer = hardwareBuffer;
        mReleaseCallback = releaseCallback;
        mBitmap = null;
    }

    /**
     * Returns the captured {@link Bitmap}, if the screenshot was taken in {@link
     * Destination#BITMAP} format.
     *
     * @return The captured {@link Bitmap}, or {@code null} if not applicable.
     */
    @CalledByNative
    public Bitmap getBitmap() {
        assert mDestination == Destination.BITMAP;
        return assertNonNull(mBitmap);
    }

    /**
     * Returns the captured {@link HardwareBuffer}, if the screenshot was taken in {@link
     * Destination#HARDWARE_BUFFER} format.
     *
     * @return The captured {@link HardwareBuffer}, or {@code null} if not applicable.
     */
    @CalledByNative
    public HardwareBuffer getHardwareBuffer() {
        assert mDestination == Destination.HARDWARE_BUFFER;
        return assertNonNull(mHardwareBuffer);
    }

    /**
     * Returns a {@link Runnable} that should be executed to release the resources associated with
     * the {@link HardwareBuffer}. This is only applicable when the screenshot was taken in {@link
     * Destination#HARDWARE_BUFFER} format.
     *
     * @return The {@link Runnable} to release resources, or {@code null} if not applicable.
     */
    @CalledByNative
    public @JniType("base::OnceClosure") Runnable getReleaseCallback() {
        assert mDestination == Destination.HARDWARE_BUFFER;
        return assertNonNull(mReleaseCallback);
    }

    private final @Destination int mDestination;
    private final @Nullable Bitmap mBitmap;
    private final @Nullable HardwareBuffer mHardwareBuffer;
    private final @Nullable Runnable mReleaseCallback;
}
