// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

/**
 * An interface to receive message events from a JavaScript object of a Tab.
 *
 * This callback, the name of the JavaScript object, as well as the origins that are
 * targeted are set via {@link Tab#registerWebMessageCallback}.
 */
public abstract class WebMessageCallback {
    /**
     * A WebMessage was received.
     *
     * <b>WARNING</b>: It is possible to receive messages from an inactive page. This happens if a
     * message it sent around the same time the page is put in the back forward cache. As a result
     * of this, it is possible for this method to be called with a new proxy that is inactive.
     *
     * @param replyProxy An object that may be used to post a message back to the page.
     * @param message The message from the page.
     */
    public void onWebMessageReceived(WebMessageReplyProxy replyProxy, String message) {}

    /**
     * {@link WebMessageReplyProxy} was closed.
     *
     * This typically happens when navigating to another page. If the page goes into the back
     * forward cache, then message channels are left open (and this is not called). In that case
     * this method will be called either when the page is evicted from the cache or when the user
     * goes back to it and then navigates away and it doesn't go into the back forward cache again.
     *
     * @param replyProxy The proxy that has been closed.
     */
    public void onWebMessageReplyProxyClosed(WebMessageReplyProxy replyProxy) {}

    /**
     * The active state of the reply proxy has changed.
     * If a channel is active it is not closed and not in the back forward cache.
     *
     * @param proxy The proxy that changed state.
     */
    public void onWebMessageReplyProxyActiveStateChanged(WebMessageReplyProxy proxy) {}
}
