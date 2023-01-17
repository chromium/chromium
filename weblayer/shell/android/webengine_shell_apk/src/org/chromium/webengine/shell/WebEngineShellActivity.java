// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Log;
import org.chromium.webengine.CookieManager;
import org.chromium.webengine.Navigation;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebFragment;
import org.chromium.webengine.WebMessageCallback;
import org.chromium.webengine.WebMessageReplyProxy;
import org.chromium.webengine.WebSandbox;

import java.util.Arrays;

/**
 * Activity for managing the Demo Shell.
 *
 * TODO(swestphal):
 *  - Add url bar
 *  - UI to add/remove/switch tabs
 *  - Progress bar when navigation is ongoing
 *  - Expose some tab/navigation events in the UI
 *  - Move cookie test to manual-test activity
 *  - Move registerWebMessageCallback to manual-test activity
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

        setupActivitySpinner((Spinner) findViewById(R.id.activity_nav), this, 0);

        ListenableFuture<String> sandboxVersionFuture = WebSandbox.getVersion(mContext);

        Futures.addCallback(sandboxVersionFuture, new FutureCallback<String>() {
            @Override
            public void onSuccess(String version) {
                ((TextView) findViewById(R.id.version)).setText(version);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, mContext.getMainExecutor());

        ListenableFuture<WebSandbox> webSandboxFuture = WebSandbox.create(mContext);
        Futures.addCallback(webSandboxFuture, new FutureCallback<WebSandbox>() {
            @Override
            public void onSuccess(WebSandbox webSandbox) {
                onWebSandboxReady(webSandbox);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, mContext.getMainExecutor());
    }

    @Override
    public void startActivity(Intent intent) {
        if (mWebSandbox != null) {
            // Shutdown sandbox before another activity is opened.
            mWebSandbox.shutdown();
            mWebSandbox = null;
        }
        super.startActivity(intent);
    }

    private void onWebSandboxReady(WebSandbox webSandbox) {
        mWebSandbox = webSandbox;
        webSandbox.setRemoteDebuggingEnabled(true);

        WebEngine webEngine = webSandbox.getWebEngine("shell-engine");
        if (webEngine != null) {
            assert webSandbox.getWebEngines().size() == 1;

            mTabManager = webEngine.getTabManager();
            return;
        }

        ListenableFuture<WebEngine> webEngineFuture = webSandbox.createWebEngine("shell-engine");
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
        mTabManager = webEngine.getTabManager();
        CookieManager cookieManager = webEngine.getCookieManager();

        Tab activeTab = mTabManager.getActiveTab();

        activeTab.registerTabObserver(new DefaultObservers.DefaultTabObserver());
        activeTab.getNavigationController().registerNavigationObserver(
                new DefaultObservers.DefaultNavigationObserver() {
                    @Override
                    public void onNavigationCompleted(@NonNull Navigation navigation) {
                        super.onNavigationCompleted(navigation);
                        ListenableFuture<String> scriptResultFuture =
                                activeTab.executeScript("1+1", true);
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
                });

        activeTab.getNavigationController().navigate("https://google.com");

        activeTab.registerWebMessageCallback(new WebMessageCallback() {
            @Override
            public void onWebMessageReceived(WebMessageReplyProxy replyProxy, String message) {
                Log.i(TAG, "received WebMessage: " + message);
                replyProxy.postMessage("Bouncing answer from tab: " + message);
            }

            @Override
            public void onWebMessageReplyProxyClosed(WebMessageReplyProxy replyProxy) {}

            @Override
            public void onWebMessageReplyProxyActiveStateChanged(WebMessageReplyProxy proxy) {}
        }, "x", Arrays.asList("*"));

        mTabManager.registerTabListObserver(new DefaultObservers.DefaultTabListObserver());

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

        getSupportFragmentManager()
                .beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, webEngine.getFragment(), WEB_FRAGMENT_TAG)
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
        ListenableFuture<Boolean> tryNavigateBackFuture = fragment.getWebEngine().tryNavigateBack();
        Futures.addCallback(tryNavigateBackFuture, new FutureCallback<Boolean>() {
            @Override
            public void onSuccess(Boolean didNavigate) {
                if (!didNavigate) {
                    WebEngineShellActivity.super.onBackPressed();
                }
            }
            @Override
            public void onFailure(Throwable thrown) {
                if (mWebSandbox != null) {
                    mWebSandbox.shutdown();
                }
                WebEngineShellActivity.super.onBackPressed();
            }
        }, mContext.getMainExecutor());
    }

    // TODO(swestphal): Move this to a helper class.
    static void setupActivitySpinner(Spinner spinner, Activity activity, int index) {
        ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(
                activity, R.array.activities_drop_down, android.R.layout.simple_spinner_item);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);
        spinner.setSelection(index, false);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                final Intent intent;
                switch (pos) {
                    case 0:
                        intent = new Intent(activity, WebEngineShellActivity.class);
                        break;
                    case 1:
                        intent = new Intent(activity, WebEngineStateTestActivity.class);
                        break;
                    case 2:
                        intent = new Intent(activity, WebEngineNavigationTestActivity.class);
                        break;
                    default:
                        assert false : "Unhandled item: " + String.valueOf(pos);
                        intent = null;
                }
                activity.startActivity(intent);
                activity.finish();
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
    }
}
