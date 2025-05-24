// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A default implementation of Controller which dismisses the dialog when a button is clicked.
 *
 * The result of the dialog is passed back via a Callback.
 */
@NullMarked
public class SimpleModalDialogController implements ModalDialogProperties.Controller {
    private final ModalDialogManager mModalDialogManager;
    private @Nullable Callback<Integer> mActionCallback;

    /**
     * @param modalDialogManager the dialog manager where the dialog will be shown.
     * @param action a callback which will be run with the result of the confirmation.
     */
    public SimpleModalDialogController(
            ModalDialogManager modalDialogManager, Callback<Integer> action) {
        mModalDialogManager = modalDialogManager;
        mActionCallback = action;
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
    public void onDismiss(PropertyModel model, int dismissalCause) {
        Callback<Integer> action = mActionCallback;
        assert action != null;
        mActionCallback = null;
        action.onResult(dismissalCause);
    }
}
