// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Log;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

import java.util.List;
import java.util.Set;

/**
 * Activity for managing the Demo Shell.
 */
public class WebEngineStateTestActivity extends AppCompatActivity {
    private static final String TAG = "WebEngineShell";

    private static final String WEB_ENGINE_TAG = "WEB_ENGINE_TAG";

    private Context mContext;

    private WebSandbox mWebSandbox;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.state_test);
        mContext = getApplicationContext();

        setSandboxStateText(false);
        setEngineStateText(false);
        setNumberOfTabsText(0);
        setVisibilityText(false);

        WebEngineShellActivity.setupActivitySpinner(
                (Spinner) findViewById(R.id.activity_nav), this, 1);

        final Button startSandboxButton = findViewById(R.id.start_sandbox);
        startSandboxButton.setOnClickListener((View v) -> { startupSandbox(); });
        final Button shutdownSandboxButton = findViewById(R.id.shutdown_sandbox);
        shutdownSandboxButton.setOnClickListener((View v) -> {
            boolean shutdown = shutdownSandbox();
            Log.i(TAG, "Sandbox shutdown successful: " + shutdown);
        });

        final Button startEngineButton = findViewById(R.id.start_engine);
        startEngineButton.setOnClickListener((View v) -> { startupWebEngine(); });
        final Button shutdownEngineButton = findViewById(R.id.shutdown_engine);
        shutdownEngineButton.setOnClickListener((View v) -> {
            boolean closed = closeWebEngine();
            Log.i(TAG, "WenEngine closed successfully: " + closed);
        });

        final Button openTabButton = findViewById(R.id.open_tab);
        openTabButton.setOnClickListener(
                (View v) -> { openNewTabAndNavigate("https://google.com"); });
        final Button closeTabButton = findViewById(R.id.close_tab);
        closeTabButton.setOnClickListener((View v) -> {
            boolean closed = closeTab();
            Log.i(TAG, "Closed tab successfully: " + closed);
        });

        final Button inflateFragmentButton = findViewById(R.id.inflate_fragment);
        inflateFragmentButton.setOnClickListener((View v) -> {
            boolean inflated = inflateFragment();
            Log.i(TAG, "Fragment inflation successful: " + inflated);
        });
        final Button removeFragmentButton = findViewById(R.id.remove_fragment);
        removeFragmentButton.setOnClickListener((View v) -> {
            boolean removed = removeFragment();
            Log.i(TAG, "Fragment removal successful: " + removed);
        });
    }

    @Override
    public void startActivity(Intent intent) {
        if (mWebSandbox != null) {
            // Shutdown sandbox before another activity is opened.
            mWebSandbox.shutdown();
        }
        super.startActivity(intent);
    }

    @Override
    public void onBackPressed() {
        if (mWebSandbox != null) {
            mWebSandbox.shutdown();
        }
        super.onBackPressed();
    }

    private void setNumberOfTabsText(int num) {
        ((TextView) findViewById(R.id.num_open_tabs)).setText("Tabs: (" + num + ")");
    }

    private void setSandboxStateText(boolean on) {
        ((TextView) findViewById(R.id.sandbox_state))
                .setText("Sandbox: (" + (on ? "on" : "off") + ")");
    }

    private void setEngineStateText(boolean on) {
        ((TextView) findViewById(R.id.engine_state))
                .setText("Engine: (" + (on ? "started" : "closed") + ")");
    }

    private void setVisibilityText(boolean visible) {
        ((TextView) findViewById(R.id.fragment_state))
                .setText("Fragment: (" + (visible ? "visible" : "gone") + ")");
    }

    private void startupSandbox() {
        ListenableFuture<WebSandbox> webSandboxFuture = WebSandbox.create(mContext);
        Futures.addCallback(webSandboxFuture, new FutureCallback<WebSandbox>() {
            @Override
            public void onSuccess(WebSandbox webSandbox) {
                onWebSandboxReady(webSandbox);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, ContextCompat.getMainExecutor(mContext));
    }

    private void onWebSandboxReady(WebSandbox webSandbox) {
        setSandboxStateText(true);
        mWebSandbox = webSandbox;
        webSandbox.setRemoteDebuggingEnabled(true);

        WebEngine currentWebEngine = getCurrentWebEngine();
        if (currentWebEngine != null) {
            Log.i(TAG, "Sandbox and WebEngine already created");
            return;
        }
        Log.i(TAG, "Sandbox ready");
        startupWebEngine();
    }

    private void startupWebEngine() {
        if (mWebSandbox == null) {
            Log.w(TAG, "WebSandbox not started");
            return;
        }
        WebEngine webEngine = getCurrentWebEngine();
        if (webEngine != null) {
            Log.w(TAG, "WebEngine already created");
            return;
        }
        ListenableFuture<WebEngine> webEngineFuture = mWebSandbox.createWebEngine(WEB_ENGINE_TAG);
        Futures.addCallback(webEngineFuture, new FutureCallback<WebEngine>() {
            @Override
            public void onSuccess(WebEngine webEngine) {
                Log.i(TAG, "WebEngine started");
                onWebEngineReady(webEngine);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, ContextCompat.getMainExecutor(mContext));
    }

    private void onWebEngineReady(WebEngine webEngine) {
        setEngineStateText(true);
        TabManager tabManager = webEngine.getTabManager();
        setNumberOfTabsText(tabManager.getAllTabs().size());

        tabManager.registerTabListObserver(new DefaultObservers.DefaultTabListObserver() {
            @Override
            public void onTabAdded(@NonNull Tab tab) {
                super.onTabAdded(tab);
                setNumberOfTabsText(tabManager.getAllTabs().size());
                // Recursively add tab and navigation observers to any new tab.
                tab.registerTabObserver(new DefaultObservers.DefaultTabObserver());
                tab.getNavigationController().registerNavigationObserver(
                        new DefaultObservers.DefaultNavigationObserver());
            }

            @Override
            public void onTabRemoved(@NonNull Tab tab) {
                super.onTabRemoved(tab);
                setNumberOfTabsText(tabManager.getAllTabs().size());
            }

            @Override
            public void onWillDestroyFragmentAndAllTabs() {
                super.onWillDestroyFragmentAndAllTabs();
                setNumberOfTabsText(tabManager.getAllTabs().size());
            }
        });

        Tab activeTab = tabManager.getActiveTab();
        activeTab.registerTabObserver(new DefaultObservers.DefaultTabObserver());
        activeTab.getNavigationController().registerNavigationObserver(
                new DefaultObservers.DefaultNavigationObserver());
        activeTab.getNavigationController().navigate("https://www.google.com");
    }

    /**
     * Tries to shutdown WebEngine and returns if succeeded.
     */
    private boolean closeWebEngine() {
        setEngineStateText(false);
        setVisibilityText(false);
        WebEngine webEngine = getCurrentWebEngine();

        if (webEngine != null) {
            webEngine.close();
            return true;
        }
        return false;
    }

    /**
     * Tries to shutdown the Sandbox and returns if succeeded.
     */
    private boolean shutdownSandbox() {
        if (mWebSandbox == null) return false;

        mWebSandbox.shutdown();
        setEngineStateText(false);
        setSandboxStateText(false);
        setVisibilityText(false);
        return true;
    }

    private void openNewTabAndNavigate(String url) {
        WebEngine webEngine = getCurrentWebEngine();
        if (webEngine == null) {
            Log.w(TAG, "No WebEngine created");
            return;
        }
        ListenableFuture<Tab> newTabFuture = webEngine.getTabManager().createTab();
        Futures.addCallback(newTabFuture, new FutureCallback<Tab>() {
            @Override
            public void onSuccess(Tab newTab) {
                newTab.setActive();
                newTab.getNavigationController().navigate(url);
                Log.i(TAG, "Tab opened");
            }
            @Override
            public void onFailure(Throwable thrown) {
                Log.i(TAG, "Opening Tab failed");
            }
        }, ContextCompat.getMainExecutor(mContext));
    }

    private boolean closeTab() {
        WebEngine webEngine = getCurrentWebEngine();
        if (webEngine == null) return false;

        Tab activeTab = webEngine.getTabManager().getActiveTab();

        if (activeTab == null) return false;
        activeTab.close();

        Set<Tab> allTabs = webEngine.getTabManager().getAllTabs();
        allTabs.remove(activeTab);
        if (allTabs.size() > 0) {
            allTabs.iterator().next().setActive();
        }

        return true;
    }

    // There could be more but we only have one in this test activity.
    private WebEngine getCurrentWebEngine() {
        if (mWebSandbox == null) return null;

        WebEngine webEngine = mWebSandbox.getWebEngine(WEB_ENGINE_TAG);
        if (webEngine != null) {
            assert mWebSandbox.getWebEngines().size() == 1;
            return webEngine;
        }
        return null;
    }

    /**
     * Tries to inflate the fragment and returns if succeeded.
     */
    private boolean inflateFragment() {
        WebEngine webEngine = getCurrentWebEngine();
        if (webEngine == null) {
            Log.w(TAG, "no WebEngine created");
            return false;
        }

        FragmentManager fragmentManager = getSupportFragmentManager();
        List<Fragment> fragments = fragmentManager.getFragments();
        if (fragments.size() > 1) {
            throw new IllegalStateException("More than one fragment added, shouldn't happen");
        }
        if (fragments.size() == 1) {
            // Fragment already inflated.
            return false;
        }
        setVisibilityText(true);
        getSupportFragmentManager()
                .beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, webEngine.getFragment())
                .commit();
        return true;
    }

    /**
     * Tries to remove the WebFragment and returns if succeeded.
     */
    private boolean removeFragment() {
        FragmentManager fragmentManager = getSupportFragmentManager();
        List<Fragment> fragments = fragmentManager.getFragments();
        if (fragments.size() > 1) {
            throw new IllegalStateException("More than one fragment added, shouldn't happen");
        }
        if (fragments.size() == 0) return false; // Fragment not inflated.

        setVisibilityText(false);
        getSupportFragmentManager()
                .beginTransaction()
                .setReorderingAllowed(true)
                .remove(fragments.get(0))
                .commit();
        return true;
    }
}
