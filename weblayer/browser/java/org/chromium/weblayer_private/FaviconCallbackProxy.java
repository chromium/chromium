// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.graphics.Bitmap;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.IFaviconFetcher;
import org.chromium.weblayer_private.interfaces.IFaviconFetcherClient;

/**
 * Owns the c++ ErrorPageCallbackProxy class, which is responsible for forwarding all
 * ErrorPageDelegate calls to this class, which in turn forwards to the
 * ErrorPageCallbackClient.
 */
@JNINamespace("weblayer")
public final class FaviconCallbackProxy extends IFaviconFetcher.Stub {
    private TabImpl mTab;
    private long mNativeFaviconCallbackProxy;
    private IFaviconFetcherClient mClient;

    FaviconCallbackProxy(TabImpl tab, long nativeTab, IFaviconFetcherClient client) {
        assert client != null;
        mTab = tab;
        mClient = client;
        mNativeFaviconCallbackProxy =
                FaviconCallbackProxyJni.get().createFaviconCallbackProxy(this, nativeTab);
    }

    @Override
    public void destroy() {
        // As Tab implicitly destroys this, and the embedder is allowed to destroy this, allow
        // destroy() to be called multiple times.
        if (mNativeFaviconCallbackProxy == 0) {
            return;
        }
        mTab.removeFaviconCallbackProxy(this);
        try {
            mClient.onDestroyed();
        } catch (RemoteException e) {
            throw new AndroidRuntimeException(e);
        }
        FaviconCallbackProxyJni.get().deleteFaviconCallbackProxy(mNativeFaviconCallbackProxy);
        mNativeFaviconCallbackProxy = 0;
        mClient = null;
    }

    @CalledByNative
    private void onFaviconChanged(Bitmap bitmap) throws RemoteException {
        mTab.onFaviconChanged(bitmap);
        mClient.onFaviconChanged(bitmap);
    }

    @NativeMethods
    interface Natives {
        long createFaviconCallbackProxy(FaviconCallbackProxy proxy, long tab);
        void deleteFaviconCallbackProxy(long proxy);
    }
}
