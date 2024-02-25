// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gl;

import android.graphics.SurfaceTexture;
import android.util.Log;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Wrapper class for the underlying platform's SurfaceTexture in order to
 * provide a stable JNI API.
 */
@JNINamespace("gl")
class SurfaceTexturePlatformWrapper {
    private static final String TAG = "SurfaceTexturePlatformWrapper";

    @CalledByNative
    private static SurfaceTexture create(int textureId) {
        return new SurfaceTexture(textureId);
    }

    @CalledByNative
    private static void destroy(SurfaceTexture surfaceTexture) {
        surfaceTexture.setOnFrameAvailableListener(null);
        surfaceTexture.release();
    }

    @CalledByNative
    private static void setFrameAvailableCallback(
            SurfaceTexture surfaceTexture, long nativeSurfaceTextureListener) {
        surfaceTexture.setOnFrameAvailableListener(
                new SurfaceTextureListener(nativeSurfaceTextureListener));
    }

    @CalledByNative
    private static void updateTexImage(SurfaceTexture surfaceTexture) {
        try {
            surfaceTexture.updateTexImage();
        } catch (RuntimeException e) {
            Log.e(TAG, "Error calling updateTexImage", e);
        }
    }

    @CalledByNative
    private static void getTransformMatrix(SurfaceTexture surfaceTexture, float[] matrix) {
        surfaceTexture.getTransformMatrix(matrix);
    }

    @CalledByNative
    private static void attachToGLContext(SurfaceTexture surfaceTexture, int texName) {
        surfaceTexture.attachToGLContext(texName);
    }

    @CalledByNative
    private static void detachFromGLContext(SurfaceTexture surfaceTexture) {
        surfaceTexture.detachFromGLContext();
    }

    @CalledByNative
    private static void release(SurfaceTexture surfaceTexture) {
        surfaceTexture.release();
    }

    @CalledByNative
    private static void setDefaultBufferSize(SurfaceTexture surfaceTexture, int width, int height) {
        surfaceTexture.setDefaultBufferSize(width, height);
    }
}
