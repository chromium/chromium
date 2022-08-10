// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Handler;
import android.os.Looper;

import org.chromium.browserfragment.interfaces.ITabProxy;

/**
 * This class acts as a proxy between a Tab object in the embedding app
 * and the Tab implementation in WebLayer.
 */
class TabProxy extends ITabProxy.Stub {
    private Handler mHandler = new Handler(Looper.getMainLooper());

    private int mTabId;
    private String mGuid;

    TabProxy(Tab tab) {
        mTabId = tab.getId();
        mGuid = tab.getGuid();
    }

    private Tab getTab() {
        return Tab.getTabById(mTabId);
    }

    @Override
    public void setActive() {
        mHandler.post(() -> {
            Tab tab = getTab();
            tab.getBrowser().setActiveTab(tab);
        });
    }
}