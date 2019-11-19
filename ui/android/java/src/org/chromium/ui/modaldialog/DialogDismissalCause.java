// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({DialogDismissalCause.UNKNOWN, DialogDismissalCause.POSITIVE_BUTTON_CLICKED,
        DialogDismissalCause.NEGATIVE_BUTTON_CLICKED, DialogDismissalCause.ACTION_ON_CONTENT,
        DialogDismissalCause.DISMISSED_BY_NATIVE,
        DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE, DialogDismissalCause.TAB_SWITCHED,
        DialogDismissalCause.TAB_DESTROYED, DialogDismissalCause.ACTIVITY_DESTROYED,
        DialogDismissalCause.NOT_ATTACHED_TO_WINDOW})
@Retention(RetentionPolicy.SOURCE)
public @interface DialogDismissalCause {
    // Please do not remove or change the order of the existing values, and add new value at the end
    // of the enum. Dismissal causes that are fully controlled by clients (i.e. are not used inside
    // the dialog manager or the dialog presenters) are marked "Controlled by client" on comments.

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
    /** User clicks the navigate back button or touches the scrim outside the dialog. */
    int NAVIGATE_BACK_OR_TOUCH_OUTSIDE = 5;
    /** User switches away the tab associated with the dialog. */
    int TAB_SWITCHED = 6;
    /** The Tab associated with the dialog is destroyed. */
    int TAB_DESTROYED = 7;
    /** The activity associated with the dialog is destroyed. */
    int ACTIVITY_DESTROYED = 8;
    /** The content view of the activity associated with the dialog is not attached to window. */
    int NOT_ATTACHED_TO_WINDOW = 9;
}
