// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import org.chromium.browserfragment.interfaces.ITabProxy;

oneway interface ITabObserverDelegate {
    void notifyActiveTabChanged(in ITabProxy activeTab) = 1;
    void notifyTabAdded(in ITabProxy tab) = 2;
    void notifyTabRemoved(in ITabProxy tab) = 3;
    void notifyWillDestroyBrowserAndAllTabs() = 4;
}