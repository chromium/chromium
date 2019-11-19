// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/**
 * Interface used by Tab to inform the client of changes. This largely duplicates the
 * TabCallback interface, but is a singleton to avoid unnecessary IPC.
 */
interface ITabClient {
  void visibleUrlChanged(in String url) = 0;

  void onNewTab(in int tabId, in int mode) = 1;

  void onRenderProcessGone() = 2;

  void onCloseTab() = 3;
}
