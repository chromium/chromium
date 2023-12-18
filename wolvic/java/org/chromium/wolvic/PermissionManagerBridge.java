// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.Arrays;
import java.util.stream.Stream;

@JNINamespace("wolvic")
public class PermissionManagerBridge {
    // Android permission which is requested when no android permissions are required for the given
    // permission request. This permission must be always granted.
    public static final String NO_ANDROID_PERMISSION = "org.chromium.wolvic.NO_ANDROID_PERMISSION";

    public enum PermissionType {
        GEOLOCATION(0),
        DESKTOP_NOTIFICATION(1),
        PERSISTENT_STORAGE(2),
        XR(3),
        AUTOPLAY_INAUDIBLE(4),
        AUTOPLAY_AUDIBLE(5),
        MEDIA_KEY_SYSTEM_ACCESS(6),
        TRACKING(7),
        STORAGE_ACCESS(8),

        // Any chromium permissions that are not yet supported by Wolvic are
        // mapped into this value and automatically denied.
        NOT_SUPPORTED(9);

        public static final PermissionType[] types = PermissionType.values();

        private final int value;

        private PermissionType(int value) {
            this.value = value;
        }

        private static PermissionType fromValue(int value) {
            return types[value];
        }

        private int getValue() {
            return value;
        }

    }

    public enum PermissionStatus {
        PROMPT(0),
        DENY(1),
        ALLOW(2);

        public static final PermissionStatus[] statuses = PermissionStatus.values();

        private final int value;

        private PermissionStatus(int value) {
            this.value = value;
        }

        private static PermissionStatus fromValue(int value) {
            return statuses[value];
        }

        private int getValue() {
            return value;
        }
    }

    public interface PermissionCallback {
        void onPermissionResult(PermissionStatus[] results);
    }

    public interface Delegate {
        void onPermissionRequest(PermissionType[] permissionTypes, String[] androidPermissionTypes,
                                 String url, boolean isOffTheRecord, PermissionCallback callback);
    }
    
    private static PermissionManagerBridge sInstance;
    private Delegate mDelegate;

    public static PermissionManagerBridge get() {
        if (sInstance == null) {
            sInstance = new PermissionManagerBridge();
        }
        return sInstance;
    }

    public void setDelegate(@NonNull Delegate delegate) {
        mDelegate = delegate;
    }

    @CalledByNative
    public static void onPermissionRequest(int[] permissionTypes,
                                           String[] androidPermissionTypes,
                                           String url,
                                           boolean isOffTheRecord,
                                           long inProgressRequestPtr) {
        PermissionManagerBridge bridge = get();
        if (bridge.mDelegate == null) {
            return;
        }
        bridge.mDelegate.onPermissionRequest(
                Arrays.stream(permissionTypes)
                        .mapToObj(PermissionType::fromValue)
                        .toArray(PermissionType[]::new),
                androidPermissionTypes,
                url,
                isOffTheRecord,
                new PermissionCallback() {
                    @Override
                    public void onPermissionResult(PermissionStatus[] results) {
                        PermissionManagerBridgeJni.get().onPermissionResult(
                                isOffTheRecord,
                                inProgressRequestPtr,
                                Arrays.stream(results)
                                        .mapToInt(PermissionStatus::getValue)
                                        .toArray());
                    }
        });
    }

    @NativeMethods
    public interface Natives {
        void onPermissionResult(boolean isOffTheRecord, long inProgressRequestPtr, int[] results);
    }
}