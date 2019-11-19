// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;

/**
 * Owns the c++ DownloadCallbackProxy class, which is responsible for forwarding all
 * DownloadDelegate delegate calls to this class, which in turn forwards to the
 * DownloadCallbackClient.
 */
@JNINamespace("weblayer")
public final class DownloadCallbackProxy {
    private long mNativeDownloadCallbackProxy;
    private IDownloadCallbackClient mClient;

    DownloadCallbackProxy(long tab, IDownloadCallbackClient client) {
        assert client != null;
        mClient = client;
        mNativeDownloadCallbackProxy =
                DownloadCallbackProxyJni.get().createDownloadCallbackProxy(this, tab);
    }

    public void setClient(IDownloadCallbackClient client) {
        assert client != null;
        mClient = client;
    }

    public void destroy() {
        DownloadCallbackProxyJni.get().deleteDownloadCallbackProxy(mNativeDownloadCallbackProxy);
        mNativeDownloadCallbackProxy = 0;
    }

    @CalledByNative
    private void downloadRequested(String url, String userAgent, String contentDisposition,
            String mimetype, long contentLength) throws RemoteException {
        mClient.downloadRequested(url, userAgent, contentDisposition, mimetype, contentLength);
    }

    @NativeMethods
    interface Natives {
        long createDownloadCallbackProxy(DownloadCallbackProxy proxy, long tab);
        void deleteDownloadCallbackProxy(long proxy);
    }
}
