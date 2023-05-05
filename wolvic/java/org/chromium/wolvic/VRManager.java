// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import androidx.annotation.AnyThread;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("content")
public class VRManager {
    private static long mExternalContext;

    @CalledByNative
    private static synchronized long getExternalContext() {
        assert mExternalContext != 0;
        return mExternalContext;
    }

    @AnyThread
    @SuppressWarnings("NoSynchronizedMethodCheck")
    public static synchronized void setExternalContext(final long externalContext) {
        mExternalContext = externalContext;
    }
}
