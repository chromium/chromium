// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.ui.ModalDialogWrapper;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

@JNINamespace("ui")
public class ModalDialogManagerBridge {

    private final ModalDialogManager mModalDialogManager;
    private long mNativePtr;

    public ModalDialogManagerBridge(@NonNull ModalDialogManager manager) {
        mModalDialogManager = manager;
        mNativePtr = ModalDialogManagerBridgeJni.get().create(this);
    }

    public long getNativePtr() {
        return mNativePtr;
    }

    public void destroyNative() {
        assert mNativePtr != 0;
        ModalDialogManagerBridgeJni.get().destroy(mNativePtr);
        mNativePtr = 0;
    }

    @CalledByNative
    private int suspendModalDialogs(@ModalDialogType int dialogType) {
        assert mNativePtr != 0;
        return mModalDialogManager.suspendType(dialogType);
    }

    @CalledByNative
    private void resumeModalDialogs(@ModalDialogType int dialogType, int token) {
        assert mNativePtr != 0;
        mModalDialogManager.resumeType(dialogType, token);
    }

    @CalledByNative
    private void showDialog(ModalDialogWrapper dialog, @ModalDialogType int dialogType) {
        mModalDialogManager.showDialog(dialog.getPropertyModel(), dialogType);
    }

    @CalledByNative
    private void dismissDialog(ModalDialogWrapper dialog) {
        mModalDialogManager.dismissDialog(
                dialog.getPropertyModel(), DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    @NativeMethods
    interface Natives {
        long create(ModalDialogManagerBridge manager);

        void destroy(long mNativePtr);
    }
}
