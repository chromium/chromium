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

import org.chromium.browserfragment.interfaces.IBooleanCallback;
import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegate;
import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegateClient;
import org.chromium.browserfragment.interfaces.IFragmentParams;
import org.chromium.browserfragment.interfaces.ITabCallback;
import org.chromium.browserfragment.interfaces.ITabListObserverDelegate;
import org.chromium.browserfragment.interfaces.ITabParams;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * This class acts as a proxy between the embedding app's BrowserFragment and
 * the WebLayer implementation.
 */
class BrowserFragmentDelegate extends IBrowserFragmentDelegate.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private Context mContext;
    private WebLayer mWebLayer;

    // TODO(rayankans): Create an event handler instead of using the weblayer fragment directly.
    private BrowserFragment mFragment;

    private BrowserFragmentTabListDelegate mBrowserDelegate;

    private IBrowserFragmentDelegateClient mClient;
    private SurfaceControlViewHost mSurfaceControlViewHost;

    BrowserFragmentDelegate(Context context, WebLayer webLayer, IFragmentParams params) {
        mContext = context;
        mWebLayer = webLayer;
        mBrowserDelegate = new BrowserFragmentTabListDelegate();

        BrowserFragmentCreateParams createParams = (new BrowserFragmentCreateParams.Builder())
                                                           .setProfileName(params.profileName)
                                                           .setPersistenceId(params.persistenceId)
                                                           .setIsIncognito(params.isIncognito)
                                                           .build();
        mHandler.post(() -> {
            mFragment = (BrowserFragment) WebLayer.createBrowserFragmentWithParams(createParams);
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
    public void retrieveContentViewRenderView() {
        mHandler.post(() -> {
            try {
                mClient.onContentViewRenderViewReady(
                        ObjectWrapper.wrap(mFragment.getBrowser().getContentViewRenderView()));
            } catch (RemoteException e) {
            }
        });
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
    public void getActiveTab(ITabCallback tabCallback) {
        mHandler.post(() -> {
            Tab activeTab = mFragment.getBrowser().getActiveTab();
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
    public void onAttach() {
        mHandler.post(() -> mFragment.onAttach(mContext));
    }

    @Override
    public void onAttachWithContext(IObjectWrapper context) {
        mHandler.post(() -> mFragment.onAttach(ObjectWrapper.unwrap(context, Context.class)));
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mHandler.post(() -> mFragment.onCreate(savedInstanceState, mBrowserDelegate));
    }

    @Override
    public void onDestroy() {
        mHandler.post(() -> mFragment.onDestroy(/* force= */ true));
    }

    @Override
    public void onDetach() {
        mHandler.post(() -> {
            mFragment.onDetach();
            if (mSurfaceControlViewHost != null) {
                mSurfaceControlViewHost.release();
                mSurfaceControlViewHost = null;
            }
        });
    }

    @Override
    public void onStart() {
        mHandler.post(() -> {
            mFragment.onStart();

            // Retrieve the instance state.
            Bundle instanceState = new Bundle();
            mFragment.onSaveInstanceState(instanceState);

            try {
                mClient.onStarted(instanceState);
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
    public void setTabListObserverDelegate(ITabListObserverDelegate browserObserverDelegate) {
        mBrowserDelegate.setObserver(browserObserverDelegate);
    }

    @Override
    public void tryNavigateBack(IBooleanCallback callback) {
        mHandler.post(() -> {
            mFragment.getBrowser().tryNavigateBack(didNavigate -> {
                try {
                    callback.onResult(didNavigate);
                } catch (RemoteException e) {
                }
            });
        });
    }

    @Override
    public void createTab(ITabCallback callback) {
        mHandler.post(() -> {
            Tab newTab = mFragment.getBrowser().createTab();
            try {
                callback.onResult(TabParams.buildParcelable(newTab));
            } catch (RemoteException e) {
            }
        });
    }
}