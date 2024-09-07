// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import android.content.res.Resources;

import androidx.annotation.StringRes;

import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/** Set of shared helper functions for using {@link ModalDialogManager}. */
public final class ModalDialogUtils {
    /** Do not allow instantiation for utils class. */
    private ModalDialogUtils() {}

    /** Shows a dialog that only has a single confirmation button with no meaningful action. */
    public static void showOneButtonConfirmation(
            ModalDialogManager modalDialogManager,
            Resources resources,
            @StringRes int titleRes,
            @StringRes int messageParagraphRes,
            @StringRes int positiveButtonRes) {
        String titleText = resources.getString(titleRes);
        String messageParagraphText = resources.getString(messageParagraphRes);
        String positiveButtonText = resources.getString(positiveButtonRes);
        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, @ButtonType int buttonType) {
                        modalDialogManager.dismissDialog(
                                model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    }

                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {}
                };
        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE, titleText)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, messageParagraphText)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonText)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();
        modalDialogManager.showDialog(model, ModalDialogType.APP);
    }
}
