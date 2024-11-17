// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gl;

import android.graphics.SurfaceTexture;
import android.util.Log;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Exposes SurfaceTexture APIs to native. */
@JNINamespace("gl")
class ChromeSurfaceTexture extends SurfaceTexture
        implements SurfaceTexture.OnFrameAvailableListener {
    private static final String TAG = "SurfaceTexture";

    // Used to determine the class instance to dispatch the native call to.
    private long mNativeSurfaceTextureListener;

    @CalledByNative
    ChromeSurfaceTexture(int textureId) {
        super(textureId);
    }

    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        if (mNativeSurfaceTextureListener != 0) {
            ChromeSurfaceTextureJni.get().frameAvailable(mNativeSurfaceTextureListener);
        }
    }

    @CalledByNative
    public void destroy() {
        if (mNativeSurfaceTextureListener != 0) {
            ChromeSurfaceTextureJni.get().destroy(mNativeSurfaceTextureListener);
            mNativeSurfaceTextureListener = 0;
        }
        release();
    }

    @CalledByNative
    public void setNativeListener(long nativeSurfaceTextureListener) {
        assert mNativeSurfaceTextureListener == 0;
        mNativeSurfaceTextureListener = nativeSurfaceTextureListener;
        setOnFrameAvailableListener(this);
    }

    @CalledByNative
    @Override
    public void updateTexImage() {
        try {
            super.updateTexImage();
        } catch (RuntimeException e) {
            Log.e(TAG, "Error calling updateTexImage", e);
        }
    }

    @CalledByNative
    @Override
    public void getTransformMatrix(float[] matrix) {
        super.getTransformMatrix(matrix);
    }

    @CalledByNative
    @Override
    public void attachToGLContext(int texName) {
        super.attachToGLContext(texName);
    }

    @CalledByNative
    @Override
    public void detachFromGLContext() {
        super.detachFromGLContext();
    }

    @CalledByNative
    @Override
    public void setDefaultBufferSize(int width, int height) {
        super.setDefaultBufferSize(width, height);
    }

    @NativeMethods
    interface Natives {
        // These are methods on SurfaceTextureListener.
        void frameAvailable(long nativeSurfaceTextureListener);

        void destroy(long nativeSurfaceTextureListener);
    }
}
