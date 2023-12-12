// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

@JNINamespace("wolvic")
public class UserDialogManagerBridge {
    public static interface Delegate {
        void onAlertDialog(@NonNull String message, DialogCallback callback);
        void onConfirmDialog(@NonNull String message, DialogCallback callback);
        void onTextDialog(@NonNull String message, @NonNull String defaultUserInput, DialogCallback callback);
        void onBeforeUnloadDialog(DialogCallback callback);
    }

    public static class DialogCallback {
        private final long mInProgressDialogPtr;

        public DialogCallback(long inProgressDialogPtr) {
            mInProgressDialogPtr = inProgressDialogPtr;
        }

        public void confirm(@Nullable String userInput) {
            UserDialogManagerBridgeJni.get().confirmDialog(mInProgressDialogPtr, userInput);
        }

        public void dismiss() {
            UserDialogManagerBridgeJni.get().dismissDialog(mInProgressDialogPtr);
        }
    }

    private static UserDialogManagerBridge sInstance;
    private Delegate mDelegate;

    public static UserDialogManagerBridge get() {
        if (sInstance == null) {
            sInstance = new UserDialogManagerBridge();
        }
        return sInstance;
    }

    // Called from the Java side to set the delegate that will accept the calls
    // from the native side.
    public void setDelegate(@NonNull Delegate delegate) {
        mDelegate = delegate;
    }

    // The following methods are called from the native side to ask user dialog manager to
    // show the corresponding dialog.
    @CalledByNative
    public static void onAlertDialog(@NonNull String message, long inProgressDialogPtr) {
        UserDialogManagerBridge bridge = get();
        if (bridge.mDelegate != null) {
            bridge.mDelegate.onAlertDialog(message, new DialogCallback(inProgressDialogPtr));
        }
    }

    @CalledByNative
    public static void onConfirmDialog(@NonNull String message, long inProgressDialogPtr) {
        UserDialogManagerBridge bridge = get();
        if (bridge.mDelegate != null) {
            bridge.mDelegate.onConfirmDialog(message, new DialogCallback(inProgressDialogPtr));
        }
    }

    @CalledByNative
    public static void onTextDialog(@NonNull String message, @NonNull String defaultUserInput, long inProgressDialogPtr) {
        UserDialogManagerBridge bridge = get();
        if (bridge.mDelegate != null) {
            bridge.mDelegate.onTextDialog(message, defaultUserInput, new DialogCallback(inProgressDialogPtr));
        }
    }


    @CalledByNative
    public static void onBeforeUnloadDialog(long inProgressDialogPtr) {
        UserDialogManagerBridge bridge = get();
        if (bridge.mDelegate != null) {
            bridge.mDelegate.onBeforeUnloadDialog(new DialogCallback(inProgressDialogPtr));
        }
    }

    @NativeMethods
    public interface Natives {
        void confirmDialog(long inProgressDialogPtr, @Nullable String userInput);
        void dismissDialog(long inProgressDialogPtr);
    }
}