// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.JniRepeatingCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

@JNINamespace("ui")
@NullMarked
public class ModalDialogWrapper implements ModalDialogProperties.Controller {
    /** The native-side counterpart of this class */
    private final long mNativeDelegatePtr;

    private final @Nullable ModalDialogManager mModalDialogManager;

    private final PropertyModel.Builder mPropertyModelBuilder;

    private final @Nullable Context mContext;

    private final ArrayList<JniRepeatingCallback> mLinkCallbacks = new ArrayList<>();

    @CalledByNative
    private static ModalDialogWrapper create(long nativeDelegatePtr, WindowAndroid window) {
        return new ModalDialogWrapper(nativeDelegatePtr, window);
    }

    private ModalDialogWrapper(long nativeDelegatePtr, WindowAndroid window) {
        mNativeDelegatePtr = nativeDelegatePtr;
        mModalDialogManager = window.getModalDialogManager();
        mContext = window.getContext().get();
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
    private void withTitleIcon(Bitmap iconBitmap) {
        if (mContext == null) return;
        Drawable iconDrawable = new BitmapDrawable(mContext.getResources(), iconBitmap);
        mPropertyModelBuilder.with(ModalDialogProperties.TITLE_ICON, iconDrawable);
    }

    /**
     * Sets the message paragraphs building from text spans.
     *
     * @param paragraphSpans A 2D array of strings. The outer array represents paragraphs. Each
     *     inner array contains the text for each span within that paragraph.
     * @param paragraphCallbacks A 2D array of JniRepeatingCallback objects with the exact same
     *     dimensions as {@code paragraphSpans}. For each text span, this array holds either null
     *     for plain text, or a callback for a clickable link.
     */
    @CalledByNative
    private void withMessageParagraphs(
            String[][] paragraphSpans,
            JniRepeatingCallback<@Nullable Void>[][] paragraphCallbacks) {
        ArrayList<CharSequence> charSequences = new ArrayList<>();
        for (int i = 0; i < paragraphSpans.length; i++) {
            SpannableStringBuilder paragraphBuilder = new SpannableStringBuilder();
            String[] spans = paragraphSpans[i];
            JniRepeatingCallback<@Nullable Void>[] callbacks = assumeNonNull(paragraphCallbacks[i]);
            assert spans.length == callbacks.length
                    : "ModalDialogWrapper.withMessageParagraphs() received invalid inputs:"
                            + " paragraphSpans and paragraphCallbacks did not have the same"
                            + " dimension";

            for (int j = 0; j < spans.length; j++) {
                String text = spans[j];
                JniRepeatingCallback<@Nullable Void> callback = callbacks[j];

                if (callback == null) {
                    paragraphBuilder.append(text);
                } else {
                    mLinkCallbacks.add(callback);
                    SpannableString clickableText = new SpannableString(text);
                    clickableText.setSpan(
                            new ClickableSpan() {
                                @Override
                                public void onClick(View view) {
                                    callback.onResult(null);
                                }
                            },
                            0,
                            text.length(),
                            Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                    paragraphBuilder.append(clickableText);
                }
            }
            charSequences.add(paragraphBuilder);
        }

        mPropertyModelBuilder.with(ModalDialogProperties.MESSAGE_PARAGRAPHS, charSequences);
    }

    @CalledByNative
    private void withMenuItems(Bitmap[] icons, String[] texts) {
        if (mContext == null) return;
        assert icons.length == texts.length
                : "Menu item icons and texts must have the same length.";

        ArrayList<ModalDialogProperties.ModalDialogMenuItem> menuItems = new ArrayList<>();
        for (int i = 0; i < icons.length; i++) {
            final int index = i;
            Drawable iconDrawable = new BitmapDrawable(mContext.getResources(), icons[i]);
            Runnable callback =
                    () -> {
                        ModalDialogWrapperJni.get().menuItemClicked(mNativeDelegatePtr, index);
                    };
            menuItems.add(
                    new ModalDialogProperties.ModalDialogMenuItem(
                            iconDrawable, texts[i], callback));
        }
        mPropertyModelBuilder.with(ModalDialogProperties.MENU_ITEMS, menuItems);
    }

    @CalledByNative
    private void withCheckbox(String text, boolean isChecked) {
        mPropertyModelBuilder.with(ModalDialogProperties.CHECKBOX_TEXT, text);
        mPropertyModelBuilder.with(ModalDialogProperties.CHECKBOX_CHECKED, isChecked);
    }

    @Override
    public void onCheckboxChecked(boolean isChecked) {
        ModalDialogWrapperJni.get().checkboxToggled(mNativeDelegatePtr, isChecked);
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
        for (JniRepeatingCallback callback : mLinkCallbacks) {
            callback.destroy();
        }
        mLinkCallbacks.clear();
        ModalDialogWrapperJni.get().destroy(mNativeDelegatePtr);
    }

    @NativeMethods
    interface Natives {
        void positiveButtonClicked(long nativeModalDialogWrapper);

        void negativeButtonClicked(long nativeModalDialogWrapper);

        void checkboxToggled(long nativeModalDialogWrapper, boolean isChecked);

        void menuItemClicked(long nativeModalDialogWrapper, int index);

        void dismissed(long nativeModalDialogWrapper);

        void destroy(long nativeModalDialogWrapper);
    }
}
