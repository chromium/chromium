// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment;

import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.browserfragment.interfaces.ITabParams;
import org.chromium.browserfragment.interfaces.ITabProxy;

/**
 * Tab controls of the tab content.
 */
public class Tab {
    private ITabProxy mTabProxy;
    private TabNavigationController mTabNavigationController;

    private String mGuid;

    Tab(@NonNull ITabParams tabParams) {
        assert tabParams.tabProxy != null;
        assert tabParams.tabGuid != null;
        assert tabParams.navigationControllerProxy != null;

        mTabProxy = tabParams.tabProxy;
        mGuid = tabParams.tabGuid;
        mTabNavigationController = new TabNavigationController(tabParams.navigationControllerProxy);
    }

    public String getGuid() {
        return mGuid;
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

    @Override
    public int hashCode() {
        return mGuid.hashCode();
    }

    @Override
    public boolean equals(final Object obj) {
        if (obj instanceof Tab) {
            return this == obj || mGuid.equals(((Tab) obj).getGuid());
        }
        return false;
    }
}
