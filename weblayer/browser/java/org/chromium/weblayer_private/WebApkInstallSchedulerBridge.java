// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.webapps.WebApkInstallResult;

/**
 * Java counterpart to webapk_install_scheduler_bridge.
 * Contains functionality to schedule WebAPK installs with the {@link
 * WebApkInstallCoordinatorService} in Chrome. This Java object is created by and owned by the
 * native WebApkInstallSchedulerBridge.
 */
@JNINamespace("weblayer")
public class WebApkInstallSchedulerBridge {
    // Raw pointer to the native WebApkInstallSchedulerBridge that is cleared when the native object
    // destroys this object.
    private long mNativePointer;

    /**
     * This callback is passed via the {@link WebApkInstallSchedulerClient} to the Chrome
     * {@link WebApkInstallCoordinatorService} and will be called from there with the result
     * of the install. This result is passed along to the native side.
     */
    Callback<Integer> mOnInstallCompleteCallback = (result) -> {
        if (mNativePointer != 0) {
            WebApkInstallSchedulerBridgeJni.get().onInstallFinished(
                    mNativePointer, WebApkInstallSchedulerBridge.this, result);
        }
    };

    private WebApkInstallSchedulerBridge(long nativePtr) {
        mNativePointer = nativePtr;
    }

    @CalledByNative
    private static WebApkInstallSchedulerBridge create(long nativePtr) {
        return new WebApkInstallSchedulerBridge(nativePtr);
    }

    @CalledByNative
    public void scheduleInstall(
            byte[] apkProto, Bitmap primaryIcon, boolean isPrimaryIconMaskable) {
        WebApkInstallSchedulerClient.scheduleInstall(
                apkProto, primaryIcon, isPrimaryIconMaskable, mOnInstallCompleteCallback);
    }

    /**
     * Returns if the {@link WebApkInstallCoordinatorService} is available.
     */
    @CalledByNative
    public static boolean isInstallServiceAvailable() {
        return WebApkInstallSchedulerClient.isInstallServiceAvailable();
    }

    @CalledByNative
    private void destroy() {
        // Remove the reference to the native side.
        mNativePointer = 0;
    }

    @NativeMethods
    interface Natives {
        void onInstallFinished(long nativeWebApkInstallSchedulerBridge,
                WebApkInstallSchedulerBridge caller, @WebApkInstallResult int result);
    }
}
