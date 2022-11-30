// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.weblayer_private.interfaces.GoogleAccountServiceType;
import org.chromium.weblayer_private.interfaces.IGoogleAccountsCallbackClient;

/**
 * Owns the C++ GoogleAccountsCallbackProxy which is responsible for forwarding all calls to this
 * class.
 */
@JNINamespace("weblayer")
public final class GoogleAccountsCallbackProxy {
    private long mNativeGoogleAccountsCallbackProxy;
    private IGoogleAccountsCallbackClient mClient;

    GoogleAccountsCallbackProxy(long tab, IGoogleAccountsCallbackClient client) {
        assert client != null;
        mClient = client;
        mNativeGoogleAccountsCallbackProxy =
                GoogleAccountsCallbackProxyJni.get().createGoogleAccountsCallbackProxy(this, tab);
    }

    public void setClient(IGoogleAccountsCallbackClient client) {
        assert client != null;
        mClient = client;
    }

    public void destroy() {
        GoogleAccountsCallbackProxyJni.get().deleteGoogleAccountsCallbackProxy(
                mNativeGoogleAccountsCallbackProxy);
        mNativeGoogleAccountsCallbackProxy = 0;
    }

    @CalledByNative
    private void onGoogleAccountsRequest(@GAIAServiceType int serviceType, String email,
            String continueUrl, boolean isSameTab) throws RemoteException {
        mClient.onGoogleAccountsRequest(
                implTypeToJavaType(serviceType), email, continueUrl, isSameTab);
    }

    @CalledByNative
    private String getGaiaId() throws RemoteException {
        return mClient.getGaiaId();
    }

    @GoogleAccountServiceType
    private static int implTypeToJavaType(@GAIAServiceType int type) {
        switch (type) {
            case GAIAServiceType.GAIA_SERVICE_TYPE_SIGNOUT:
                return GoogleAccountServiceType.SIGNOUT;
            case GAIAServiceType.GAIA_SERVICE_TYPE_ADDSESSION:
                return GoogleAccountServiceType.ADD_SESSION;
            // SIGNUP and INCOGNITO should not be possible currently, so pass through to DEFAULT.
            case GAIAServiceType.GAIA_SERVICE_TYPE_SIGNUP:
            case GAIAServiceType.GAIA_SERVICE_TYPE_INCOGNITO:
            case GAIAServiceType.GAIA_SERVICE_TYPE_DEFAULT:
                return GoogleAccountServiceType.DEFAULT;
        }
        assert false;
        return GoogleAccountServiceType.DEFAULT;
    }

    @NativeMethods
    interface Natives {
        long createGoogleAccountsCallbackProxy(GoogleAccountsCallbackProxy proxy, long tab);
        void deleteGoogleAccountsCallbackProxy(long proxy);
    }
}
