// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({ExternalIntentInIncognitoUserDecision.ALLOW, ExternalIntentInIncognitoUserDecision.DENY})
@Retention(RetentionPolicy.SOURCE)
@interface ExternalIntentInIncognitoUserDecision {
    int ALLOW =
            org.chromium.weblayer_private.interfaces.ExternalIntentInIncognitoUserDecision.ALLOW;
    int DENY = org.chromium.weblayer_private.interfaces.ExternalIntentInIncognitoUserDecision.DENY;
}
