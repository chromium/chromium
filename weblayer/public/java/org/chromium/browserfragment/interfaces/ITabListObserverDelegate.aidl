// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import org.chromium.browserfragment.interfaces.ITabParams;

oneway interface ITabListObserverDelegate {
    void notifyActiveTabChanged(in ITabParams tabParams) = 1;
    void notifyTabAdded(in ITabParams tabParams) = 2;
    void notifyTabRemoved(in ITabParams tabParams) = 3;
    void notifyWillDestroyBrowserAndAllTabs() = 4;
}