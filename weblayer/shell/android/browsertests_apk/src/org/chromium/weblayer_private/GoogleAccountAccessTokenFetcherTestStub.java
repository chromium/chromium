// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.webkit.ValueCallback;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.weblayer_private.interfaces.IGoogleAccountAccessTokenFetcherClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/**
 * Implementation of IGoogleAccountAccessTokenFetcherClient that saves requests made from the
 * WebLayer implementation for introspection by tests and/or responses directed by tests.
 */
@JNINamespace("weblayer")
public class GoogleAccountAccessTokenFetcherTestStub
        extends IGoogleAccountAccessTokenFetcherClient.Stub {
    private HashMap<Integer, ValueCallback<String>> mOutstandingRequests =
            new HashMap<Integer, ValueCallback<String>>();
    private int mMostRecentRequestId;
    private Set<String> mMostRecentScopes;
    private Set<String> mScopesForMostRecentInvalidToken = new HashSet<String>();
    private String mMostRecentInvalidToken = "";

    @Override
    public void fetchAccessToken(
            IObjectWrapper scopesWrapper, IObjectWrapper onTokenFetchedWrapper) {
        Set<String> scopes = ObjectWrapper.unwrap(scopesWrapper, Set.class);
        ValueCallback<String> valueCallback =
                ObjectWrapper.unwrap(onTokenFetchedWrapper, ValueCallback.class);

        mMostRecentScopes = scopes;
        mMostRecentRequestId++;
        mOutstandingRequests.put(mMostRecentRequestId, valueCallback);
    }

    @Override
    public void onAccessTokenIdentifiedAsInvalid(
            IObjectWrapper scopesWrapper, IObjectWrapper tokenWrapper) {
        Set<String> scopes = ObjectWrapper.unwrap(scopesWrapper, Set.class);
        String token = ObjectWrapper.unwrap(tokenWrapper, String.class);

        mScopesForMostRecentInvalidToken = scopes;
        mMostRecentInvalidToken = token;
    }

    @CalledByNative
    int getMostRecentRequestId() {
        return mMostRecentRequestId;
    }

    @CalledByNative
    String[] getMostRecentRequestScopes() {
        return mMostRecentScopes.toArray(new String[0]);
    }

    @CalledByNative
    int getNumOutstandingRequests() {
        return mOutstandingRequests.size();
    }

    @CalledByNative
    String[] getScopesForMostRecentInvalidToken() {
        return mScopesForMostRecentInvalidToken.toArray(new String[0]);
    }

    @CalledByNative
    String getMostRecentInvalidToken() {
        return mMostRecentInvalidToken;
    }

    @CalledByNative
    public void respondWithTokenForRequest(int requestId, String token) {
        ValueCallback<String> callback = mOutstandingRequests.get(requestId);
        assert callback != null;
        mOutstandingRequests.remove(requestId);

        callback.onReceiveValue(token);
    }
}
