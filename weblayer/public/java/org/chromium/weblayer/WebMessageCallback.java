// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Receives messages from a JavaScript object that was created by calling {@link
 * Tab#registerWebMessageCallback().
 */
public abstract class WebMessageCallback {
    /**
     * Called when a message is received from the page.
     *
     * @param replyProxy An object that may be used to post a message back to the page.
     * @param message The message from the page.
     */
    public void onWebMessageReceived(
            @NonNull WebMessageReplyProxy replyProxy, @NonNull WebMessage message) {}
}
