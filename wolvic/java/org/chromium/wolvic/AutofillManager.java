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
public class AutofillManager {
    public interface Bridge {
        boolean isAutocompleteEnabled(Context context);
        boolean isPasswordManagerEnabled(Context context);
        boolean onLoginSelect(String[] usernames);
        void dismiss();
    }

    public interface Listener {
        void onLoginSelected(int index);
    }

    public interface Factory {
        public Bridge create(Listener listener);
    }

    private static Factory sFactory;
    private final Bridge mBridge;
    private final long mNativeAutofillManagerHandler;
    private String[] mUserNames;

    private AutofillManager(long nativeAutofillManagerHandler) {
        Listener listener = new Listener() {
            @Override
            public void onLoginSelected(int index) {
                AutofillManagerJni.get().onLoginSelected(mNativeAutofillManagerHandler, index);
            }
        };

        mNativeAutofillManagerHandler = nativeAutofillManagerHandler;
        mBridge = sFactory.create(listener);
    }

    public static void setFactory(Factory factory) {
        sFactory = factory;
    }

    @CalledByNative
    public static AutofillManager create(long nativeAutofillManagerHandler) {
        return new AutofillManager(nativeAutofillManagerHandler);
    }

    @CalledByNative
    public boolean isAutocompleteEnabled(@NonNull WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return false;
        return mBridge.isAutocompleteEnabled(windowAndroid.getContext().get());
    }

    @CalledByNative
    public boolean isPasswordManagerEnabled(@NonNull WindowAndroid windowAndroid) {
        if (windowAndroid.getContext().get() == null) return false;
        return mBridge.isPasswordManagerEnabled(windowAndroid.getContext().get());
    }

    @CalledByNative
    public void createUsernameArray(int arraySize) {
        mUserNames = arraySize > 0 ? new String[arraySize] : null;
    }

    @CalledByNative
    public void addUsername(int idx, String username) {
        if (idx < 0 || idx >= mUserNames.length) return;
        mUserNames[idx] = username;
    }

    @CalledByNative
    public boolean showAutofillPopup() {
        if (mUserNames == null) return false;
        return mBridge.onLoginSelect(mUserNames);
    }

    @CalledByNative
    public void dismissPrompt() {
        mBridge.dismiss();
    }

    @NativeMethods
    interface Natives {
        void onLoginSelected(long nativeWolvicAutofillClient, int index);
    }
}
