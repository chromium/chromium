// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The model properties for a modal dialog. */
public class ModalDialogProperties {
    /** Interface that controls the actions on the modal dialog. */
    public interface Controller {
        /**
         * Handle click event of the buttons on the dialog.
         * @param model The dialog model that is associated with this click event.
         * @param buttonType The type of the button.
         */
        void onClick(PropertyModel model, @ButtonType int buttonType);

        /**
         * Handle dismiss event when the dialog is dismissed by actions on the dialog. Note that it
         * can be dangerous to the {@code dismissalCause} for business logic other than metrics
         * recording, unless the dismissal cause is fully controlled by the client (e.g. button
         * clicked), because the dismissal cause can be different values depending on modal dialog
         * type and mode of presentation (e.g. it could be unknown on VR but a specific value on
         * non-VR).
         * @param model The dialog model that is associated with this dismiss event.
         * @param dismissalCause The reason of the dialog being dismissed.
         * @see DialogDismissalCause
         */
        void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause);
    }

    @IntDef({
        ModalDialogProperties.ButtonType.POSITIVE,
        ModalDialogProperties.ButtonType.NEGATIVE,
        ModalDialogProperties.ButtonType.TITLE_ICON,
        ButtonType.POSITIVE_EPHEMERAL
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonType {
        int POSITIVE = 0;
        int NEGATIVE = 1;
        int TITLE_ICON = 2;
        int POSITIVE_EPHEMERAL = 3;
    }

    /**
     * Styles of the primary and negative button. Only one of them can be filled at the same time.
     */
    @IntDef({
        ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE,
        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
        ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_FILLED,
        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonStyles {
        int PRIMARY_OUTLINE_NEGATIVE_OUTLINE = 0;
        int PRIMARY_FILLED_NEGATIVE_OUTLINE = 1;
        int PRIMARY_OUTLINE_NEGATIVE_FILLED = 2;
        int PRIMARY_FILLED_NO_NEGATIVE = 3;
    }

    /**
     * Specifies a button as part of a button group. To employ this UI component, set an array of
     * `ModalDialogButtonSpec` objects as value for the property value for the property key
     * `BUTTON_GROUP_BUTTON_SPEC_LIST`.
     */
    public static class ModalDialogButtonSpec {
        private final @ButtonType int mButtonType;
        private final String mText;
        private final String mContentDescription;

        public ModalDialogButtonSpec(@ButtonType int buttonType, String buttonText) {
            this(buttonType, buttonText, buttonText);
        }

        public ModalDialogButtonSpec(
                @ButtonType int buttonType, String buttonText, String buttonContentDescription) {
            mButtonType = buttonType;
            mText = buttonText;
            mContentDescription = buttonContentDescription;
        }

        public int getButtonType() {
            return mButtonType;
        }

        public String getText() {
            return mText;
        }

        public String getContentDescription() {
            return mContentDescription;
        }
    }

    /** Styles of the dialog. Only one of them can be set at the same time. */
    @IntDef({
        DialogStyles.NORMAL,
        DialogStyles.FULLSCREEN_DIALOG,
        DialogStyles.FULLSCREEN_DARK_DIALOG,
        DialogStyles.DIALOG_WHEN_LARGE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DialogStyles {
        int NORMAL = 0;

        /** Default Fullscreen mode. */
        int FULLSCREEN_DIALOG = 1;

        /**
         * Fullscreen mode with dark status and navigation bar. This is only supported for Android
         * versions >= O. For versions < O, the FULLSCREEN_DIALOG style will be used.
         */
        int FULLSCREEN_DARK_DIALOG = 2;

        /** Fullscreen mode on phone, and dialog on large screen. */
        int DIALOG_WHEN_LARGE = 3;
    }

    /** The {@link Controller} that handles events on user actions. */
    public static final ReadableObjectPropertyKey<Controller> CONTROLLER =
            new ReadableObjectPropertyKey<>();

    /** The content description of the dialog for accessibility. */
    public static final ReadableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new ReadableObjectPropertyKey<>();

    /** The title of the dialog. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /** The maximum number of lines that the title can take. */
    public static final WritableIntPropertyKey TITLE_MAX_LINES = new WritableIntPropertyKey();

    /** The title icon of the dialog. */
    public static final WritableObjectPropertyKey<Drawable> TITLE_ICON =
            new WritableObjectPropertyKey<>();

    /** The message paragraph 1 of the dialog. */
    public static final WritableObjectPropertyKey<CharSequence> MESSAGE_PARAGRAPH_1 =
            new WritableObjectPropertyKey<>();

    /** The message paragraph 2 of the dialog. Shown below the paragraph 1 when both are set. */
    public static final WritableObjectPropertyKey<CharSequence> MESSAGE_PARAGRAPH_2 =
            new WritableObjectPropertyKey<>();

    /** The customized content view of the dialog. */
    public static final WritableObjectPropertyKey<View> CUSTOM_VIEW =
            new WritableObjectPropertyKey<>();

    /** The customized view replacing the button bar of the dialog. */
    public static final WritableObjectPropertyKey<View> CUSTOM_BUTTON_BAR_VIEW =
            new WritableObjectPropertyKey<>();

    /** The text on the positive button. */
    public static final WritableObjectPropertyKey<String> POSITIVE_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** Content description for the positive button. */
    public static final WritableObjectPropertyKey<String> POSITIVE_BUTTON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The enabled state on the positive button. */
    public static final WritableBooleanPropertyKey POSITIVE_BUTTON_DISABLED =
            new WritableBooleanPropertyKey();

    /** The text on the negative button. */
    public static final WritableObjectPropertyKey<String> NEGATIVE_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** Content description for the negative button. */
    public static final WritableObjectPropertyKey<String> NEGATIVE_BUTTON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** The enabled state on the negative button. */
    public static final WritableBooleanPropertyKey NEGATIVE_BUTTON_DISABLED =
            new WritableBooleanPropertyKey();

    /** The message on the dialog footer. */
    public static final WritableObjectPropertyKey<CharSequence> FOOTER_MESSAGE =
            new WritableObjectPropertyKey<>();

    /** Whether the dialog should be dismissed on user tapping the scrim. */
    public static final WritableBooleanPropertyKey CANCEL_ON_TOUCH_OUTSIDE =
            new WritableBooleanPropertyKey();

    /**
     * Whether button touch events should be filtered when buttons are obscured by another visible
     * window.
     */
    public static final ReadableBooleanPropertyKey FILTER_TOUCH_FOR_SECURITY =
            new ReadableBooleanPropertyKey();

    /**
     * Callback to be called when the modal dialog filters touch events because the buttons are
     * obscured by another window.
     */
    public static final ReadableObjectPropertyKey<Runnable> TOUCH_FILTERED_CALLBACK =
            new ReadableObjectPropertyKey<>();

    /** Configure a button group UI component. */
    public static final ReadableObjectPropertyKey<ModalDialogButtonSpec[]>
            BUTTON_GROUP_BUTTON_SPEC_LIST = new ReadableObjectPropertyKey<>();

    /** Whether the title is scrollable with the message. */
    public static final WritableBooleanPropertyKey TITLE_SCROLLABLE =
            new WritableBooleanPropertyKey();

    /**
     * Whether the custom view should be wrapped in a ScrollView. The custom view must not be a
     * ScrollView itself if this is set.
     */
    public static final WritableBooleanPropertyKey WRAP_CUSTOM_VIEW_IN_SCROLLABLE =
            new WritableBooleanPropertyKey();

    /** Whether the primary (positive) or negative button should be a filled button */
    public static final ReadableIntPropertyKey BUTTON_STYLES = new ReadableIntPropertyKey();

    /** Whether the dialog should follow {@link DialogStyles}. */
    public static final ReadableIntPropertyKey DIALOG_STYLES = new ReadableIntPropertyKey();

    /** Whether the dialog should be focused for accessibility. */
    public static final WritableBooleanPropertyKey FOCUS_DIALOG = new WritableBooleanPropertyKey();

    /**
     * The handler for back presses done on a {@ModalDialogType.APP}. By default, a back press
     * dismisses the dialog.
     */
    public static final WritableObjectPropertyKey<OnBackPressedCallback>
            APP_MODAL_DIALOG_BACK_PRESS_HANDLER = new WritableObjectPropertyKey();

    /**
     * Duration of initial tap protection period after dialog is displayed to user. During this
     * period, none of dialog buttons will respond to any click event; i.e.: {@link
     * Controller#onClick(PropertyModel, int)} won't be triggered until it is elapsed.
     */
    public static final WritableLongPropertyKey BUTTON_TAP_PROTECTION_PERIOD_MS =
            new WritableLongPropertyKey();

    /**
     * Whether a tab modal dialog should be canceled by the escape key. The value is always set to
     * true in {@link org.chromium.components.browser_ui.modaldialog.TabModalPresenter}.
     *
     * <p>Please note that app modal dialogs are canceled by the escape key without the need of
     * specifying any property.
     */
    public static final WritableBooleanPropertyKey TAB_MODAL_DIALOG_CANCEL_ON_ESCAPE =
            new WritableBooleanPropertyKey();

    /** The minimum horizontal margin used by the dialog relative to the window. */
    public static final WritableIntPropertyKey HORIZONTAL_MARGIN = new WritableIntPropertyKey();

    /** The minimum vertical margin used by the dialog relative to the window. */
    public static final WritableIntPropertyKey VERTICAL_MARGIN = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                CONTROLLER,
                CONTENT_DESCRIPTION,
                TITLE,
                TITLE_MAX_LINES,
                TITLE_ICON,
                MESSAGE_PARAGRAPH_1,
                MESSAGE_PARAGRAPH_2,
                CUSTOM_VIEW,
                CUSTOM_BUTTON_BAR_VIEW,
                POSITIVE_BUTTON_TEXT,
                POSITIVE_BUTTON_CONTENT_DESCRIPTION,
                POSITIVE_BUTTON_DISABLED,
                NEGATIVE_BUTTON_TEXT,
                NEGATIVE_BUTTON_CONTENT_DESCRIPTION,
                NEGATIVE_BUTTON_DISABLED,
                FOOTER_MESSAGE,
                CANCEL_ON_TOUCH_OUTSIDE,
                BUTTON_GROUP_BUTTON_SPEC_LIST,
                TOUCH_FILTERED_CALLBACK,
                FILTER_TOUCH_FOR_SECURITY,
                WRAP_CUSTOM_VIEW_IN_SCROLLABLE,
                TITLE_SCROLLABLE,
                BUTTON_STYLES,
                DIALOG_STYLES,
                FOCUS_DIALOG,
                APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                BUTTON_TAP_PROTECTION_PERIOD_MS,
                TAB_MODAL_DIALOG_CANCEL_ON_ESCAPE,
                HORIZONTAL_MARGIN,
                VERTICAL_MARGIN,
            };
}
