// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.Spinner;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.Tab;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

/**
 * Activity for testing navigations and state resumption.
 */
public class WebEngineNavigationTestActivity extends AppCompatActivity {
    private static final String TAG = "WebEngineShell";

    private static final String WEB_ENGINE_TAG = "WEB_ENGINE_TAG";

    private Context mContext;

    private WebSandbox mWebSandbox;

    private DefaultObservers mDefaultObservers = new DefaultObservers();

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.navigation_test);
        mContext = getApplicationContext();

        WebEngineShellActivity.setupActivitySpinner(
                (Spinner) findViewById(R.id.activity_nav), this, 2);

        ListenableFuture<WebSandbox> webSandboxFuture = WebSandbox.create(mContext);
        Futures.addCallback(webSandboxFuture, new FutureCallback<WebSandbox>() {
            @Override
            public void onSuccess(WebSandbox webSandbox) {
                onWebSandboxReady(webSandbox, savedInstanceState);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, ContextCompat.getMainExecutor(mContext));

        Button openActivityButton = (Button) findViewById(R.id.open_activity);
        openActivityButton.setOnClickListener(
                (View v) -> { super.startActivity(new Intent(this, EmptyActivity.class)); });

        Button replaceFragmentButton = (Button) findViewById(R.id.replace_fragment);
        replaceFragmentButton.setOnClickListener((View v) -> {
            getSupportFragmentManager()
                    .beginTransaction()
                    .setReorderingAllowed(true)
                    .add(R.id.fragment_container_view, new EmptyFragment())
                    .addToBackStack(null)
                    .commit();
        });
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

    private void onWebSandboxReady(WebSandbox webSandbox, Bundle savedInstanceState) {
        mWebSandbox = webSandbox;
        webSandbox.setRemoteDebuggingEnabled(true);

        WebEngine webEngine = webSandbox.getWebEngine(WEB_ENGINE_TAG);
        if (webEngine != null) {
            assert webSandbox.getWebEngines().size() == 1;

            return;
        }

        ListenableFuture<WebEngine> webEngineFuture = webSandbox.createWebEngine(WEB_ENGINE_TAG);
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
        TabManager tabManager = webEngine.getTabManager();

        Tab activeTab = tabManager.getActiveTab();
        activeTab.getNavigationController().navigate("https://google.com");

        activeTab.registerTabObserver(mDefaultObservers);
        activeTab.getNavigationController().registerNavigationObserver(mDefaultObservers);
        tabManager.registerTabListObserver(mDefaultObservers);

        getSupportFragmentManager()
                .beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, webEngine.getFragment())
                .commit();
    }

    /**
     * Empty Activity used to test back navigation to an Activity containing a WebFragment.
     */
    public static class EmptyActivity extends AppCompatActivity {}

    /**
     * Empty Fragment used to test back navigation to a WebFragment.
     */
    public static class EmptyFragment extends Fragment {
        @Override
        public View onCreateView(
                LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
            View v = new View(getActivity());
            v.setLayoutParams(new LayoutParams(LayoutParams.FILL_PARENT, LayoutParams.FILL_PARENT));
            v.setBackgroundColor(Color.parseColor("#f1f1f1"));
            return v;
        }
    }
}
