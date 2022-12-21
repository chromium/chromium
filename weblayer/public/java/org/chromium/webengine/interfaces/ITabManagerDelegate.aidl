// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.ITabProxy;
import org.chromium.webengine.interfaces.ITabCallback;
import org.chromium.webengine.interfaces.ITabListObserverDelegate;

oneway interface ITabManagerDelegate {

    void setTabListObserverDelegate(ITabListObserverDelegate tabListObserverDelegate) = 1;
    void notifyInitialTabs() = 2;

    void getActiveTab(ITabCallback callback) = 3;
    void createTab(ITabCallback callback) = 4;
}