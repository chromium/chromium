// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Handler;
import android.os.Looper;

import org.chromium.webengine.interfaces.IWebMessageReplyProxy;

/**
 * This class act as a proxy between webengine and the {@link WebMessageReplyProxy} in
 * weblayer.
 */
class WebMessageReplyProxyProxy extends IWebMessageReplyProxy.Stub {
    private Handler mHandler = new Handler(Looper.getMainLooper());

    private final WebMessageReplyProxy mWebMessageReplyProxy;

    WebMessageReplyProxyProxy(WebMessageReplyProxy webMessageReplyProxy) {
        mWebMessageReplyProxy = webMessageReplyProxy;
    }

    @Override
    public void postMessage(String message) {
        mHandler.post(() -> { mWebMessageReplyProxy.postMessage(new WebMessage(message)); });
    }
}
