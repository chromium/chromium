// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IWebMessageCallbackClient;
import org.chromium.weblayer_private.interfaces.IWebMessageReplyProxy;

/**
 * WebMessageReplyProxyImpl is responsible for both sending and receiving WebMessages.
 */
@JNINamespace("weblayer")
public final class WebMessageReplyProxyImpl extends IWebMessageReplyProxy.Stub {
    private long mNativeWebMessageReplyProxyImpl;
    private final IWebMessageCallbackClient mClient;
    // Unique id (scoped to the call to Tab.registerWebMessageCallback()) for this proxy. This is
    // sent over AIDL.
    private final int mId;

    private WebMessageReplyProxyImpl(long nativeWebMessageReplyProxyImpl, int id,
            IWebMessageCallbackClient client, boolean isMainFrame, String sourceOrigin,
            PageImpl page) {
        mNativeWebMessageReplyProxyImpl = nativeWebMessageReplyProxyImpl;
        mClient = client;
        mId = id;
        try {
            client.onNewReplyProxy(this, mId, isMainFrame, sourceOrigin);
            if (WebLayerFactoryImpl.getClientMajorVersion() >= 99) {
                client.onSetPage(mId, page.getClientPage());
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private static WebMessageReplyProxyImpl create(long nativeWebMessageReplyProxyImpl, int id,
            IWebMessageCallbackClient client, boolean isMainFrame, String sourceOrigin,
            PageImpl page) {
        return new WebMessageReplyProxyImpl(
                nativeWebMessageReplyProxyImpl, id, client, isMainFrame, sourceOrigin, page);
    }

    @CalledByNative
    private void onActiveStateChanged() throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() >= 90) {
            mClient.onReplyProxyActiveStateChanged(mId);
        }
    }

    @CalledByNative
    private void onNativeDestroyed() throws RemoteException {
        mNativeWebMessageReplyProxyImpl = 0;
        mClient.onReplyProxyDestroyed(mId);
    }

    @CalledByNative
    private void onPostMessage(String message) throws RemoteException {
        mClient.onPostMessage(mId, message);
    }

    @Override
    public void postMessage(String message) {
        if (mNativeWebMessageReplyProxyImpl != 0) {
            WebMessageReplyProxyImplJni.get().postMessage(mNativeWebMessageReplyProxyImpl, message);
        }
    }

    @Override
    public boolean isActive() {
        // Client code checks for closed before calling this.
        assert mNativeWebMessageReplyProxyImpl != 0;
        return WebMessageReplyProxyImplJni.get().isActive(mNativeWebMessageReplyProxyImpl);
    }

    @NativeMethods
    interface Natives {
        void postMessage(long nativeWebMessageReplyProxyImpl, String message);
        boolean isActive(long nativeWebMessageReplyProxyImpl);
    }
}
