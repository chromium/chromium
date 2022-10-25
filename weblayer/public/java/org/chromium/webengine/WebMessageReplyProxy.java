// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.RemoteException;

import org.chromium.webengine.interfaces.IWebMessageReplyProxy;

/**
 * Used to post a message to a page. WebMessageReplyProxy is created when a page posts a message to
 * the JavaScript object that was created by way of {@link Tab#registerWebMessageCallback}.
 */
public class WebMessageReplyProxy {
    private final IWebMessageReplyProxy mWebMessageReplyProxy;

    WebMessageReplyProxy(IWebMessageReplyProxy webMessageReplyProxy) {
        mWebMessageReplyProxy = webMessageReplyProxy;
    }

    /**
     * Post a message back to the JavaScript object.
     *
     * @param message The message to post.
     */
    public void postMessage(String message) {
        try {
            mWebMessageReplyProxy.postMessage(message);
        } catch (RemoteException e) {
        }
    };
}
