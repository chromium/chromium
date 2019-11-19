// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;
import android.webkit.ValueCallback;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Owns the c++ FullscreenCallbackProxy class, which is responsible for forwarding all
 * FullscreenDelegate delegate calls to this class, which in turn forwards to the
 * FullscreenCallbackClient.
 */
@JNINamespace("weblayer")
public final class FullscreenCallbackProxy {
    private long mNativeFullscreenCallbackProxy;
    private IFullscreenCallbackClient mClient;

    FullscreenCallbackProxy(long tab, IFullscreenCallbackClient client) {
        assert client != null;
        mClient = client;
        mNativeFullscreenCallbackProxy =
                FullscreenCallbackProxyJni.get().createFullscreenCallbackProxy(this, tab);
    }

    public void setClient(IFullscreenCallbackClient client) {
        assert client != null;
        mClient = client;
    }

    public void destroy() {
        FullscreenCallbackProxyJni.get().deleteFullscreenCallbackProxy(
                mNativeFullscreenCallbackProxy);
        mNativeFullscreenCallbackProxy = 0;
    }

    @CalledByNative
    private void enterFullscreen() throws RemoteException {
        ValueCallback<Void> exitFullscreenCallback = new ValueCallback<Void>() {
            @Override
            public void onReceiveValue(Void result) {
                if (mNativeFullscreenCallbackProxy == 0) {
                    throw new IllegalStateException("Called after destroy()");
                }
                FullscreenCallbackProxyJni.get().doExitFullscreen(
                        mNativeFullscreenCallbackProxy, FullscreenCallbackProxy.this);
            }
        };
        mClient.enterFullscreen(ObjectWrapper.wrap(exitFullscreenCallback));
    }

    @CalledByNative
    private void exitFullscreen() throws RemoteException {
        mClient.exitFullscreen();
    }

    @NativeMethods
    interface Natives {
        long createFullscreenCallbackProxy(FullscreenCallbackProxy proxy, long tab);
        void deleteFullscreenCallbackProxy(long proxy);
        void doExitFullscreen(long nativeFullscreenCallbackProxy, FullscreenCallbackProxy proxy);
    }
}
