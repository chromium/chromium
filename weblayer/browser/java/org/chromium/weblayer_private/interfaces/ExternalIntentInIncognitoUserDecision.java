// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({ExternalIntentInIncognitoUserDecision.ALLOW, ExternalIntentInIncognitoUserDecision.DENY})
@Retention(RetentionPolicy.SOURCE)
public @interface ExternalIntentInIncognitoUserDecision {
    int ALLOW = 0;
    int DENY = 1;
}
