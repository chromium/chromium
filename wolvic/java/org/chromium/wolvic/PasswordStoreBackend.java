// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import android.util.Log;
@JNINamespace("wolvic")
public class PasswordStoreBackend {
    public interface Bridge {
        void addLogin(int replyId, PasswordForm form);
        void updateLogin(int replyId, PasswordForm form);
        void removeLogin(int replyId, PasswordForm form);
        void getAllLogins(int replyId);
        void getLoginsForSignonRealm(int replyId, String signonRealm);
        void getAutofillableLogins(int replyId);
    }

    public interface Factory {
        public Bridge create(Listener listener);
    }

    public interface Listener {
        void onCompleteWithLogins(int replyId, PasswordForm[] passwords);
        void onLoginChanged(int replyId);
        void onError(int replyId, Exception exception);
    }

    private static Factory sFactory;
    private final Bridge mBridge;
    private long mNativePasswordStoreBackendHandler;

    private PasswordStoreBackend(long nativePasswordStoreBackendHandler) {
        mNativePasswordStoreBackendHandler = nativePasswordStoreBackendHandler;
        mBridge = sFactory.create(new Listener() {
            @Override
            public void onCompleteWithLogins(int replyId, PasswordForm[] passwords) { 
                if (mNativePasswordStoreBackendHandler == 0) return;

                PasswordStoreBackendJni.get().onCompleteWithLogins(mNativePasswordStoreBackendHandler, replyId, passwords);
            }

            @Override
            public void onLoginChanged(int replyId) {
                if (mNativePasswordStoreBackendHandler == 0) return;

                PasswordStoreBackendJni.get().onLoginChanged(mNativePasswordStoreBackendHandler, replyId);
            }

            @Override
            public void onError(int replyId, Exception exception) {
                PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
                    if (mNativePasswordStoreBackendHandler == 0) return;
                    PasswordStoreBackendJni.get().onError(mNativePasswordStoreBackendHandler,
                            replyId, exception.toString());                            
                });
            }
        });
    }

    public static void setFactory(Factory factory) {
        sFactory = factory;
    }

    @CalledByNative
    public static PasswordStoreBackend create(long nativePasswordStoreBackendHandler) {
        return new PasswordStoreBackend(nativePasswordStoreBackendHandler);
    }

    @CalledByNative
    public void addLogin(int replyId, PasswordForm form) {
        mBridge.addLogin(replyId, form);
    }

    @CalledByNative
    public void updateLogin(int replyId, PasswordForm form) {
        mBridge.updateLogin(replyId, form);
    }

    @CalledByNative
    public void removeLogin(int replyId, PasswordForm form) {
        mBridge.removeLogin(replyId, form);
    }

    @CalledByNative
    public void getAllLogins(int replyId) {
        mBridge.getAllLogins(replyId);
    }

    @CalledByNative
    public void getLoginsForSignonRealm(int replyId, String signonRealm) {
        mBridge.getLoginsForSignonRealm(replyId, signonRealm);
    }

    @CalledByNative
    public void getAutofillableLogins(int replyId) {
        mBridge.getAutofillableLogins(replyId);
    }

    @CalledByNative
    private void destroy() {
        mNativePasswordStoreBackendHandler = 0;
    }

    @NativeMethods
    interface Natives {
        void onCompleteWithLogins(long nativeWolvicPasswordStoreBackend, int replyId, PasswordForm[] passwords);
        void onLoginChanged(long nativeWolvicPasswordStoreBackend, int replyId);
        void onError(long nativeWolvicPasswordStoreBackend, int replyId, String error);
    }
}
