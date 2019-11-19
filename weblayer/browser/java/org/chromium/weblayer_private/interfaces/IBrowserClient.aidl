// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.ITab;

interface IBrowserClient {
  void onActiveTabChanged(in int activeTabId) = 0;
  void onTabAdded(in ITab tab) = 1;
  void onTabRemoved(in int tabId) = 2;
}
