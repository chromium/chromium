// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;
import android.webkit.ValueCallback;

import org.chromium.base.Callback;
import org.chromium.weblayer_private.interfaces.IExternalIntentInIncognitoCallbackClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Proxies calls to present the warning dialog gating external intent launches in incognito to
 * ExternalIntentInIncognitoCallbackClient.
 */
public final class ExternalIntentInIncognitoCallbackProxy {
    private IExternalIntentInIncognitoCallbackClient mClient;

    ExternalIntentInIncognitoCallbackProxy(IExternalIntentInIncognitoCallbackClient client) {
        setClient(client);
    }

    void setClient(IExternalIntentInIncognitoCallbackClient client) {
        mClient = client;
    }

    /*
     * Proxies onExternalIntentInIncognito() calls to the client.
     */
    void onExternalIntentInIncognito(Callback<Integer> onUserDecisionCallback)
            throws RemoteException {
        assert mClient != null;

        ValueCallback<Integer> onUserDecisionValueCallback = (Integer userDecision) -> {
            onUserDecisionCallback.onResult(userDecision);
        };

        mClient.onExternalIntentInIncognito(ObjectWrapper.wrap(onUserDecisionValueCallback));
    }
}
