// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IClientPage;
import org.chromium.weblayer_private.interfaces.IWebMessageReplyProxy;

interface IWebMessageCallbackClient {
  void onNewReplyProxy(in IWebMessageReplyProxy proxy,
                       in int proxyId,
                       in boolean isMainFrame,
                       in String sourceOrigin) = 0;
  void onPostMessage(in int proxyId, in String message) = 1;
  void onReplyProxyDestroyed(in int proxyId) = 2;

  // @since 90
  void onReplyProxyActiveStateChanged(in int proxyId) = 3;

  // @since 99
  void onSetPage(in int proxyId, IClientPage page) = 4;
}
