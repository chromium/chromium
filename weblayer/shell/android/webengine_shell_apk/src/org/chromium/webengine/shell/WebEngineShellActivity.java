// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.util.Patterns;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
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
import org.chromium.webengine.FullscreenClient;
import org.chromium.webengine.Navigation;
import org.chromium.webengine.NavigationObserver;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebFragment;
import org.chromium.webengine.WebSandbox;
import org.chromium.webengine.shell.topbar.CustomSpinner;
import org.chromium.webengine.shell.topbar.TabEventsDelegate;
import org.chromium.webengine.shell.topbar.TabEventsObserver;

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
public class WebEngineShellActivity
        extends AppCompatActivity implements FullscreenCallback, TabEventsObserver {
    private static final String TAG = "WebEngineShell";

    private static final String WEB_FRAGMENT_TAG = "WEB_FRAGMENT_TAG";

    private WebEngineShellApplication mApplication;
    private Context mContext;

    private TabManager mTabManager;
    private TabEventsDelegate mTabEventsDelegate;

    private ProgressBar mProgressBar;
    private EditText mUrlBar;
    private Button mTabCountButton;
    private CustomSpinner mTabListSpinner;
    private ArrayAdapter<TabWrapper> mTabListAdapter;

    private ImageButton mReloadButton;
    private Drawable mRefreshDrawable;
    private Drawable mStopDrawable;

    private DefaultObservers mDefaultObservers;

    private int mSystemVisibilityToRestore;
    private boolean mIsTabListOpen;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            mIsTabListOpen = savedInstanceState.getBoolean("isTabListOpen");
        }
        setContentView(R.layout.main);

        mApplication = (WebEngineShellApplication) getApplication();
        mContext = getApplicationContext();
        mDefaultObservers = new DefaultObservers();

        setupActivitySpinner((Spinner) findViewById(R.id.activity_nav), this, 0);
        mProgressBar = findViewById(R.id.progress_bar);
        mUrlBar = findViewById(R.id.url_bar);
        mTabCountButton = findViewById(R.id.tab_count);
        mTabListSpinner = findViewById(R.id.tab_list);

        mReloadButton = findViewById(R.id.reload_button);
        mRefreshDrawable = getDrawable(R.drawable.ic_refresh);
        mStopDrawable = getDrawable(R.drawable.ic_stop);
        setProgress(1.0);

        mUrlBar.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                Uri query = Uri.parse(v.getText().toString());
                if (query.isAbsolute()) {
                    mTabManager.getActiveTab().getNavigationController().navigate(
                            query.normalizeScheme().toString());
                } else if (Patterns.DOMAIN_NAME.matcher(query.toString()).matches()) {
                    mTabManager.getActiveTab().getNavigationController().navigate(
                            "https://" + query);
                } else {
                    mTabManager.getActiveTab().getNavigationController().navigate(
                            "https://www.google.com/search?q="
                            + Uri.encode(v.getText().toString()));
                }
                // Hides keyboard on Enter key pressed
                InputMethodManager imm = (InputMethodManager) mContext.getSystemService(
                        Context.INPUT_METHOD_SERVICE);
                imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
                return true;
            }
        });

        mReloadButton.setOnClickListener(v -> {
            if (mReloadButton.getDrawable().equals(mRefreshDrawable)) {
                mTabManager.getActiveTab().getNavigationController().reload();
            } else if (mReloadButton.getDrawable().equals(mStopDrawable)) {
                mTabManager.getActiveTab().getNavigationController().stop();
            }
        });

        mTabCountButton.setOnClickListener(v -> mTabListSpinner.performClick());

        mTabListSpinner.setOnItemSelectedListener(new OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                mTabListAdapter.getItem(pos).getTab().setActive();
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });

        ListenableFuture<String> sandboxVersionFuture = WebSandbox.getVersion(mContext);
        Futures.addCallback(sandboxVersionFuture, new FutureCallback<String>() {
            @Override
            public void onSuccess(String version) {
                ((TextView) findViewById(R.id.version)).setText(version);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, ContextCompat.getMainExecutor(mContext));

        Futures.addCallback(mApplication.getWebEngine(), new FutureCallback<WebEngine>() {
            @Override
            public void onSuccess(WebEngine webEngine) {
                onWebEngineReady(webEngine);
            }

            @Override
            public void onFailure(Throwable thrown) {
                Toast.makeText(mContext, "Failed to start WebEngine.", Toast.LENGTH_LONG).show();
            }
        }, ContextCompat.getMainExecutor(mContext));

        Futures.addCallback(
                mApplication.getTabEventsDelegate(), new FutureCallback<TabEventsDelegate>() {
                    @Override
                    public void onSuccess(TabEventsDelegate tabEventsDelegate) {
                        mTabEventsDelegate = tabEventsDelegate;
                        tabEventsDelegate.registerObserver(WebEngineShellActivity.this);
                    }

                    @Override
                    public void onFailure(Throwable thrown) {}
                }, ContextCompat.getMainExecutor(mContext));
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        outState.putBoolean("isTabListOpen", mTabListSpinner.isOpen());
        super.onSaveInstanceState(outState);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mTabManager == null) return;
        for (Tab tab : mTabManager.getAllTabs()) {
            tab.setFullscreenCallback(null);
        }
        if (mTabEventsDelegate != null) mTabEventsDelegate.unregisterObserver();
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
                    case 3:
                        intent = new Intent(activity, WebEngineSinglePageActivity.class);
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
    public void onEnterFullscreen(WebEngine webEngine, Tab tab, FullscreenClient fullscreenClient) {
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

    private void onWebEngineReady(WebEngine webEngine) {
        mTabManager = webEngine.getTabManager();

        CookieManager cookieManager = webEngine.getCookieManager();
        Tab activeTab = mTabManager.getActiveTab();

        mTabCountButton.setText(String.valueOf(getTabsCount()));
        mTabListAdapter = new ArrayAdapter<TabWrapper>(
                mContext, android.R.layout.simple_spinner_dropdown_item);
        mTabListSpinner.setAdapter(mTabListAdapter);

        for (Tab t : mTabManager.getAllTabs()) {
            TabWrapper tabWrapper = new TabWrapper(t);
            mTabListAdapter.add(tabWrapper);
            if (t.equals(mTabManager.getActiveTab())) {
                mTabListSpinner.setSelection(mTabListAdapter.getPosition(tabWrapper));
            }
        }

        if (mIsTabListOpen) {
            mTabListSpinner.performClick();
        }

        for (Tab tab : mTabManager.getAllTabs()) {
            tab.setFullscreenCallback(WebEngineShellActivity.this);
        }

        if (activeTab.getDisplayUri().toString().equals("")) {
            mTabManager.registerTabListObserver(new TabListObserver() {
                @Override
                public void onTabAdded(@NonNull WebEngine webEngine, @NonNull Tab tab) {
                    tab.setFullscreenCallback(WebEngineShellActivity.this);
                }
            });
            activeTab.registerTabObserver(mDefaultObservers);
            activeTab.getNavigationController().registerNavigationObserver(mDefaultObservers);

            activeTab.getNavigationController().registerNavigationObserver(
                    new NavigationObserver() {
                        @Override
                        public void onNavigationCompleted(
                                @NonNull Tab tab, @NonNull Navigation navigation) {
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
        }
        if (getSupportFragmentManager().findFragmentByTag(WEB_FRAGMENT_TAG) == null) {
            getSupportFragmentManager()
                    .beginTransaction()
                    .setReorderingAllowed(true)
                    .add(R.id.fragment_container_view, webEngine.getFragment(), WEB_FRAGMENT_TAG)
                    .commit();
        }
    }

    int getTabsCount() {
        if (mTabManager == null) {
            return 0;
        }
        return mTabManager.getAllTabs().size();
    }

    void setProgress(double progress) {
        int progressValue = (int) Math.rint(progress * 100);
        if (progressValue != mProgressBar.getMax()) {
            mReloadButton.setImageDrawable(mStopDrawable);
            mProgressBar.setVisibility(View.VISIBLE);
        } else {
            mReloadButton.setImageDrawable(mRefreshDrawable);
            mProgressBar.setVisibility(View.INVISIBLE);
        }
        mProgressBar.setProgress(progressValue);
    }

    @Override
    public void onVisibleUriChanged(String uri) {
        mUrlBar.setText(uri);
    }

    @Override
    public void onActiveTabChanged(Tab activeTab) {
        mUrlBar.setText(activeTab.getDisplayUri().toString());
        for (int position = 0; position < mTabListAdapter.getCount(); ++position) {
            TabWrapper tabWrapper = mTabListAdapter.getItem(position);
            if (tabWrapper.getTab().equals(activeTab)) {
                mTabListSpinner.setSelection(position);
                return;
            }
        }
    }

    @Override
    public void onTabAdded(Tab tab) {
        mTabCountButton.setText(String.valueOf(getTabsCount()));
        mTabListAdapter.add(new TabWrapper(tab));
    }

    @Override
    public void onTabRemoved(Tab tab) {
        mTabCountButton.setText(String.valueOf(getTabsCount()));
        for (int position = 0; position < mTabListAdapter.getCount(); ++position) {
            TabWrapper tabAdapter = mTabListAdapter.getItem(position);
            if (tabAdapter.getTab().equals(tab)) {
                mTabListAdapter.remove(tabAdapter);
                return;
            }
        }
    }

    @Override
    public void onLoadProgressChanged(double progress) {
        setProgress(progress);
    }

    static class TabWrapper {
        final Tab mTab;
        public TabWrapper(Tab tab) {
            mTab = tab;
        }

        public Tab getTab() {
            return mTab;
        }

        @NonNull
        @Override
        public String toString() {
            return mTab.getDisplayUri().getAuthority() + mTab.getDisplayUri().getPath();
        }
    }
}
