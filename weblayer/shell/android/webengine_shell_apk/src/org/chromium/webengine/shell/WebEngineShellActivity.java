// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ProgressBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Log;
import org.chromium.webengine.CookieManager;
import org.chromium.webengine.FullscreenCallback;
import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebFragment;
import org.chromium.webengine.WebSandbox;
import org.chromium.webengine.shell.topbar.TopBarImpl;
import org.chromium.webengine.shell.topbar.TopBarObservers;

import java.util.Arrays;

/**
 * Activity for managing the Demo Shell.
 *
 * TODO(swestphal):
 *  - UI to add/remove tabs
 *  - Expose some tab/navigation events in the UI
 *  - Move cookie test to manual-test activity
 *  - Move registerWebMessageCallback to manual-test activity
 */
public class WebEngineShellActivity extends AppCompatActivity implements FullscreenCallback {
    private static final String TAG = "WebEngineShell";

    private static final String WEB_FRAGMENT_TAG = "WEB_FRAGMENT_TAG";

    private Context mContext;

    private WebSandbox mWebSandbox;
    private TabManager mTabManager;

    private DefaultObservers mDefaultTabListObserver;

    private int mSystemVisibilityToRestore;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        mContext = getApplicationContext();
        mDefaultTabListObserver = new DefaultObservers();

        setupActivitySpinner((Spinner) findViewById(R.id.activity_nav), this, 0);

        ListenableFuture<String> sandboxVersionFuture = WebSandbox.getVersion(mContext);

        Futures.addCallback(sandboxVersionFuture, new FutureCallback<String>() {
            @Override
            public void onSuccess(String version) {
                ((TextView) findViewById(R.id.version)).setText(version);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, ContextCompat.getMainExecutor(mContext));

        ListenableFuture<WebSandbox> webSandboxFuture = WebSandbox.create(mContext);
        Futures.addCallback(webSandboxFuture, new FutureCallback<WebSandbox>() {
            @Override
            public void onSuccess(WebSandbox webSandbox) {
                onWebSandboxReady(webSandbox);
            }

            @Override
            public void onFailure(Throwable thrown) {
                Toast.makeText(mContext, "Failed to start WebSandbox. WebView update needed.",
                             Toast.LENGTH_LONG)
                        .show();
            }
        }, ContextCompat.getMainExecutor(mContext));
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

            for (Tab tab : mTabManager.getAllTabs()) {
                tab.setFullscreenCallback(this);
            }
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
        }, ContextCompat.getMainExecutor(mContext));
    }

    private void onWebEngineReady(WebEngine webEngine) {
        mTabManager = webEngine.getTabManager();
        CookieManager cookieManager = webEngine.getCookieManager();

        Tab activeTab = mTabManager.getActiveTab();
        ProgressBar progressBar = findViewById(R.id.progress_bar);
        EditText urlBar = findViewById(R.id.url_bar);
        ImageButton reloadButton = findViewById(R.id.reload_button);
        Button tabCountButton = findViewById(R.id.tab_count);
        Spinner tabListSpinner = findViewById(R.id.tab_list);
        new TopBarObservers(new TopBarImpl(this, mTabManager, urlBar, progressBar, reloadButton,
                                    tabCountButton, tabListSpinner),
                mTabManager);

        activeTab.setFullscreenCallback(this);
        mTabManager.registerTabListObserver(new TabListObserver() {
            @Override
            public void onTabAdded(@NonNull WebEngine webEngine, @NonNull Tab tab) {
                tab.setFullscreenCallback(WebEngineShellActivity.this);
            }
        });
        activeTab.registerTabObserver(mDefaultTabListObserver);
        activeTab.getNavigationController().registerNavigationObserver(mDefaultTabListObserver);
        mTabManager.registerTabListObserver(mDefaultTabListObserver);

        activeTab.getNavigationController().registerNavigationObserver(new NavigationObserver() {
            @Override
            public void onNavigationCompleted(@NonNull Tab tab, @NonNull Navigation navigation) {
                ListenableFuture<String> scriptResultFuture = activeTab.executeScript("1+1", true);
                Futures.addCallback(
                        scriptResultFuture, new FutureCallback<String>() {
                            @Override
                            public void onSuccess(String result) {
                                Log.w(TAG, "executeScript result: " + result);
                            }
                            @Override
                            public void onFailure(Throwable thrown) {
                                Log.w(TAG, "executeScript failed: " + thrown);
                            }
                        }, ContextCompat.getMainExecutor(mContext));
            }
        });
        activeTab.getNavigationController().navigate("https://google.com");

        activeTab.addMessageEventListener((Tab source, String message) -> {
            Log.w(TAG, "Received post message from web content: " + message);
        }, Arrays.asList("*"));
        activeTab.postMessage("Hello!", "*");

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
                }, ContextCompat.getMainExecutor(mContext));
            }

            @Override
            public void onFailure(Throwable thrown) {
                Log.w(TAG, "setCookie failed: " + thrown);
            }
        }, ContextCompat.getMainExecutor(mContext));

        getSupportFragmentManager()
                .beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, webEngine.getFragment(), WEB_FRAGMENT_TAG)
                .commit();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mTabManager == null) return;
        for (Tab tab : mTabManager.getAllTabs()) {
            tab.setFullscreenCallback(null);
        }
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
        }, ContextCompat.getMainExecutor(mContext));
    }

    // TODO(swestphal): Move this to a helper class.
    static void setupActivitySpinner(Spinner spinner, Activity activity, int index) {
        ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(activity,
                R.array.activities_drop_down, android.R.layout.simple_spinner_dropdown_item);
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

    @Override
    public void onEnterFullscreen(WebEngine webEngine, Tab tab) {
        final WindowManager.LayoutParams attrs = getWindow().getAttributes();
        attrs.flags |= WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
        getWindow().setAttributes(attrs);

        findViewById(R.id.activity_nav).setVisibility(View.GONE);
        findViewById(R.id.version).setVisibility(View.GONE);
        findViewById(R.id.app_bar).setVisibility(View.GONE);
        findViewById(R.id.progress_bar).setVisibility(View.GONE);

        View decorView = getWindow().getDecorView();

        mSystemVisibilityToRestore = decorView.getSystemUiVisibility();
        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
                | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                | View.SYSTEM_UI_FLAG_LOW_PROFILE | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    @Override
    public void onExitFullscreen(WebEngine webEngine, Tab tab) {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(mSystemVisibilityToRestore);

        findViewById(R.id.activity_nav).setVisibility(View.VISIBLE);
        findViewById(R.id.version).setVisibility(View.VISIBLE);
        findViewById(R.id.app_bar).setVisibility(View.VISIBLE);
        findViewById(R.id.progress_bar).setVisibility(View.VISIBLE);

        final WindowManager.LayoutParams attrs = getWindow().getAttributes();
        if ((attrs.flags & WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS) != 0) {
            attrs.flags &= ~WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
            getWindow().setAttributes(attrs);
        }
    }
}
