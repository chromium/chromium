// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({NavigationState.WAITING_RESPONSE, NavigationState.RECEIVING_BYTES,
        NavigationState.COMPLETE, NavigationState.FAILED})
@Retention(RetentionPolicy.SOURCE)
public @interface NavigationState {
    int WAITING_RESPONSE = 0;
    int RECEIVING_BYTES = 1;
    int COMPLETE = 2;
    int FAILED = 3;
}
