// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.chromium.url.GURL;

/**
 * Manages showing http auth prompt.
 */
@JNINamespace("wolvic")
public final class HttpAuthManager {
    public interface Bridge {
        void show();
        void close();
    }

    public interface Listener {
        void proceed(String username, String password);
        void cancel();
    }

    public interface Factory {
        public Bridge create(GURL url, boolean isProxy,
                             boolean firstAuthAttempt, Listener listener);
    }

    private static Factory sFactory;
    private final Bridge mBridge;
    private long mNativeHttpAuthManagerHandle;

    @CalledByNative
    public static HttpAuthManager create(long nativeHttpAuthManagerHandle, GURL url,
                                         boolean isProxy, boolean firstAuthAttempt) {
        return new HttpAuthManager(nativeHttpAuthManagerHandle, url, isProxy, firstAuthAttempt);
    }

    @CalledByNative
    void destroyed() {
        mNativeHttpAuthManagerHandle = 0;
    }

    @CalledByNative
    private void closeDialog() {
        if (mBridge != null) mBridge.close();
    }

    public static void setFactory(Factory factory) {
        sFactory = factory;
    }

    private HttpAuthManager(long nativeHttpAuthManagerHandle, GURL url, boolean isProxy,
                            boolean firstAuthAttempt) {
        mNativeHttpAuthManagerHandle = nativeHttpAuthManagerHandle;

        Listener listener = new Listener() {
            @Override
            public void proceed(String username, String password) {
                if (mNativeHttpAuthManagerHandle != 0) {
                    HttpAuthManagerJni.get().proceed(mNativeHttpAuthManagerHandle, username, password);
                }
            }

            @Override
            public void cancel() {
                if (mNativeHttpAuthManagerHandle != 0) {
                    HttpAuthManagerJni.get().cancel(mNativeHttpAuthManagerHandle);
                }
            }
        };

        mBridge = sFactory.create(url, isProxy, firstAuthAttempt, listener);
        mBridge.show();
    }

    @NativeMethods
    interface Natives {
        void proceed(long nativeHttpAuthManager, String username, String password);
        void cancel(long nativeHttpAuthManager);
    }
}
