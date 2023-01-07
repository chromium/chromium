// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Receives messages from a JavaScript object that was created by calling {@link
 * Tab#registerWebMessageCallback().
 */
abstract class WebMessageCallback {
    /**
     * Called when a message is received from the page.
     *
     * Each {@link WebMessageReplyProxy} represents a single endpoint. Multiple messages sent to the
     * same endpoint use the same {@link WebMessageReplyProxy}.
     *
     * <b>WARNING</b>: it is possible to receive messages from an inactive page. This happens if a
     * message it sent around the same time the page is put in the back forward cache. As a result
     * of this, it is possible for this method to be called with a new proxy that is inactive.
     *
     * @param replyProxy An object that may be used to post a message back to the page.
     * @param message The message from the page.
     */
    public void onWebMessageReceived(
            @NonNull WebMessageReplyProxy replyProxy, @NonNull WebMessage message) {}

    /**
     * Called when a {@link WebMessageReplyProxy} is closed. This typically happens when navigating
     * to another page. If the page goes into the back forward cache, then message channels are left
     * open (and this is not called). In that case this method will be called either when the page
     * is evicted from the cache or when the user goes back to it and then navigates away and it
     * doesn't go into the back forward cache again.
     *
     * @param replyProxy The proxy that has been closed.
     */
    public void onWebMessageReplyProxyClosed(@NonNull WebMessageReplyProxy replyProxy) {}

    /**
     * Called when the active state of a proxy changes.
     *
     * @param proxy The proxy whose active state changed.
     *
     * @since 90
     */
    public void onWebMessageReplyProxyActiveStateChanged(@NonNull WebMessageReplyProxy proxy) {}
}
