// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.IWebMessageReplyProxy;

oneway interface IWebMessageCallback {
    void onWebMessageReceived(IWebMessageReplyProxy replyProxy, String message) = 1;
    void onWebMessageReplyProxyClosed(IWebMessageReplyProxy replyProxy) = 2;
    void onWebMessageReplyProxyActiveStateChanged(IWebMessageReplyProxy proxy) = 3;
}
