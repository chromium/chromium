// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.permissions;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.permissions.AndroidPermissionRequester;
import org.chromium.ui.base.WindowAndroid;

/** Util functions to request Android permissions for a content setting. */
@JNINamespace("weblayer")
public final class PermissionRequestUtils {
    @CalledByNative
    private static void requestPermission(
            WindowAndroid windowAndroid, long nativeCallback, int[] contentSettingsTypes) {
        if (!AndroidPermissionRequester.requestAndroidPermissions(windowAndroid,
                    contentSettingsTypes, new AndroidPermissionRequester.RequestDelegate() {
                        @Override
                        public void onAndroidPermissionAccepted() {
                            PermissionRequestUtilsJni.get().onResult(nativeCallback, true);
                        }

                        @Override
                        public void onAndroidPermissionCanceled() {
                            PermissionRequestUtilsJni.get().onResult(nativeCallback, false);
                        }
                    })) {
            PermissionRequestUtilsJni.get().onResult(nativeCallback, false);
        }
    }

    @NativeMethods
    interface Natives {
        void onResult(long callback, boolean result);
    }
}
