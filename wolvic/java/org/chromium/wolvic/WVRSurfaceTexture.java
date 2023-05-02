// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import android.graphics.SurfaceTexture;
import android.util.Log;
import android.util.LongSparseArray;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.concurrent.atomic.AtomicInteger;

@JNINamespace("wolvic")
/* package */ final class WVRSurfaceTexture {
    private static final String TAG = "WVRSurfaceTexture";
    private static final LongSparseArray<WVRSurfaceTexture> sSurfaceTextures =
            new LongSparseArray<WVRSurfaceTexture>();

    private long mHandle;
    private SurfaceTexture mSurfaceTexture;

    private long mAttachedContext;
    private int mTexName;

    private AtomicInteger mUseCount;
    private boolean mIsLocked = false;

    @CalledByNative
    private static WVRSurfaceTexture create(long handle, SurfaceTexture surfaceTexture) {
        return new WVRSurfaceTexture(handle, surfaceTexture);
    }

    private WVRSurfaceTexture(long handle, SurfaceTexture surfaceTexture) {
        mHandle = handle;
        mSurfaceTexture = surfaceTexture;

        synchronized (sSurfaceTextures) {
            sSurfaceTextures.put(mHandle, this);
        }

        mUseCount = new AtomicInteger(1);

        // Start off detached
        mSurfaceTexture.detachFromGLContext();
    }

    public synchronized void attachToGLContext(final long context, final int texName) {
        if (context == mAttachedContext && texName == mTexName) {
            return;
        }

        mSurfaceTexture.attachToGLContext(texName);

        mAttachedContext = context;
        mTexName = texName;
    }

    public synchronized void detachFromGLContext() {
        mSurfaceTexture.detachFromGLContext();

        mAttachedContext = mTexName = 0;
    }

    public synchronized boolean isAttachedToGLContext(final long context) {
        return mAttachedContext == context;
    }

    public synchronized void updateTexImage() {
        try {
            mSurfaceTexture.updateTexImage();
        } catch (final Exception e) {
            Log.w(TAG, "updateTexImage() failed", e);
        }
    }

    public synchronized void release() {
        try {
            mSurfaceTexture.release();
            synchronized (sSurfaceTextures) {
                sSurfaceTextures.remove(mHandle);
            }
        } catch (final Exception e) {
            Log.w(TAG, "release() failed", e);
        }
    }

    public synchronized void releaseTexImage() {
        try {
            mSurfaceTexture.releaseTexImage();
        } catch (final Exception e) {
            Log.w(TAG, "releaseTexImage() failed", e);
        }
    }

    public synchronized void incrementUse() {
        mUseCount.incrementAndGet();
    }

    public synchronized void decrementUse() {
        final int useCount = mUseCount.decrementAndGet();

        if (useCount == 0) {
            if (mAttachedContext == 0) {
                release();
                sSurfaceTextures.remove(mHandle);
                return;
            }
        }
    }

    public static WVRSurfaceTexture lookup(final long handle) {
        synchronized (sSurfaceTextures) {
            return sSurfaceTextures.get(handle);
        }
    }
}
