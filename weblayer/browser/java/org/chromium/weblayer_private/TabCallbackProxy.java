// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Owns the C++ TabCallbackProxy class, which is responsible for forwarding all
 * BrowserObserver calls to this class, which in turn forwards to the TabClient.
 * To avoid unnecessary IPC only one TabCallbackProxy is created per Tab.
 */
@JNINamespace("weblayer")
public final class TabCallbackProxy {
    private long mNativeTabCallbackProxy;
    private ITabClient mClient;

    TabCallbackProxy(long tab, ITabClient client) {
        mClient = client;
        mNativeTabCallbackProxy = TabCallbackProxyJni.get().createTabCallbackProxy(this, tab);
    }

    public void destroy() {
        TabCallbackProxyJni.get().deleteTabCallbackProxy(mNativeTabCallbackProxy);
        mNativeTabCallbackProxy = 0;
    }

    @CalledByNative
    private void visibleUriChanged(String string) throws RemoteException {
        mClient.visibleUriChanged(string);
    }

    @CalledByNative
    private void onRenderProcessGone() throws RemoteException {
        mClient.onRenderProcessGone();
    }

    @CalledByNative
    private void onTitleUpdated(String title) throws RemoteException {
        mClient.onTitleUpdated(ObjectWrapper.wrap(title));
    }

    @NativeMethods
    interface Natives {
        long createTabCallbackProxy(TabCallbackProxy proxy, long tab);
        void deleteTabCallbackProxy(long proxy);
    }
}
