// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.browserfragment.interfaces.ITabProxy;

/**
 * Tab controls of the tab content.
 */
public class Tab {
    private ITabProxy mTabProxy;
    private TabNavigationController mTabNavigationController;

    Tab(ITabProxy tabProxy) {
        mTabProxy = tabProxy;

        try {
            mTabNavigationController =
                    new TabNavigationController(mTabProxy.getNavigationController());
        } catch (RemoteException e) {
            // TODO(swestphal): Raise exception.
        }
    }

    /**
     * Sets this tab to active.
     */
    public void setActive() {
        try {
            mTabProxy.setActive();
        } catch (RemoteException e) {
        }
    }

    /**
     * Returns the navigation controller for this Tab.
     *
     * @return The TabNavigationController.
     */
    @NonNull
    public TabNavigationController getNavigationController() {
        return mTabNavigationController;
    }
}
