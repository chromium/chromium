// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.ModalDialogWrapper;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

@JNINamespace("ui")
@NullMarked
public class ModalDialogManagerBridge {
    @JniType("::ui::ModalDialogManagerBridge")
    public interface NativeModalDialogManagerBridge extends JniTypeToken {}

    private final ModalDialogManager mModalDialogManager;
    private final JniUniquePtr<NativeModalDialogManagerBridge> mNativePtr;

    public ModalDialogManagerBridge(ModalDialogManager manager) {
        mModalDialogManager = manager;
        mNativePtr = ModalDialogManagerBridgeJni.get().create(this);
    }

    public JniPtr<NativeModalDialogManagerBridge> getNativePtr() {
        return mNativePtr;
    }

    public void destroyNative() {
        mNativePtr.destroy();
    }

    @CalledByNative
    private int suspendModalDialogs(@ModalDialogType int dialogType) {
        return mModalDialogManager.suspendType(dialogType);
    }

    @CalledByNative
    private void resumeModalDialogs(@ModalDialogType int dialogType, int token) {
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
        JniUniquePtr<NativeModalDialogManagerBridge> create(ModalDialogManagerBridge manager);
    }
}
