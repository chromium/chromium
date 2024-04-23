// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.stream.Stream;

@JNINamespace("wolvic")
public class PermissionManagerBridge {
    // Android permission which is requested when no android permissions are required for the given
    // permission request. This permission must be always granted.
    public static final String NO_ANDROID_PERMISSION = "org.chromium.wolvic.NO_ANDROID_PERMISSION";

    @IntDef({ContentPermissionType.GEOLOCATION, ContentPermissionType.DESKTOP_NOTIFICATION,
            ContentPermissionType.PERSISTENT_STORAGE, ContentPermissionType.XR,
            ContentPermissionType.AUTOPLAY_INAUDIBLE, ContentPermissionType.AUTOPLAY_AUDIBLE,
            ContentPermissionType.MEDIA_KEY_SYSTEM_ACCESS, ContentPermissionType.TRACKING,
            ContentPermissionType.STORAGE_ACCESS, ContentPermissionType.NOT_SUPPORTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContentPermissionType {
        int GEOLOCATION = 0;
        int DESKTOP_NOTIFICATION = 1;
        int PERSISTENT_STORAGE = 2;
        int XR = 3;
        int AUTOPLAY_INAUDIBLE = 4;
        int AUTOPLAY_AUDIBLE = 5;
        int MEDIA_KEY_SYSTEM_ACCESS = 6;
        int TRACKING = 7;
        int STORAGE_ACCESS = 8;

        int NOT_SUPPORTED = 9;
    }

    @IntDef({PermissionStatus.PROMPT, PermissionStatus.DENY, PermissionStatus.ALLOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PermissionStatus {
        int PROMPT = 0;
        int DENY = 1;
        int ALLOW = 2;
    }

    @IntDef({MediaSourceType.CAMERA, MediaSourceType.SCREEN, MediaSourceType.MICROPHONE,
            MediaSourceType.AUDIOCAPTURE, MediaSourceType.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MediaSourceType {
        int CAMERA = 0;
        int SCREEN = 1;
        int MICROPHONE = 2;
        int AUDIOCAPTURE = 3;
        int OTHER = 4;
    }

    @IntDef({MediaType.VIDEO, MediaType.AUDIO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MediaType {
        int VIDEO = 0;
        int AUDIO = 1;
    }

    public static class MediaSource {
        public final @NonNull String id;
        public final @NonNull String name;
        public final @MediaSourceType int source;
        public final @MediaType int type;

        public MediaSource(@NonNull String id, @Nullable String name,
                           @MediaSourceType int source,
                           @MediaType int type) {
            this.id = id;
            this.name = name;
            this.source = source;
            this.type = type;
        }
    }

    public interface PermissionCallback {
        void onPermissionResult(@PermissionStatus int[] results);
    }

    public interface MediaCallback {
        void onMediaPermissionResult(boolean granted, @Nullable String videoId,
                                     @Nullable String audioId);
    }

    public interface Delegate {
        void onContentPermissionRequest(@ContentPermissionType int[] permissionTypes, String url,
                                        boolean isOffTheRecord, PermissionCallback callback);
        void onAndroidPermissionRequest(String[] androidPermissionTypes,
                                        PermissionCallback callback);
        void onMediaPermissionRequest(String url, MediaSource[] video, MediaSource[] audio,
                                      MediaCallback callback);
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
    public static void onContentPermissionRequest(@ContentPermissionType int[] permissionTypes,
                                                  String url,
                                                  boolean isOffTheRecord,
                                                  long inProgressRequestPtr) {
        PermissionManagerBridge bridge = get();
        if (bridge.mDelegate == null) {
            return;
        }
        bridge.mDelegate.onContentPermissionRequest(
                permissionTypes,
                url,
                isOffTheRecord,
                new PermissionCallback() {
                    @Override
                    public void onPermissionResult(@PermissionStatus int[] results) {
                        PermissionManagerBridgeJni.get().onContentPermissionResult(
                                isOffTheRecord,
                                inProgressRequestPtr,
                                results);
                    }
        });
    }

    @CalledByNative
    public static void onAndroidPermissionRequest(String[] permissionTypes,
                                                  boolean isOffTheRecord,
                                                  long inProgressRequestPtr) {
        PermissionManagerBridge bridge = get();
        if (bridge.mDelegate == null) {
            return;
        }
        bridge.mDelegate.onAndroidPermissionRequest(
                permissionTypes,
                new PermissionCallback() {
                    @Override
                    public void onPermissionResult(@PermissionStatus int[] results) {
                        PermissionManagerBridgeJni.get().onAndroidPermissionResult(
                                isOffTheRecord,
                                inProgressRequestPtr,
                                results);
                    }
        });
    }

    @CalledByNative
    public static void onMediaPermissionRequest(MediaSource[] video, MediaSource[] audio,
                                                String url, boolean isOffTheRecord,
                                                long inProgressRequestPtr) {
        PermissionManagerBridge bridge = get();
        if (bridge.mDelegate == null) {
            return;
        }
        bridge.mDelegate.onMediaPermissionRequest(url, video, audio, new MediaCallback() {
            @Override
            public void onMediaPermissionResult(boolean granted, @Nullable String videoId,
                                                @Nullable String audioId) {
                PermissionManagerBridgeJni.get().onMediaPermissionResult(isOffTheRecord,
                        inProgressRequestPtr, granted, videoId, audioId);
            }
        });
    }

    @NativeMethods
    public interface Natives {
        void onContentPermissionResult(boolean isOffTheRecord,
                                       long inProgressRequestPtr,
                                       @PermissionStatus int[] results);
        void onAndroidPermissionResult(boolean isOffTheRecord,
                                       long inProgressRequestPtr,
                                       @PermissionStatus int[] results);
        void onMediaPermissionResult(boolean isOffTheRecord,
                                     long inProgressRequestPtr,
                                     boolean granted,
                                     @Nullable String videoId,
                                     @Nullable String audioId);
    }
}