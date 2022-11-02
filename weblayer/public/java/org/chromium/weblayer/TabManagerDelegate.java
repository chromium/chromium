// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.ITabCallback;
import org.chromium.webengine.interfaces.ITabManagerDelegate;
import org.chromium.webengine.interfaces.ITabParams;

class TabManagerDelegate extends ITabManagerDelegate.Stub {
    private Handler mHandler = new Handler(Looper.getMainLooper());

    private Browser mBrowser;

    TabManagerDelegate(Browser browser) {
        mBrowser = browser;
    }

    @Override
    public void getActiveTab(ITabCallback tabCallback) {
        mHandler.post(() -> {
            Tab activeTab = mBrowser.getActiveTab();
            try {
                if (activeTab != null) {
                    ITabParams tabParams = TabParams.buildParcelable(activeTab);
                    tabCallback.onResult(tabParams);
                } else {
                    tabCallback.onResult(null);
                }
            } catch (RemoteException e) {
            }
        });
    }

    @Override
    public void createTab(ITabCallback callback) {
        mHandler.post(() -> {
            Tab newTab = mBrowser.createTab();
            try {
                callback.onResult(TabParams.buildParcelable(newTab));
            } catch (RemoteException e) {
            }
        });
    }

    @Override
    public void tryNavigateBack(IBooleanCallback callback) {
        mHandler.post(() -> {
            mBrowser.tryNavigateBack(didNavigate -> {
                try {
                    callback.onResult(didNavigate);
                } catch (RemoteException e) {
                }
            });
        });
    }
}