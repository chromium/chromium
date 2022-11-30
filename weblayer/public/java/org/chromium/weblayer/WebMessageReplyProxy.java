// Copyright 2020 The Chromium Authors
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
 *
 * Each {@link WebMessageReplyProxy} represents a single endpoint. Multiple messages sent to the
 * same endpoint use the same {@link WebMessageReplyProxy}.
 */
class WebMessageReplyProxy {
    private final IWebMessageReplyProxy mIReplyProxy;
    private final boolean mIsMainFrame;
    private final String mSourceOrigin;
    private boolean mIsClosed;
    // Added in 99.
    private Page mPage;

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

    /**
     * Returns whether the reply proxy has been closed. Any messages sent when the channel is closed
     * are dropped.
     */
    public boolean isClosed() {
        ThreadCheck.ensureOnUiThread();
        return mIsClosed;
    }

    void markClosed() {
        mIsClosed = true;
    }

    /**
     * Returns whether the channel is active. The channel is active if it is not closed and not in
     * the back forward cache.
     *
     * @return Whether the channel is active.
     */
    public boolean isActive() {
        ThreadCheck.ensureOnUiThread();
        if (mIsClosed) return false;
        if (WebLayer.getSupportedMajorVersionInternal() < 90) {
            return true;
        }
        try {
            return mIReplyProxy.isActive();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the Page associated with this proxy. For child frame, the Page of the main frame is
     * returned.
     *
     * @return The Page.
     *
     * @since 99
     */
    public Page getPage() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 99) {
            throw new UnsupportedOperationException();
        }
        return mPage;
    }

    // Only called in >= 99.
    void setPage(Page page) {
        mPage = page;
    }
}
