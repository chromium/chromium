// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.Tab;

/**
 * NewTabCallback test helper. Primarily used to wait for a new tab to be created.
 */
public class NewTabCallbackImpl extends NewTabCallback {
    private final CallbackHelper mCallbackHelper = new CallbackHelper();

    @Override
    public void onNewTab(Tab tab, int mode) {
        mCallbackHelper.notifyCalled();
        tab.getBrowser().setActiveTab(tab);
    }

    @Override
    public void onCloseTab() {
        assert false;
    }

    public void waitForNewTab() {
        try {
            // waitForFirst() only handles a single call. If you need more convert from
            // waitForFirst().
            mCallbackHelper.waitForFirst();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
