// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

@JNINamespace("ui")
@NullMarked
public class ModalDialogWrapper implements ModalDialogProperties.Controller {
    /** The native-side counterpart of this class */
    private final long mNativeDelegatePtr;

    private final @Nullable ModalDialogManager mModalDialogManager;

    private final PropertyModel.Builder mPropertyModelBuilder;

    @CalledByNative
    private static ModalDialogWrapper create(long nativeDelegatePtr, WindowAndroid window) {
        return new ModalDialogWrapper(nativeDelegatePtr, window);
    }

    private ModalDialogWrapper(long nativeDelegatePtr, WindowAndroid window) {
        mNativeDelegatePtr = nativeDelegatePtr;
        mModalDialogManager = window.getModalDialogManager();
        mPropertyModelBuilder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this);
    }

    public PropertyModel getPropertyModel() {
        return mPropertyModelBuilder.build();
    }

    @CalledByNative
    private void withTitleAndButtons(
            String title, String positiveButton, String negativeButton, int buttonStyles) {
        mPropertyModelBuilder
                .with(ModalDialogProperties.TITLE, title)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButton)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButton)
                .with(ModalDialogProperties.BUTTON_STYLES, buttonStyles);
    }

    @CalledByNative
    private void withParagraph1(String text) {
        mPropertyModelBuilder.with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, text);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        ModalDialogManager modalDialogManager = assumeNonNull(mModalDialogManager);
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            modalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else {
            modalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                ModalDialogWrapperJni.get().positiveButtonClicked(mNativeDelegatePtr);
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                ModalDialogWrapperJni.get().negativeButtonClicked(mNativeDelegatePtr);
                break;
            default:
                ModalDialogWrapperJni.get().dismissed(mNativeDelegatePtr);
                break;
        }
        ModalDialogWrapperJni.get().destroy(mNativeDelegatePtr);
    }

    @NativeMethods
    interface Natives {
        void positiveButtonClicked(long nativeModalDialogWrapper);

        void negativeButtonClicked(long nativeModalDialogWrapper);

        void dismissed(long nativeModalDialogWrapper);

        void destroy(long nativeModalDialogWrapper);
    }
}
