// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gl;

import android.graphics.SurfaceTexture;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

/**
 * Listener to an android SurfaceTexture object for frame availability.
 */
@JNINamespace("gl")
@MainDex
class SurfaceTextureListener implements SurfaceTexture.OnFrameAvailableListener {
    // Used to determine the class instance to dispatch the native call to.
    private final long mNativeSurfaceTextureListener;

    SurfaceTextureListener(long nativeSurfaceTextureListener) {
        assert nativeSurfaceTextureListener != 0;
        mNativeSurfaceTextureListener = nativeSurfaceTextureListener;
    }

    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        SurfaceTextureListenerJni.get().frameAvailable(
                mNativeSurfaceTextureListener, SurfaceTextureListener.this);
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            SurfaceTextureListenerJni.get().destroy(
                    mNativeSurfaceTextureListener, SurfaceTextureListener.this);
        } finally {
            super.finalize();
        }
    }

    @NativeMethods
    interface Natives {
        void frameAvailable(long nativeSurfaceTextureListener, SurfaceTextureListener caller);
        void destroy(long nativeSurfaceTextureListener, SurfaceTextureListener caller);
    }
}
