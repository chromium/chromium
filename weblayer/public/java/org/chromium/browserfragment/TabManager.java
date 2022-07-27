// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.os.RemoteException;

import androidx.annotation.Nullable;

import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegate;
import org.chromium.browserfragment.interfaces.ITabProxy;

/**
 * Class for interaction with Browser Tabs.
 * Calls into BrowserFragmentDelegate which runs on the Binder thread, and requires
 * finished initialization from onCreate on UIThread.
 * Access only via ListenableFuture through BrowserFragment.
 */
public class TabManager {
    private IBrowserFragmentDelegate mDelegate;

    TabManager(IBrowserFragmentDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Returns the currently active Tab; null if no Tab is active.
     *
     * @return the currently active Tab.
     */
    @Nullable
    public Tab getActiveTab() {
        try {
            ITabProxy tabProxy = mDelegate.getActiveTab();
            if (tabProxy != null) {
                return new Tab(tabProxy);
            }
        } catch (RemoteException e) {
        }
        return null;
    }
}
