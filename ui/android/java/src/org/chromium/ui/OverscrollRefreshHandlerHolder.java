// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.chromium.build.NullUtil.assertNonNull;

import android.util.LongSparseArray;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/**
 * Package-private holder for active {@link OverscrollRefreshHandler} native references and JNI
 * dispatch methods.
 *
 * <p>Confined to the UI thread to ensure thread-safe access to {@link #sRefs} without
 * synchronization overhead. Avoids exposing internal JNI bookkeeping on the public {@link
 * OverscrollRefreshHandler} interface.
 */
@NullMarked
final class OverscrollRefreshHandlerHolder {
    // UI-thread confined to ensure thread-safe access to LongSparseArray without synchronization.
    // Use LongSparseArray to store refs to Java objects without boxing. This is necessary to avoid
    // a ScopedJavaGlobalRef in C++ of which there are only 51200 app wide.
    private static final LongSparseArray<OverscrollRefreshHandler> sRefs = new LongSparseArray<>();

    private OverscrollRefreshHandlerHolder() {}

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void setRef(long nativePtr, OverscrollRefreshHandler handler) {
        ThreadUtils.assertOnUiThread();
        assert sRefs.indexOfKey(nativePtr) < 0;
        sRefs.put(nativePtr, handler);
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void removeRef(long nativePtr) {
        ThreadUtils.assertOnUiThread();
        int index = sRefs.indexOfKey(nativePtr);
        assert index >= 0;
        sRefs.removeAt(index);
    }

    // TODO(bokan): Can we make the initiatingEdge param nullable in JNI?
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static boolean onPullStart(
            long nativePtr,
            @JniType("OverscrollAction") @OverscrollAction int type,
            @JniType("ui::BackGestureEventSwipeEdge") @BackGestureEventSwipeEdge int edge) {
        ThreadUtils.assertOnUiThread();
        return assertNonNull(sRefs.get(nativePtr)).start(type, edge);
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void onPullUpdate(long nativePtr, float xDelta, float yDelta) {
        ThreadUtils.assertOnUiThread();
        assertNonNull(sRefs.get(nativePtr)).pull(xDelta, yDelta);
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void onPullRelease(
            long nativePtr,
            @JniType("OverscrollActivationStatus") @OverscrollActivationStatus int status) {
        ThreadUtils.assertOnUiThread();
        assertNonNull(sRefs.get(nativePtr)).release(status);
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void onPullReset(long nativePtr) {
        ThreadUtils.assertOnUiThread();
        assertNonNull(sRefs.get(nativePtr)).reset();
    }
}
