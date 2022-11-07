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

import org.chromium.webengine.interfaces.IFragmentParams;
import org.chromium.webengine.interfaces.ITabListObserverDelegate;
import org.chromium.webengine.interfaces.IWebFragmentDelegate;
import org.chromium.webengine.interfaces.IWebFragmentDelegateClient;
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * This class acts as a proxy between the embedding app's WebFragment and
 * the WebLayer implementation.
 */
class WebFragmentDelegate extends IWebFragmentDelegate.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private Context mContext;
    private WebLayer mWebLayer;
    private WebFragmentEventHandler mEventHandler;

    private WebFragmentTabListDelegate mTabListDelegate;

    private IWebFragmentDelegateClient mClient;
    private SurfaceControlViewHost mSurfaceControlViewHost;

    WebFragmentDelegate(Context context, WebLayer webLayer, IFragmentParams params) {
        mContext = context;
        mWebLayer = webLayer;
        mTabListDelegate = new WebFragmentTabListDelegate();

        WebFragmentCreateParams createParams = (new WebFragmentCreateParams.Builder())
                                                       .setProfileName(params.profileName)
                                                       .setPersistenceId(params.persistenceId)
                                                       .setIsIncognito(params.isIncognito)
                                                       .build();
        mHandler.post(() -> { mEventHandler = createWebFragmentEventHandler(createParams); });
    }

    private WebFragmentEventHandler createWebFragmentEventHandler(WebFragmentCreateParams params) {
        ThreadCheck.ensureOnUiThread();
        String profileName = Profile.sanitizeProfileName(params.getProfileName());
        boolean isIncognito = params.isIncognito() || "".equals(profileName);
        // Support for named incognito profiles was added in 87. Checking is done in
        // WebFragment, as this code should not trigger loading WebLayer.
        Bundle args = new Bundle();
        args.putString(BrowserFragmentArgs.PROFILE_NAME, profileName);
        if (params.getPersistenceId() != null) {
            args.putString(BrowserFragmentArgs.PERSISTENCE_ID, params.getPersistenceId());
        }
        args.putBoolean(BrowserFragmentArgs.IS_INCOGNITO, isIncognito);
        args.putBoolean(BrowserFragmentArgs.USE_VIEW_MODEL, params.getUseViewModel());

        return new WebFragmentEventHandler(args);
    }

    @Override
    public void setClient(IWebFragmentDelegateClient client) {
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

        mEventHandler.setSurfaceControlViewHost(mSurfaceControlViewHost);
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
                        ObjectWrapper.wrap(mEventHandler.getContentViewRenderView()));
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
            mEventHandler.onCreate(savedInstanceState, mTabListDelegate);

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
                mClient.onTabManagerReady(new TabManagerDelegate(mEventHandler.getBrowser()));
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
        mTabListDelegate.setObserver(browserObserverDelegate);
    }

    /**
     * Set the minimum surface size of this BrowserFragment instance.
     * Setting this avoids expensive surface resize for a fragment view resize that is within the
     * minimum size. The trade off is the additional memory and power needed for the larger
     * surface. For example, for a browser use case, it's likely worthwhile to set the minimum
     * surface size to the screen size to avoid surface resize when entering and exiting fullscreen.
     * It is safe to call this before Views are initialized.
     * Note Android does have a max size limit on Surfaces which applies here as well; this
     * generally should not be larger than the device screen size.
     * Note the surface size is increased to the layout size only if both the width and height are
     * no larger than the minimum surface size. No adjustment is made if the surface size is larger
     * than the minimum size in one dimension and smaller in the other dimension.
     */
    @Override
    public void setMinimumSurfaceSize(int width, int height) {
        mHandler.post(() -> mEventHandler.setMinimumSurfaceSize(width, height));
    }
}