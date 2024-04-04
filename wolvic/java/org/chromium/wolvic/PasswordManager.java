// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

@JNINamespace("wolvic")
public class PasswordManager {
    public interface Bridge {
        boolean isAutoFillEnabled(Context context);
        boolean isPasswordManagerEnabled(Context context);
        boolean saveOrUpdatePassword(PasswordForm form);
        boolean onLoginSelect(PasswordForm[] forms);
        void onLoginUsed(PasswordForm form);
        void dismiss();
    }

    public interface Listener {
        void onLoginSaved(PasswordForm form);
        void onLoginSelected(PasswordForm form);
    }

    public interface Factory {
        public Bridge create(Listener listener);
    }

    private static Factory sFactory;
    private final Bridge mBridge;
    private final long mNativePasswordManagerHandler;

    private PasswordManager(long nativePasswordManagerHandler) {
        Listener listener = new Listener() {
            @Override
            public void onLoginSaved(PasswordForm form) {
                PasswordManagerJni.get().onLoginSaved(mNativePasswordManagerHandler, form);
            }

            @Override
            public void onLoginSelected(PasswordForm form) {
                PasswordManagerJni.get().onLoginSelected(mNativePasswordManagerHandler, form);
            }
        };

        mNativePasswordManagerHandler = nativePasswordManagerHandler;
        mBridge = sFactory.create(listener);
    }

    public static void setFactory(Factory factory) {
        sFactory = factory;
    }

    @CalledByNative
    public static PasswordManager create(long nativePasswordManagerHandler) {
        return new PasswordManager(nativePasswordManagerHandler);
    }

    @CalledByNative
    public boolean isSavingAndFillingEnabled(@NonNull WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return false;
        return mBridge.isPasswordManagerEnabled(windowAndroid.getContext().get()) &&
                isFillingEnabled(windowAndroid);
    }

    @CalledByNative
    public boolean isFillingEnabled(@NonNull WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return false;
        return mBridge.isAutoFillEnabled(windowAndroid.getContext().get());
    }

    @CalledByNative
    public boolean saveOrUpdatePassword(PasswordForm form) {
        return mBridge.saveOrUpdatePassword(form);
    }

    @CalledByNative
    public boolean chooseCredentials(PasswordForm[] forms) {
        return mBridge.onLoginSelect(forms);
    }

    @CalledByNative
    public void notifySuccessfulLoginWithExistingPassword(PasswordForm form) {
    }

    @CalledByNative
    public void onPasswordAutofilled(PasswordForm form) {
        mBridge.onLoginUsed(form);
    }

    @CalledByNative
    public void dismissPrompt() {
        mBridge.dismiss();
    }

    @NativeMethods
    interface Natives {
        void onLoginSaved(long nativeWolvicPasswordManagerClient, PasswordForm form);
        void onLoginSelected(long nativeWolvicPasswordManagerClient, PasswordForm form);
    }
}
