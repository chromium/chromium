// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.webengine.interfaces.IBooleanCallback;
import org.chromium.webengine.interfaces.IWebEngineDelegate;
import org.chromium.webengine.interfaces.IWebEngineDelegateClient;
import org.chromium.webengine.interfaces.IWebEngineParams;
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;

import java.util.ArrayList;

/**
 * Class to delegate between a webengine.WebEngine and its weblayer.Browser counter part.
 */
class WebEngineDelegate extends IWebEngineDelegate.Stub {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private Browser mBrowser;

    WebEngineDelegate(Browser browser) {
        mBrowser = browser;
    }

    static void create(Context context, WebLayer webLayer, IWebEngineParams params,
            IWebEngineDelegateClient client) {
        new Handler(Looper.getMainLooper()).post(() -> {
            Browser browser = new Browser(webLayer.createBrowser(context, bundleParams(params)));

            WebFragmentEventsDelegate fragmentEventsDelegate =
                    new WebFragmentEventsDelegate(context, browser.connectFragment());

            CookieManagerDelegate cookieManagerDelegate =
                    new CookieManagerDelegate(browser.getProfile().getCookieManager());
            TabManagerDelegate tabManagerDelegate = new TabManagerDelegate(browser);

            WebEngineDelegate webEngineDelegate = new WebEngineDelegate(browser);

            browser.registerTabInitializationCallback(new TabInitializationCallback() {
                @Override
                public void onTabInitializationCompleted() {
                    new Handler(Looper.getMainLooper()).post(() -> {
                        try {
                            client.onDelegatesReady(webEngineDelegate, fragmentEventsDelegate,
                                    tabManagerDelegate, cookieManagerDelegate);
                        } catch (RemoteException e) {
                            throw new RuntimeException("Failed to initialize WebEngineDelegate", e);
                        }
                    });
                }
            });

            browser.initializeState();
        });
    }

    private static Bundle bundleParams(IWebEngineParams params) {
        String profileName = Profile.sanitizeProfileName(params.profileName);
        boolean isIncognito = params.isIncognito || "".equals(profileName);
        boolean isExternalIntentsEnabled = params.isExternalIntentsEnabled;
        // Support for named incognito profiles was added in 87. Checking is done in
        // WebFragment, as this code should not trigger loading WebLayer.
        Bundle args = new Bundle();
        args.putString(BrowserFragmentArgs.PROFILE_NAME, profileName);
        if (params.persistenceId != null) {
            args.putString(BrowserFragmentArgs.PERSISTENCE_ID, params.persistenceId);
        }
        if (params.allowedOrigins != null) {
            args.putStringArrayList(
                    BrowserFragmentArgs.ALLOWED_ORIGINS, (ArrayList<String>) params.allowedOrigins);
        }
        args.putBoolean(BrowserFragmentArgs.IS_INCOGNITO, isIncognito);
        args.putBoolean(BrowserFragmentArgs.IS_EXTERNAL_INTENTS_ENABLED, isExternalIntentsEnabled);
        args.putBoolean(BrowserFragmentArgs.USE_VIEW_MODEL, false);

        return args;
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

    @Override
    public void shutdown() {
        mHandler.post(() -> {
            // This is for the weblayer.Browser / weblayer_private.BrowserImpl which has a lifetime
            // that exceeds the Fragment, onDestroy only destroys the Fragment UI.
            // TODO(swestphal): Check order.
            mBrowser.prepareForDestroy();
            mBrowser.shutdown();
            mBrowser.onDestroyed();
            mBrowser = null;
        });
    }
}