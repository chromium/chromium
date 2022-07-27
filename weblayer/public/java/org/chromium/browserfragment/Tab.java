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

    Tab(ITabProxy tabProxy) {
        mTabProxy = tabProxy;
    }

    /**
     * Navigates this Tab to the given URI.
     *
     * @param uri The destination URI.
     */
    public void navigate(@NonNull String uri) {
        try {
            mTabProxy.navigate(uri);
        } catch (RemoteException e) {
        }
    }
}
