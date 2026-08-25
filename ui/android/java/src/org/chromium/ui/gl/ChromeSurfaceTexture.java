// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gl;

import android.graphics.SurfaceTexture;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/** Exposes SurfaceTexture APIs to native. */
@JNINamespace("gl")
@NullMarked
class ChromeSurfaceTexture extends SurfaceTexture {
    @CalledByNative
    ChromeSurfaceTexture(int textureId) {
        super(textureId);
    }

    @CalledByNative
    public void destroy() {
        release();
    }
}
