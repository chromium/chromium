// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
    DialogDismissalCause.UNKNOWN,
    DialogDismissalCause.POSITIVE_BUTTON_CLICKED,
    DialogDismissalCause.NEGATIVE_BUTTON_CLICKED,
    DialogDismissalCause.ACTION_ON_CONTENT,
    DialogDismissalCause.DISMISSED_BY_NATIVE,
    DialogDismissalCause.NAVIGATE_BACK,
    DialogDismissalCause.TOUCH_OUTSIDE,
    DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE,
    DialogDismissalCause.TAB_SWITCHED,
    DialogDismissalCause.TAB_DESTROYED,
    DialogDismissalCause.ACTIVITY_DESTROYED,
    DialogDismissalCause.NOT_ATTACHED_TO_WINDOW,
    DialogDismissalCause.NAVIGATE,
    DialogDismissalCause.WEB_CONTENTS_DESTROYED,
    DialogDismissalCause.DIALOG_INTERACTION_DEFERRED,
    DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED,
    DialogDismissalCause.ACTION_ON_DIALOG_NOT_POSSIBLE,
    DialogDismissalCause.CLIENT_TIMEOUT
})
@Retention(RetentionPolicy.SOURCE)
public @interface DialogDismissalCause {
    // Dismissal causes that are fully controlled by clients (i.e. are not used inside the
    // dialog manager or the dialog presenters) are marked "Controlled by client" on comments.

    /** No specified reason for the dialog dismissal. */
    int UNKNOWN = 0;

    /** Controlled by client: Positive button (e.g. OK button) is clicked by the user. */
    int POSITIVE_BUTTON_CLICKED = 1;

    /** Controlled by client: Negative button (e.g. Cancel button) is clicked by the user. */
    int NEGATIVE_BUTTON_CLICKED = 2;

    /** Controlled by client: Action taken on the dialog content triggers the dialog dismissal. */
    int ACTION_ON_CONTENT = 3;

    /** Controlled by client: Dialog is dismissed by native c++ objects. */
    int DISMISSED_BY_NATIVE = 4;

    /** User clicks the navigate back button. Tab Modal dialog only. */
    int NAVIGATE_BACK = 5;

    /** User touches the scrim outside the dialog. Tab Modal dialog only. */
    int TOUCH_OUTSIDE = 6;

    /**
     * User either clicks the navigate back button or touches the scrim outside the dialog. App
     * Modal dialog only.
     */
    int NAVIGATE_BACK_OR_TOUCH_OUTSIDE = 7;

    /** User switches away the tab associated with the dialog. */
    int TAB_SWITCHED = 8;

    /** The Tab associated with the dialog is destroyed. */
    int TAB_DESTROYED = 9;

    /** The activity associated with the dialog is destroyed. */
    int ACTIVITY_DESTROYED = 10;

    /** The content view of the activity associated with the dialog is not attached to window. */
    int NOT_ATTACHED_TO_WINDOW = 11;

    /** User has navigated, e.g. by typing a URL in the location bar. */
    int NAVIGATE = 12;

    /** Controlled by client: The web contents associated with the dialog is destroyed. */
    int WEB_CONTENTS_DESTROYED = 13;

    /**
     * Controlled by client: The dialog interaction is deferred due to user engaging with other UI
     * surfaces outside of dialog or they clicked on a content of the dialog that allows to defer.
     *
     * <p>Note that deferred would indicate that the dialog would be shown again later on unless the
     * user has completed the interaction successfully.
     */
    int DIALOG_INTERACTION_DEFERRED = 14;

    /**
     * Controlled by client: Action taken on the dialog content that the client validates and
     * triggers dismissal, if satisfied.
     */
    int ACTION_ON_DIALOG_COMPLETED = 15;

    /**
     * Controlled by client: The dialog was dismissed because it's not possible for client to
     * validate after the action was taken on the dialog content.
     */
    int ACTION_ON_DIALOG_NOT_POSSIBLE = 16;

    /** Controlled by client: The dialog was automatically dismissed after a timeout. */
    int CLIENT_TIMEOUT = 17;
}
