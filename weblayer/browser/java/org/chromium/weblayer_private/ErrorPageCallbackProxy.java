// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;

/**
 * Owns the c++ ErrorPageCallbackProxy class, which is responsible for forwarding all
 * ErrorPageDelegate calls to this class, which in turn forwards to the
 * ErrorPageCallbackClient.
 */
@JNINamespace("weblayer")
public final class ErrorPageCallbackProxy {
    private long mNativeErrorPageCallbackProxy;
    private IErrorPageCallbackClient mClient;

    ErrorPageCallbackProxy(long tab, IErrorPageCallbackClient client) {
        assert client != null;
        mClient = client;
        mNativeErrorPageCallbackProxy =
                ErrorPageCallbackProxyJni.get().createErrorPageCallbackProxy(this, tab);
    }

    public void setClient(IErrorPageCallbackClient client) {
        assert client != null;
        mClient = client;
    }

    public void destroy() {
        ErrorPageCallbackProxyJni.get().deleteErrorPageCallbackProxy(mNativeErrorPageCallbackProxy);
        mNativeErrorPageCallbackProxy = 0;
    }

    @CalledByNative
    private boolean onBackToSafety() throws RemoteException {
        return mClient.onBackToSafety();
    }

    @NativeMethods
    interface Natives {
        long createErrorPageCallbackProxy(ErrorPageCallbackProxy proxy, long tab);
        void deleteErrorPageCallbackProxy(long proxy);
    }
}
