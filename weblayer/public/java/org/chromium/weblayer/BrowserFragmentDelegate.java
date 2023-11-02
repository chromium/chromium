// Copyright 2022 The Chromium Authors
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
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;
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
    private BrowserFragmentEventHandler mEventHandler;

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
        mHandler.post(() -> { mEventHandler = createBrowserFragmentEventHandler(createParams); });
    }

    private BrowserFragmentEventHandler createBrowserFragmentEventHandler(
            BrowserFragmentCreateParams params) {
        ThreadCheck.ensureOnUiThread();
        String profileName = Profile.sanitizeProfileName(params.getProfileName());
        boolean isIncognito = params.isIncognito() || "".equals(profileName);
        // Support for named incognito profiles was added in 87. Checking is done in
        // BrowserFragment, as this code should not trigger loading WebLayer.
        Bundle args = new Bundle();
        args.putString(BrowserFragmentArgs.PROFILE_NAME, profileName);
        if (params.getPersistenceId() != null) {
            args.putString(BrowserFragmentArgs.PERSISTENCE_ID, params.getPersistenceId());
        }
        args.putBoolean(BrowserFragmentArgs.IS_INCOGNITO, isIncognito);
        args.putBoolean(BrowserFragmentArgs.USE_VIEW_MODEL, params.getUseViewModel());

        return new BrowserFragmentEventHandler(args);
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

        mEventHandler.getBrowser().setSurfaceControlViewHost(mSurfaceControlViewHost);
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
                        ObjectWrapper.wrap(mEventHandler.getBrowser().getContentViewRenderView()));
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
            Tab activeTab = mEventHandler.getBrowser().getActiveTab();
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
        mHandler.post(() -> mEventHandler.onAttach(mContext));
    }

    @Override
    public void onAttachWithContext(IObjectWrapper context) {
        mHandler.post(() -> mEventHandler.onAttach(ObjectWrapper.unwrap(context, Context.class)));
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mHandler.post(() -> {
            mEventHandler.onCreate(savedInstanceState, mBrowserDelegate);

            Profile profile = mEventHandler.getBrowser().getProfile();
            try {
                mClient.onCookieManagerReady(new CookieManagerDelegate(profile.getCookieManager()));
            } catch (RemoteException e) {
            }
        });
    }

    @Override
    public void onDestroy() {
        mHandler.post(() -> mEventHandler.onDestroy());
    }

    @Override
    public void onDetach() {
        mHandler.post(() -> {
            mEventHandler.onDetach();
            if (mSurfaceControlViewHost != null) {
                mSurfaceControlViewHost.release();
                mSurfaceControlViewHost = null;
            }
        });
    }

    @Override
    public void onStart() {
        mHandler.post(() -> {
            mEventHandler.onStart();

            // Retrieve the instance state.
            Bundle instanceState = new Bundle();
            mEventHandler.onSaveInstanceState(instanceState);

            try {
                mClient.onStarted(instanceState);
            } catch (RemoteException e) {
            }
        });
    }

    @Override
    public void onStop() {
        mHandler.post(() -> mEventHandler.onStop());
    }

    @Override
    public void onResume() {
        mHandler.post(() -> mEventHandler.onResume());
    }

    @Override
    public void onPause() {
        mHandler.post(() -> mEventHandler.onPause());
    }

    @Override
    public void setTabListObserverDelegate(ITabListObserverDelegate browserObserverDelegate) {
        mBrowserDelegate.setObserver(browserObserverDelegate);
    }

    @Override
    public void tryNavigateBack(IBooleanCallback callback) {
        mHandler.post(() -> {
            mEventHandler.getBrowser().tryNavigateBack(didNavigate -> {
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
            Tab newTab = mEventHandler.getBrowser().createTab();
            try {
                callback.onResult(TabParams.buildParcelable(newTab));
            } catch (RemoteException e) {
            }
        });
    }
}