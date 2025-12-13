// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util.modaldialog;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.activity.ComponentDialog;

import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

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
                                    Callback<ComponentDialog> onDialogCreatedCallback,
                                    Callback<View> onDialogShownCallback) {}

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

    @CalledByNativeForTesting
    public void clickNegativeButton() {
        mShownDialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(mShownDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
    }

    @CalledByNativeForTesting
    public void toggleCheckbox() {
        boolean isCurrentlyChecked = mShownDialogModel.get(ModalDialogProperties.CHECKBOX_CHECKED);

        mShownDialogModel.set(ModalDialogProperties.CHECKBOX_CHECKED, !isCurrentlyChecked);
        ModalDialogProperties.Controller controller =
                mShownDialogModel.get(ModalDialogProperties.CONTROLLER);
        if (controller != null) {
            controller.onCheckboxChecked(!isCurrentlyChecked);
        }
    }

    @CalledByNativeForTesting
    public boolean isCheckboxChecked() {
        return mShownDialogModel.get(ModalDialogProperties.CHECKBOX_CHECKED);
    }

    @CalledByNativeForTesting
    public int getButtonStyles() {
        return mShownDialogModel.get(ModalDialogProperties.BUTTON_STYLES);
    }

    @CalledByNativeForTesting
    public String[] getMessageParagraphs() {
        return mShownDialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS).stream()
                .map(String::valueOf)
                .toArray(String[]::new);
    }

    @CalledByNativeForTesting
    public void clickLinkInMessageParagraphs(int linkIndex) {
        List<CharSequence> paragraphs =
                mShownDialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS);
        int cumulativeLinkIndex = 0;
        for (CharSequence paragraph : paragraphs) {
            if (paragraph instanceof Spanned) {
                Spanned spannable = (Spanned) paragraph;
                ClickableSpan[] spans =
                        spannable.getSpans(0, spannable.length(), ClickableSpan.class);
                if (linkIndex < cumulativeLinkIndex + spans.length) {
                    spans[linkIndex - cumulativeLinkIndex].onClick(null);
                    return;
                }
                cumulativeLinkIndex += spans.length;
            }
        }
    }

    @CalledByNativeForTesting
    public Bitmap getTitleIcon() {
        Drawable icon = mShownDialogModel.get(ModalDialogProperties.TITLE_ICON);
        if (icon instanceof BitmapDrawable) {
            return ((BitmapDrawable) icon).getBitmap();
        }
        return null;
    }

    @CalledByNativeForTesting
    public void clickMenuItem(int index) {
        mShownDialogModel.get(ModalDialogProperties.MENU_ITEMS).get(index).getCallback().run();
    }

    @CalledByNativeForTesting
    public String[] getMenuItemTexts() {
        ArrayList<ModalDialogProperties.ModalDialogMenuItem> items =
                mShownDialogModel.get(ModalDialogProperties.MENU_ITEMS);
        if (items == null) {
            return new String[0];
        }
        return items.stream()
                .map(ModalDialogProperties.ModalDialogMenuItem::getText)
                .toArray(String[]::new);
    }

    @CalledByNativeForTesting
    public Bitmap[] getMenuItemIcons() {
        ArrayList<ModalDialogProperties.ModalDialogMenuItem> items =
                mShownDialogModel.get(ModalDialogProperties.MENU_ITEMS);
        if (items == null) {
            return new Bitmap[0];
        }
        return items.stream()
                .map(
                        item -> {
                            Drawable icon = item.getIcon();
                            if (icon instanceof BitmapDrawable) {
                                return ((BitmapDrawable) icon).getBitmap();
                            }
                            return null;
                        })
                .toArray(Bitmap[]::new);
    }

    public PropertyModel getShownDialogModel() {
        return mShownDialogModel;
    }
}
