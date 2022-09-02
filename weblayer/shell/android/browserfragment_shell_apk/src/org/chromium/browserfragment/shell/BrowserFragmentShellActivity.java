// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.shell;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import com.google.common.util.concurrent.AsyncFunction;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Log;
import org.chromium.browserfragment.Browser;
import org.chromium.browserfragment.BrowserFragment;
import org.chromium.browserfragment.FragmentParams;
import org.chromium.browserfragment.Navigation;
import org.chromium.browserfragment.NavigationObserver;
import org.chromium.browserfragment.Tab;
import org.chromium.browserfragment.TabListObserver;
import org.chromium.browserfragment.TabManager;
import org.chromium.browserfragment.TabObserver;
import org.chromium.browserfragment.WebMessageCallback;
import org.chromium.browserfragment.WebMessageReplyProxy;

import java.util.Arrays;
import java.util.List;

/**
 * Activity for managing the Demo Shell.
 */
public class BrowserFragmentShellActivity extends AppCompatActivity {
    private static final String TAG = "BrowserFragmentShell";

    private static final String BROWSER_FRAGMENT_TAG = "BROWSER_FRAGMENT_TAG";

    private Context mContext;

    private Browser mBrowser;
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
                onBrowserReady(browser, savedInstanceState);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, mContext.getMainExecutor());

        final Button createTabButton = findViewById(R.id.create_tab);
        final Button navigateButton = findViewById(R.id.navigate_tab);
        final Button shutdownButton = findViewById(R.id.shut_down);

        createTabButton.setOnClickListener((View v) -> {
            if (mTabManager != null) {
                ListenableFuture<Tab> newTabFuture = mTabManager.createTab();
                Futures.addCallback(newTabFuture, new FutureCallback<Tab>() {
                    @Override
                    public void onSuccess(Tab newTab) {
                        navigateButton.setEnabled(true);
                        newTab.getNavigationController().navigate("https://google.com");
                        // TODO(swestphal): Call this in a Tab-loaded-callback instead.
                        navigateButton.setOnClickListener((View v) -> {
                            navigateButton.setEnabled(false);
                            newTab.setActive();
                        });
                    }
                    @Override
                    public void onFailure(Throwable thrown) {}
                }, mContext.getMainExecutor());
            }
        });

        shutdownButton.setOnClickListener((View v) -> {
            if (mBrowser != null) {
                mBrowser.shutdown();
            }
        });
    }

    private void onBrowserReady(Browser browser, Bundle savedInstanceState) {
        mBrowser = browser;
        browser.setRemoteDebuggingEnabled(true);

        BrowserFragment fragment = getOrCreateBrowserFragment(browser, savedInstanceState);

        fragment.registerTabListObserver(new TabListObserver() {
            @Override
            public void onActiveTabChanged(@Nullable Tab activeTab) {
                Log.i(TAG, "received BrowserEvent: 'onActiveTabChanged'-event");
            }

            @Override
            public void onTabAdded(@NonNull Tab tab) {
                Log.i(TAG, "received BrowserEvent: 'onTabAdded'-event");
                tab.registerTabObserver(new TabObserver() {
                    @Override
                    public void onVisibleUriChanged(@NonNull String uri) {
                        Log.i(TAG, "received TabEvent: 'onVisibleUriChanged(" + uri + ")'");
                    }

                    @Override
                    public void onTitleUpdated(@NonNull String title) {
                        Log.i(TAG, "received TabEvent: 'onTitleUpdated(" + title + ")'");
                    }

                    @Override
                    public void onRenderProcessGone() {
                        Log.i(TAG, "received TabEvent: 'onRenderProcessGone()'");
                    }
                });

                tab.getNavigationController().registerNavigationObserver(new NavigationObserver() {
                    @Override
                    public void onNavigationFailed(@NonNull Navigation navigation) {
                        Log.i(TAG, "received NavigationEvent: 'onNavigationFailed()';");
                        Log.i(TAG,
                                "Navigation: url:" + navigation.getUri()
                                        + ", HTTP-StatusCode: " + navigation.getStatusCode()
                                        + ", samePage: " + navigation.isSameDocument());
                    }

                    @Override
                    public void onNavigationCompleted(@NonNull Navigation navigation) {
                        Log.i(TAG, "received NavigationEvent: 'onNavigationCompleted()';");
                        Log.i(TAG,
                                "Navigation: url:" + navigation.getUri()
                                        + ", HTTP-StatusCode: " + navigation.getStatusCode()
                                        + ", samePage: " + navigation.isSameDocument());
                    }

                    @Override
                    public void onNavigationStarted(@NonNull Navigation navigation) {
                        Log.i(TAG, "received NavigationEvent: 'onNavigationStarted()'");
                        Log.i(TAG,
                                "Navigation: url:" + navigation.getUri()
                                        + ", HTTP-StatusCode: " + navigation.getStatusCode()
                                        + ", samePage: " + navigation.isSameDocument());
                    }

                    @Override
                    public void onLoadProgressChanged(double progress) {
                        Log.i(TAG,
                                "received NavigationEvent: 'onLoadProgressChanged(" + progress
                                        + ")'");
                    }
                });
            }

            @Override
            public void onTabRemoved(@NonNull Tab tab) {
                Log.i(TAG, "received BrowserEvent: 'onTabRemoved'-event");
            }

            @Override
            public void onWillDestroyBrowserAndAllTabs() {
                Log.i(TAG, "received BrowserEvent: 'onWillDestroyBrowserAndAllTabs'-event");
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
                if (savedInstanceState == null) {
                    // TODO(rayankans): Expose Tab URL to avoid relying on |savedInstanceState|.
                    activeTab.getNavigationController().navigate("https://google.com");

                    activeTab.registerWebMessageCallback(new WebMessageCallback() {
                        @Override
                        public void onWebMessageReceived(
                                WebMessageReplyProxy replyProxy, String message) {
                            Log.i(TAG, "received WebMessage: " + message);
                            replyProxy.postMessage("Bouncing answer from browser: " + message);
                        }

                        @Override
                        public void onWebMessageReplyProxyClosed(WebMessageReplyProxy replyProxy) {}

                        @Override
                        public void onWebMessageReplyProxyActiveStateChanged(
                                WebMessageReplyProxy proxy) {}
                    }, "x", Arrays.asList("*"));
                }
            }
            @Override
            public void onFailure(Throwable thrown) {}
        }, mContext.getMainExecutor());
    }

    private BrowserFragment getOrCreateBrowserFragment(Browser browser, Bundle savedInstanceState) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (savedInstanceState != null) {
            List<Fragment> fragments = fragmentManager.getFragments();
            if (fragments.size() > 1) {
                throw new IllegalStateException("More than one fragment added, shouldn't happen");
            }
            if (fragments.size() == 1) {
                return (BrowserFragment) fragments.get(0);
            }
        }

        FragmentParams params =
                (new FragmentParams.Builder()).setProfileName("DefaultProfile").build();
        BrowserFragment fragment = browser.createFragment(params);

        fragmentManager.beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, fragment, BROWSER_FRAGMENT_TAG)
                .commit();

        return fragment;
    }

    @Override
    public void onBackPressed() {
        BrowserFragment fragment = (BrowserFragment) getSupportFragmentManager().findFragmentByTag(
                BROWSER_FRAGMENT_TAG);
        if (fragment == null) {
            super.onBackPressed();
            return;
        }
        ListenableFuture<Boolean> tryNavigateBackFuture = mTabManager.tryNavigateBack();
        Futures.addCallback(tryNavigateBackFuture, new FutureCallback<Boolean>() {
            @Override
            public void onSuccess(Boolean didNavigate) {
                if (!didNavigate) {
                    BrowserFragmentShellActivity.super.onBackPressed();
                }
            }
            @Override
            public void onFailure(Throwable thrown) {
                BrowserFragmentShellActivity.super.onBackPressed();
            }
        }, mContext.getMainExecutor());
    }
}
