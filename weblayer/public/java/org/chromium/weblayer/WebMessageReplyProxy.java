// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IWebMessageReplyProxy;

/**
 * Used to post a message to a page. WebMessageReplyProxy is created when a page posts a message to
 * the JavaScript object that was created by way of {@link Tab#registerWebMessageCallback}.
 */
public class WebMessageReplyProxy {
    private final IWebMessageReplyProxy mIReplyProxy;
    private final boolean mIsMainFrame;
    private final String mSourceOrigin;

    // Constructor for test mocking.
    protected WebMessageReplyProxy() {
        this(null, false, "");
    }

    WebMessageReplyProxy(
            IWebMessageReplyProxy iReplyProxy, boolean isMainFrame, String sourceOrigin) {
        mIReplyProxy = iReplyProxy;
        mIsMainFrame = isMainFrame;
        mSourceOrigin = sourceOrigin;
    }

    /**
     * Returns true if the connection is to the main frame.
     *
     * @return True if the connection is to the main frame.
     */
    public boolean isMainFrame() {
        ThreadCheck.ensureOnUiThread();
        return mIsMainFrame;
    }

    /**
     * Returns the origin of the page.
     *
     * @return the origin of the page.
     */
    public @NonNull String getSourceOrigin() {
        ThreadCheck.ensureOnUiThread();
        return mSourceOrigin;
    }

    /**
     * Posts a message to the page.
     *
     * @param message The message to send to the page.
     */
    public void postMessage(@NonNull WebMessage message) {
        ThreadCheck.ensureOnUiThread();
        try {
            mIReplyProxy.postMessage(message.getContents());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}
