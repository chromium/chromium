// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.shell;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.google.common.util.concurrent.AsyncFunction;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Log;
import org.chromium.browserfragment.Browser;
import org.chromium.browserfragment.BrowserFragment;
import org.chromium.browserfragment.Tab;
import org.chromium.browserfragment.TabManager;
import org.chromium.browserfragment.TabNavigationController;
import org.chromium.browserfragment.TabObserver;

/**
 * Activity for managing the Demo Shell.
 */
public class BrowserFragmentShellActivity extends AppCompatActivity {
    private static final String TAG = "BrowserFragmentShell";

    private static final String BROWSER_FRAGMENT_TAG = "BROWSER_FRAGMENT_TAG";

    private Context mContext;

    private TabManager mTabManager;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        mContext = getApplicationContext();

        ListenableFuture<Browser> browserFuture = Browser.create(mContext);
        Futures.addCallback(browserFuture, new FutureCallback<Browser>() {
            @Override
            public void onSuccess(Browser browser) {
                onBrowserReady(browser);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, mContext.getMainExecutor());
    }

    private void onBrowserReady(Browser browser) {
        browser.setRemoteDebuggingEnabled(true);

        BrowserFragment fragment = browser.createFragment();

        fragment.registerTabObserver(new TabObserver() {
            @Override
            public void onActiveTabChanged(@Nullable Tab activeTab) {
                Log.i(TAG, "received 'onActiveTabChanged'-event");
            }

            @Override
            public void onTabAdded(@NonNull Tab tab) {
                Log.i(TAG, "received 'onTabAdded'-event");
                tab.setActive();
            }

            @Override
            public void onTabRemoved(@NonNull Tab tab) {
                Log.i(TAG, "received 'onTabRemoved'-event");
            }

            @Override
            public void onWillDestroyBrowserAndAllTabs() {
                Log.i(TAG, "received 'onWillDestroyBrowserAndAllTabs'-event");
            }
        });

        ListenableFuture<TabManager> tabManagerFuture = fragment.getTabManager();
        AsyncFunction<TabManager, Tab> getActiveTabTask = tabManager -> {
            mTabManager = tabManager;
            return tabManager.getActiveTab();
        };
        ListenableFuture<Tab> activeTabFuture = Futures.transformAsync(
                tabManagerFuture, getActiveTabTask, mContext.getMainExecutor());

        Futures.addCallback(activeTabFuture, new FutureCallback<Tab>() {
            @Override
            public void onSuccess(Tab activeTab) {
                activeTab.getNavigationController().navigate("https://google.com");
            }
            @Override
            public void onFailure(Throwable thrown) {}
        }, mContext.getMainExecutor());

        getSupportFragmentManager()
                .beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, fragment, BROWSER_FRAGMENT_TAG)
                .commit();
    }

    @Override
    public void onBackPressed() {
        BrowserFragment fragment = (BrowserFragment) getSupportFragmentManager().findFragmentByTag(
                BROWSER_FRAGMENT_TAG);
        if (fragment == null || mTabManager == null) {
            // BrowserFragment not initialized.
            super.onBackPressed();
            return;
        }
        ListenableFuture<Tab> activeTabFuture = mTabManager.getActiveTab();

        Futures.addCallback(activeTabFuture, new FutureCallback<Tab>() {
            @Override
            public void onSuccess(Tab activeTab) {
                TabNavigationController tabNavigationController =
                        activeTab.getNavigationController();

                Futures.addCallback(
                        tabNavigationController.canGoBack(), new FutureCallback<Boolean>() {
                            @Override
                            public void onSuccess(Boolean canGoBack) {
                                if (canGoBack) {
                                    tabNavigationController.goBack();
                                    return;
                                }
                                BrowserFragmentShellActivity.super.onBackPressed();
                            }
                            @Override
                            public void onFailure(Throwable thrown) {
                                BrowserFragmentShellActivity.super.onBackPressed();
                            }
                        }, mContext.getMainExecutor());
            }
            @Override
            public void onFailure(Throwable thrown) {
                BrowserFragmentShellActivity.super.onBackPressed();
            }
        }, mContext.getMainExecutor());
    }
}
