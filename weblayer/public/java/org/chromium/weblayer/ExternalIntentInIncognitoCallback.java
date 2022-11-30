// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Allows the embedder to present a custom warning dialog gating external intent launch in
 * incognito mode rather than WebLayer's default dialog being used.
 * See {@link Tab#setExternalIntentInIncognitoCallback()}.
 * @since 93
 */
abstract class ExternalIntentInIncognitoCallback {
    /* Invoked when the user initiates a launch of an intent in incognito mode. The embedder's
     * implementation should present a modal dialog warning the user that they are leaving
     * incognito and asking if they wish to continue; it should then invoke onUserDecision() with
     * the user's decision once obtained (passing the value as an Integer wrapping an
     * @ExternalIntentInIncognitoUserDecision).
     * NOTE: The dialog presented *must* be modal, as confusion of state can otherwise occur. */
    public abstract void onExternalIntentInIncognito(
            @NonNull Callback<Integer> onUserDecisionCallback);
}
