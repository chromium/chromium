// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.content.Context;
import android.net.Uri;
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
import org.chromium.webengine.CookieManager;
import org.chromium.webengine.FragmentParams;
import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.TabObserver;
import org.chromium.webengine.WebFragment;
import org.chromium.webengine.WebMessageCallback;
import org.chromium.webengine.WebMessageReplyProxy;
import org.chromium.webengine.WebSandbox;

import java.util.Arrays;
import java.util.List;

/**
 * Activity for managing the Demo Shell.
 */
public class WebEngineShellActivity extends AppCompatActivity {
    private static final String TAG = "WebEngineShell";

    private static final String WEB_FRAGMENT_TAG = "WEB_FRAGMENT_TAG";

    private Context mContext;

    private WebSandbox mWebSandbox;
    private TabManager mTabManager;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        mContext = getApplicationContext();

        ListenableFuture<WebSandbox> webSandboxFuture = WebSandbox.create(mContext);
        Futures.addCallback(webSandboxFuture, new FutureCallback<WebSandbox>() {
            @Override
            public void onSuccess(WebSandbox webSandbox) {
                onWebSandboxReady(webSandbox, savedInstanceState);
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
            if (mWebSandbox != null) {
                mWebSandbox.shutdown();
            }
        });
    }

    private void onWebSandboxReady(WebSandbox webSandbox, Bundle savedInstanceState) {
        mWebSandbox = webSandbox;
        webSandbox.setRemoteDebuggingEnabled(true);

        WebFragment fragment = getOrCreateWebFragment(webSandbox, savedInstanceState);

        fragment.registerTabListObserver(new TabListObserver() {
            @Override
            public void onActiveTabChanged(@Nullable Tab activeTab) {
                Log.i(TAG, "received TabList Event: 'onActiveTabChanged'-event");
            }

            @Override
            public void onTabAdded(@NonNull Tab tab) {
                Log.i(TAG, "received TabList Event: 'onTabAdded'-event");
                tab.registerTabObserver(new TabObserver() {
                    @Override
                    public void onVisibleUriChanged(@NonNull String uri) {
                        Log.i(TAG, "received Tab Event: 'onVisibleUriChanged(" + uri + ")'");
                    }

                    @Override
                    public void onTitleUpdated(@NonNull String title) {
                        Log.i(TAG, "received Tab Event: 'onTitleUpdated(" + title + ")'");
                    }

                    @Override
                    public void onRenderProcessGone() {
                        Log.i(TAG, "received Tab Event: 'onRenderProcessGone()'");
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
                        ListenableFuture<String> scriptResultFuture =
                                tab.executeScript("1+1", true);
                        Futures.addCallback(scriptResultFuture, new FutureCallback<String>() {
                            @Override
                            public void onSuccess(String result) {
                                Log.w(TAG, "executeScript result: " + result);
                            }

                            @Override
                            public void onFailure(Throwable thrown) {
                                Log.w(TAG, "executeScript failed: " + thrown);
                            }
                        }, mContext.getMainExecutor());
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
                    public void onNavigationRedirected(@NonNull Navigation navigation) {
                        Log.i(TAG, "received NavigationEvent: 'onNavigationRedirected()'");
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
                Log.i(TAG, "received TabList Event: 'onTabRemoved'-event");
            }

            @Override
            public void onWillDestroyFragmentAndAllTabs() {
                Log.i(TAG, "received TabList Event: 'onWillDestroyFragmentAndAllTabs'-event");
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
                if (activeTab.getDisplayUri().equals(Uri.EMPTY)) {
                    activeTab.getNavigationController().navigate("https://google.com");

                    activeTab.registerWebMessageCallback(new WebMessageCallback() {
                        @Override
                        public void onWebMessageReceived(
                                WebMessageReplyProxy replyProxy, String message) {
                            Log.i(TAG, "received WebMessage: " + message);
                            replyProxy.postMessage("Bouncing answer from tab: " + message);
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

        ListenableFuture<CookieManager> cookieManagerFuture = fragment.getCookieManager();
        Futures.addCallback(cookieManagerFuture, new FutureCallback<CookieManager>() {
            @Override
            public void onSuccess(CookieManager cookieManager) {
                ListenableFuture<Void> setCookieFuture =
                        cookieManager.setCookie("https://sadchonks.com", "foo=bar123");
                Futures.addCallback(setCookieFuture, new FutureCallback<Void>() {
                    @Override
                    public void onSuccess(Void v) {
                        ListenableFuture<String> cookieFuture =
                                cookieManager.getCookie("https://sadchonks.com");
                        Futures.addCallback(cookieFuture, new FutureCallback<String>() {
                            @Override
                            public void onSuccess(String value) {
                                Log.w(TAG, "cookie: " + value);
                            }

                            @Override
                            public void onFailure(Throwable thrown) {}
                        }, mContext.getMainExecutor());
                    }

                    @Override
                    public void onFailure(Throwable thrown) {
                        Log.w(TAG, "setCookie failed: " + thrown);
                    }
                }, mContext.getMainExecutor());
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, mContext.getMainExecutor());
    }

    private WebFragment getOrCreateWebFragment(WebSandbox webSandbox, Bundle savedInstanceState) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (savedInstanceState != null) {
            List<Fragment> fragments = fragmentManager.getFragments();
            if (fragments.size() > 1) {
                throw new IllegalStateException("More than one fragment added, shouldn't happen");
            }
            if (fragments.size() == 1) {
                return (WebFragment) fragments.get(0);
            }
        }

        FragmentParams params =
                (new FragmentParams.Builder()).setProfileName("DefaultProfile").build();
        WebFragment fragment = webSandbox.createFragment(params);

        fragmentManager.beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, fragment, WEB_FRAGMENT_TAG)
                .commit();

        return fragment;
    }

    @Override
    public void onBackPressed() {
        WebFragment fragment =
                (WebFragment) getSupportFragmentManager().findFragmentByTag(WEB_FRAGMENT_TAG);
        if (fragment == null) {
            super.onBackPressed();
            return;
        }
        ListenableFuture<Boolean> tryNavigateBackFuture = mTabManager.tryNavigateBack();
        Futures.addCallback(tryNavigateBackFuture, new FutureCallback<Boolean>() {
            @Override
            public void onSuccess(Boolean didNavigate) {
                if (!didNavigate) {
                    WebEngineShellActivity.super.onBackPressed();
                }
            }
            @Override
            public void onFailure(Throwable thrown) {
                WebEngineShellActivity.super.onBackPressed();
            }
        }, mContext.getMainExecutor());
    }
}
