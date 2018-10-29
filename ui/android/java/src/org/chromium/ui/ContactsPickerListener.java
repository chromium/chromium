// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The callback used to indicate what action the user took in the picker.
 */
public interface ContactsPickerListener {
    /**
     * The action the user took in the picker.
     */
    @IntDef({ContactsPickerAction.CANCEL, ContactsPickerAction.CONTACTS_SELECTED,
            ContactsPickerAction.SELECT_ALL, ContactsPickerAction.UNDO_SELECT_ALL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContactsPickerAction {
        int CANCEL = 0;
        int CONTACTS_SELECTED = 1;
        int SELECT_ALL = 2;
        int UNDO_SELECT_ALL = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * Called when the user has selected an action. For possible actions see above.
     *
     * @param contacts The contacts that were selected (string contains json format).
     */
    void onContactsPickerUserAction(@ContactsPickerAction int action, String contacts);
}
