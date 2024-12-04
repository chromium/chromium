// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util.modaldialog;

import androidx.activity.ComponentDialog;

import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A fake ModalDialogManager for use in tests involving modals. Unlike ModalDialogManager, this
 * class is managed by its native `FakeModalDialogManagerBridge`.
 */
@JNINamespace("ui")
public class FakeModalDialogManager extends ModalDialogManager {
    private PropertyModel mShownDialogModel;

    @CalledByNativeForTesting
    private static FakeModalDialogManager createForTab(boolean useEmptyPresenter) {
        ModalDialogManager.Presenter presenter =
                useEmptyPresenter
                        ? new ModalDialogManager.Presenter() {
                            @Override
                            protected void addDialogView(
                                    PropertyModel model,
                                    Callback<ComponentDialog> onDialogCreatedCallback) {}

                            @Override
                            protected void removeDialogView(PropertyModel model) {}
                        }
                        : Mockito.mock(Presenter.class);
        return new FakeModalDialogManager(presenter, ModalDialogType.TAB);
    }

    public FakeModalDialogManager(int modalDialogType) {
        this(Mockito.mock(Presenter.class), modalDialogType);
    }

    public FakeModalDialogManager(ModalDialogManager.Presenter presenter, int modalDialogType) {
        super(presenter, modalDialogType);
    }

    @Override
    public void showDialog(PropertyModel model, int dialogType) {
        mShownDialogModel = model;
    }

    @Override
    public void dismissDialog(PropertyModel model, int dismissalCause) {
        model.get(ModalDialogProperties.CONTROLLER).onDismiss(model, dismissalCause);
        mShownDialogModel = null;
    }

    @Override
    @CalledByNativeForTesting
    public boolean isSuspended(@ModalDialogType int dialogType) {
        return super.isSuspended(dialogType);
    }

    @CalledByNativeForTesting
    public void clickPositiveButton() {
        mShownDialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(mShownDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
    }

    public void clickNegativeButton() {
        mShownDialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(mShownDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
    }

    public PropertyModel getShownDialogModel() {
        return mShownDialogModel;
    }
}
