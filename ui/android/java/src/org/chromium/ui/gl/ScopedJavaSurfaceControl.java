// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gl;

import android.os.Build;
import android.view.SurfaceControl;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

@RequiresApi(Build.VERSION_CODES.Q)
@JNINamespace("gl")
class ScopedJavaSurfaceControl {
    @CalledByNative
    private static void releaseSurfaceControl(SurfaceControl surfaceControl) {
        surfaceControl.release();
    }
}
