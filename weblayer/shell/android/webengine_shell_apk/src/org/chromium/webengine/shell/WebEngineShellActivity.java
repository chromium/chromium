// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentManager;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Log;
import org.chromium.webengine.CookieManager;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.WebEngine;
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
            if (mTabManager == null) return;
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

        List<WebEngine> webEngines = webSandbox.getWebEngines();
        if (webEngines.size() > 0) {
            assert webEngines.size() == 1;

            mTabManager = webEngines.get(0).getTabManager();
            return;
        }

        ListenableFuture<WebEngine> webEngineFuture = webSandbox.createWebEngine();
        Futures.addCallback(webEngineFuture, new FutureCallback<WebEngine>() {
            @Override
            public void onSuccess(WebEngine webEngine) {
                onWebEngineReady(webEngine);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, mContext.getMainExecutor());
    }

    private void onWebEngineReady(WebEngine webEngine) {
        WebFragment fragment = webEngine.getFragment();
        mTabManager = webEngine.getTabManager();
        CookieManager cookieManager = webEngine.getCookieManager();

        ListenableFuture<Tab> activeTabFuture = mTabManager.getActiveTab();

        Futures.addCallback(activeTabFuture, new FutureCallback<Tab>() {
            @Override
            public void onSuccess(Tab activeTab) {
                if (activeTab.getDisplayUri().equals(
                            Uri.EMPTY)) { // TODO(swestphal): remove as not needed anymore
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

        FragmentManager fragmentManager = getSupportFragmentManager();
        fragmentManager.beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, fragment, WEB_FRAGMENT_TAG)
                .commit();
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
