// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;
import android.webkit.ValueCallback;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.IGoogleAccountAccessTokenFetcherClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Owns the C++ GoogleAccountAccessTokenFetcherProxy class, which is responsible for forwarding all
 * access token fetches made from C++ to this class, which in turn forwards to the
 * GoogleAccountAccessTokenFetcherClient.
 */
@JNINamespace("weblayer")
public final class GoogleAccountAccessTokenFetcherProxy {
    private long mNativeGoogleAccountAccessTokenFetcherProxy;
    private IGoogleAccountAccessTokenFetcherClient mClient;

    GoogleAccountAccessTokenFetcherProxy(ProfileImpl profile) {
        mNativeGoogleAccountAccessTokenFetcherProxy =
                GoogleAccountAccessTokenFetcherProxyJni.get()
                        .createGoogleAccountAccessTokenFetcherProxy(
                                this, profile.getNativeProfile());
    }

    public void setClient(IGoogleAccountAccessTokenFetcherClient client) {
        mClient = client;
    }

    public void destroy() {
        GoogleAccountAccessTokenFetcherProxyJni.get().deleteGoogleAccountAccessTokenFetcherProxy(
                mNativeGoogleAccountAccessTokenFetcherProxy);
        mNativeGoogleAccountAccessTokenFetcherProxy = 0;
    }

    /*
     * Proxies fetchAccessToken() calls to the client. Returns the empty string if the client is not
     * set.
     */
    public void fetchAccessToken(Set<String> scopes, ValueCallback<String> onTokenFetchedCallback)
            throws RemoteException {
        if (mClient == null) {
            onTokenFetchedCallback.onReceiveValue("");
            return;
        }

        mClient.fetchAccessToken(
                ObjectWrapper.wrap(scopes), ObjectWrapper.wrap(onTokenFetchedCallback));
    }

    /*
     * Proxies onAccessTokenIdentifiedAsInvalid() calls to the client if it is set.
     */
    public void onAccessTokenIdentifiedAsInvalid(Set<String> scopes, String token)
            throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 93) return;

        if (mClient == null) return;

        mClient.onAccessTokenIdentifiedAsInvalid(
                ObjectWrapper.wrap(scopes), ObjectWrapper.wrap(token));
    }

    /*
     * Proxies access token requests from C++ to the public fetchAccessToken() interface.
     */
    @CalledByNative
    private void fetchAccessToken(String[] scopes, long callbackId) throws RemoteException {
        ValueCallback<String> onTokenFetchedCallback = (String token) -> {
            GoogleAccountAccessTokenFetcherProxyJni.get().runOnTokenFetchedCallback(
                    callbackId, token);
        };

        fetchAccessToken(new HashSet<String>(Arrays.asList(scopes)), onTokenFetchedCallback);
    }

    /*
     * Proxies invalid access token notifications from C++ to the public interface.
     */
    @CalledByNative
    private void onAccessTokenIdentifiedAsInvalid(String[] scopes, String token)
            throws RemoteException {
        onAccessTokenIdentifiedAsInvalid(new HashSet<String>(Arrays.asList(scopes)), token);
    }

    @NativeMethods
    interface Natives {
        long createGoogleAccountAccessTokenFetcherProxy(
                GoogleAccountAccessTokenFetcherProxy proxy, long profile);
        void deleteGoogleAccountAccessTokenFetcherProxy(long proxy);
        void runOnTokenFetchedCallback(long callbackId, String token);
    }
}
