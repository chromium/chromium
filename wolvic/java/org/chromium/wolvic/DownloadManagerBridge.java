// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("wolvic")
public class DownloadManagerBridge {
    public interface Delegate {
        void newDownload(String url);
    }

    private static DownloadManagerBridge sInstance;
    private Delegate mDelegate;

    public static DownloadManagerBridge get() {
        if (sInstance == null) {
            sInstance = new DownloadManagerBridge();
        }
        return sInstance;
    }

    // Called from the Java side to set the delegate that will accept the calls
    // from the native side.
    public void setDelegate(@NonNull Delegate delegate) {
        mDelegate = delegate;
    }

    // Called from the native side to notify Wolvic about the new download that
    // must be handled by Wolvic.
    @CalledByNative
    public static void newDownload(String url) {
        DownloadManagerBridge bridge = get();
        if (bridge.mDelegate != null) {
            bridge.mDelegate.newDownload(url);
        }
    }
}
