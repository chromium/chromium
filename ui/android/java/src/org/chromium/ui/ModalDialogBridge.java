// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

@JNINamespace("ui")
public class ModalDialogBridge implements ModalDialogProperties.Controller {
    /** The native-side counterpart of this class */
    private long mNativeDelegatePtr;

    private PropertyModel.Builder mPropertyModelBuilder;
    private ModalDialogManager mModalDialogManager;

    @CalledByNative
    private static ModalDialogBridge create(long nativeDelegatePtr) {
        return new ModalDialogBridge(nativeDelegatePtr);
    }

    private ModalDialogBridge(long nativeDelegatePtr) {
        mNativeDelegatePtr = nativeDelegatePtr;
        mPropertyModelBuilder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this);
    }

    @CalledByNative
    private void withTitleAndButtons(String title, String positiveButton, String negativeButton) {
        mPropertyModelBuilder
                .with(ModalDialogProperties.TITLE, title)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButton)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButton);
    }

    @CalledByNative
    private void withParagraph1(String text) {
        mPropertyModelBuilder.with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, text);
    }

    @CalledByNative
    private void showTabModal(WindowAndroid window) {
        mModalDialogManager = window.getModalDialogManager();
        mModalDialogManager.showDialog(
                mPropertyModelBuilder.build(), ModalDialogManager.ModalDialogType.TAB);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    @CalledByNative
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                ModalDialogBridgeJni.get().positiveButtonClicked(mNativeDelegatePtr);
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                ModalDialogBridgeJni.get().negativeButtonClicked(mNativeDelegatePtr);
                break;
            default:
                ModalDialogBridgeJni.get().dismissed(mNativeDelegatePtr);
                break;
        }
        ModalDialogBridgeJni.get().destroy(mNativeDelegatePtr);
    }

    @NativeMethods
    interface Natives {
        void positiveButtonClicked(long nativeModalDialogBridge);

        void negativeButtonClicked(long nativeModalDialogBridge);

        void dismissed(long nativeModalDialogBridge);

        void destroy(long nativeModalDialogBridge);
    }
}
