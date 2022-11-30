// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test_interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({AutofillEventType.VIEW_ENTERED, AutofillEventType.VIEW_EXITED,
        AutofillEventType.VALUE_CHANGED, AutofillEventType.COMMIT, AutofillEventType.CANCEL,
        AutofillEventType.SESSION_STARTED, AutofillEventType.QUERY_DONE})
@Retention(RetentionPolicy.SOURCE)
public @interface AutofillEventType {
    int VIEW_ENTERED = 1;
    int VIEW_EXITED = 2;
    int VALUE_CHANGED = 3;
    int COMMIT = 4;
    int CANCEL = 5;
    int SESSION_STARTED = 6;
    int QUERY_DONE = 7;
}
