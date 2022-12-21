// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBooleanCallback;
import org.chromium.weblayer_private.interfaces.ICookieChangedCallbackClient;
import org.chromium.weblayer_private.interfaces.ICookieManager;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IStringCallback;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.List;

/**
 * Manages cookies for a WebLayer profile.
 */
class CookieManager {
    private final ICookieManager mImpl;

    static CookieManager create(IProfile profile) {
        try {
            return new CookieManager(profile.getCookieManager());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    // Constructor for test mocking.
    protected CookieManager() {
        mImpl = null;
    }

    private CookieManager(ICookieManager impl) {
        mImpl = impl;
    }

    /**
     * Sets a cookie for the given URL.
     *
     * @param uri the URI for which the cookie is to be set.
     * @param value the cookie string, using the format of the 'Set-Cookie' HTTP response header.
     * @param callback a callback to be executed when the cookie has been set, or on failure. Called
     *     with true if the cookie is set successfully, and false if the cookie is not set for
     *     security reasons.
     *
     * @throws IllegalArgumentException if the cookie is invalid.
     */
    public void setCookie(
            @NonNull Uri uri, @NonNull String value, @NonNull IBooleanCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.setCookie(uri.toString(), value, callback);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Gets the cookies for the given URL.
     *
     * @param uri the URI to get cookies for.
     * @param callback a callback to be executed with the cookie value in the format of the 'Cookie'
     *     HTTP request header. If there is no cookie, this will be called with an empty string.
     */
    public void getCookie(@NonNull Uri uri, @NonNull IStringCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.getCookie(uri.toString(), callback);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Gets the cookies for the given URL in the form of the 'Set-Cookie' HTTP response header.
     *
     * @param uri the URI to get cookies for.
     * @param callback a callback to be executed with a list of cookie strings in the format of the
     *     'Set-Cookie' HTTP response header.
     * @since 101
     */
    public void getResponseCookies(@NonNull Uri uri, @NonNull Callback<List<String>> callback) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 101) {
            throw new UnsupportedOperationException();
        }
        try {
            ValueCallback<List<String>> valueCallback = (List<String> result) -> {
                callback.onResult(result);
            };
            mImpl.getResponseCookies(uri.toString(), ObjectWrapper.wrap(valueCallback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Adds a callback to listen for changes to cookies for the given URI.
     *
     * @param uri the URI to listen to cookie changes on.
     * @param name the name of the cookie to listen for changes on. Can be null to listen for
     *     changes on all cookies.
     * @param callback a callback that will be notified on cookie changes.
     * @return a Runnable which will unregister the callback from listening to cookie changes.
     * @throws IllegalArgumentException if the cookie name is an empty string.
     */
    @NonNull
    public Runnable addCookieChangedCallback(
            @NonNull Uri uri, @Nullable String name, @NonNull CookieChangedCallback callback) {
        ThreadCheck.ensureOnUiThread();
        if (name != null && name.isEmpty()) {
            throw new IllegalArgumentException(
                    "Name cannot be empty, use null to listen for all cookie changes.");
        }
        try {
            return ObjectWrapper.unwrap(mImpl.addCookieChangedCallback(uri.toString(), name,
                                                new CookieChangedCallbackClientImpl(callback)),
                    Runnable.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private static final class CookieChangedCallbackClientImpl
            extends ICookieChangedCallbackClient.Stub {
        private final CookieChangedCallback mCallback;

        CookieChangedCallbackClientImpl(CookieChangedCallback callback) {
            mCallback = callback;
        }

        @Override
        public void onCookieChanged(String cookie, @CookieChangeCause int cause) {
            StrictModeWorkaround.apply();
            mCallback.onCookieChanged(cookie, cause);
        }
    }
}
