// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.view.SurfaceControlViewHost;
import android.view.WindowManager;

import androidx.annotation.Nullable;

import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegate;
import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegateClient;
import org.chromium.browserfragment.interfaces.ITabObserverDelegate;
import org.chromium.browserfragment.interfaces.ITabProxy;

/**
 * This class acts as a proxy between the embedding app's BrowserFragment and
 * the WebLayer implementation.
 */
class BrowserFragmentDelegate extends IBrowserFragmentDelegate.Stub {
    private static final String DEFAULT_PROFILE = "DefaultProfile";

    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private Context mContext;
    private WebLayer mWebLayer;

    // TODO(rayankans): Create an event handler instead of using the weblayer fragment directly.
    private BrowserFragment mFragment;

    private BrowserFragmentTabDelegate mTabDelegate;

    private IBrowserFragmentDelegateClient mClient;
    private SurfaceControlViewHost mSurfaceControlViewHost;

    BrowserFragmentDelegate(Context context, WebLayer webLayer) {
        mContext = context;
        mWebLayer = webLayer;
        mTabDelegate = new BrowserFragmentTabDelegate();

        mHandler.post(() -> {
            mFragment = (BrowserFragment) WebLayer.createBrowserFragment(DEFAULT_PROFILE);
            mFragment.ignoreViewModel();
        });
    }

    @Override
    public void setClient(IBrowserFragmentDelegateClient client) {
        mClient = client;
    }

    @Override
    public void attachViewHierarchy(IBinder hostToken) {
        mHandler.post(() -> attachViewHierarchyOnUi(hostToken));
    }

    private void attachViewHierarchyOnUi(IBinder hostToken) {
        WindowManager window = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);

        assert mSurfaceControlViewHost == null;
        mSurfaceControlViewHost =
                new SurfaceControlViewHost(mContext, window.getDefaultDisplay(), hostToken);

        mFragment.getBrowser().setSurfaceControlViewHost(mSurfaceControlViewHost);
        try {
            mClient.onSurfacePackageReady(mSurfaceControlViewHost.getSurfacePackage());
        } catch (RemoteException e) {
        }
    }

    @Override
    public void resizeView(int width, int height) {
        mHandler.post(() -> {
            if (mSurfaceControlViewHost != null) {
                mSurfaceControlViewHost.relayout(width, height);
            }
        });
    }

    @Override
    @Nullable
    public ITabProxy getActiveTab() {
        Tab activeTab = mTabDelegate.getActiveTab();
        if (activeTab != null) {
            return new TabProxy(activeTab);
        }
        return null;
    }

    @Override
    public void onAttach() {
        mHandler.post(() -> mFragment.onAttach(mContext));
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mHandler.post(() -> mFragment.onCreate(savedInstanceState, mTabDelegate));
    }

    @Override
    public void onDestroy() {
        mHandler.post(() -> mFragment.onDestroy());
    }

    @Override
    public void onDetach() {
        mHandler.post(() -> mFragment.onDetach());
    }

    @Override
    public void onStart() {
        mHandler.post(() -> {
            mFragment.onStart();
            try {
                mClient.onStarted();
            } catch (RemoteException e) {
            }
        });
    }

    @Override
    public void onStop() {
        mHandler.post(() -> mFragment.onStop());
    }

    @Override
    public void onResume() {
        mHandler.post(() -> mFragment.onResume());
    }

    @Override
    public void onPause() {
        mHandler.post(() -> mFragment.onPause());
    }

    @Override
    public void onCleared() {
        mHandler.post(() -> mSurfaceControlViewHost.release());
    }

    @Override
    public void setTabObserverDelegate(ITabObserverDelegate tabObserverDelegate) {
        mTabDelegate.setObserver(tabObserverDelegate);
    }
}