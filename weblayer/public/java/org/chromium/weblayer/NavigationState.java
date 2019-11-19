// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({NavigationState.WAITING_RESPONSE, NavigationState.RECEIVING_BYTES,
        NavigationState.COMPLETE, NavigationState.FAILED})
@Retention(RetentionPolicy.SOURCE)
public @interface NavigationState {
    int WAITING_RESPONSE =
            org.chromium.weblayer_private.interfaces.NavigationState.WAITING_RESPONSE;
    int RECEIVING_BYTES = org.chromium.weblayer_private.interfaces.NavigationState.RECEIVING_BYTES;
    int COMPLETE = org.chromium.weblayer_private.interfaces.NavigationState.COMPLETE;
    int FAILED = org.chromium.weblayer_private.interfaces.NavigationState.FAILED;
}
