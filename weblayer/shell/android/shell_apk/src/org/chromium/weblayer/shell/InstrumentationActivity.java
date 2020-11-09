// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy;
import android.os.StrictMode.VmPolicy;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.base.ContextUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.FullscreenCallback;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.NewTabType;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.UrlBarOptions;
import org.chromium.weblayer.WebLayer;

import java.util.ArrayList;
import java.util.List;

/**
 * Activity for running instrumentation tests.
 */
public class InstrumentationActivity extends FragmentActivity {
    private static final String TAG = "WLInstrumentation";
    private static final String KEY_MAIN_VIEW_ID = "mainViewId";

    public static final String EXTRA_PERSISTENCE_ID = "EXTRA_PERSISTENCE_ID";
    public static final String EXTRA_PROFILE_NAME = "EXTRA_PROFILE_NAME";
    public static final String EXTRA_IS_INCOGNITO = "EXTRA_IS_INCOGNITO";
    private static final float DEFAULT_TEXT_SIZE = 15.0F;

    // Used in tests to specify whether WebLayer should be created automatically on launch.
    // True by default. If set to false, the test should call loadWebLayerSync.
    public static final String EXTRA_CREATE_WEBLAYER = "EXTRA_CREATE_WEBLAYER";

    // Used in tests to specify whether WebLayer URL bar should set default click listeners
    // that show Page Info UI on its TextView.
    public static final String EXTRA_URLBAR_TEXT_CLICKABLE = "EXTRA_URLBAR_TEXT_CLICKABLE";

    private static OnCreatedCallback sOnCreatedCallback;

    // If true, multiple fragments may be created. Only the first is attached. This is useful for
    // tests that need to create multiple BrowserFragments.
    public static boolean sAllowMultipleFragments;

    private Profile mProfile;
    private Fragment mFragment;
    private Browser mBrowser;
    private Tab mTab;
    private View mMainView;
    private int mMainViewId;
    private ViewGroup mTopContentsContainer;
    private View mUrlBarView;
    private IntentInterceptor mIntentInterceptor;
    private Bundle mSavedInstanceState;
    private TabCallback mRendererCrashListener;
    private Runnable mExitFullscreenRunnable;
    private boolean mIgnoreRendererCrashes;
    private TabListCallback mTabListCallback;
    private List<Tab> mPreviousTabList = new ArrayList<>();

    private static boolean isJaCoCoEnabled() {
        // Nothing is set at runtime indicating jacoco is being used. This looks for the existence
        // of a javacoco class to determine if jacoco is enabled.
        try {
            Class.forName("org.jacoco.agent.rt.RT");
            return true;
        } catch (LinkageError | ClassNotFoundException e) {
        }
        return false;
    }

    /**
     * Use this callback for tests that need to be notified synchronously when the Browser has been
     * created.
     */
    public static interface OnCreatedCallback {
        // Notification that a Browser was created.
        // This is called on the UI thread.
        public void onCreated(Browser browser);
    }

    // Registers a callback that is notified on the UI thread when a Browser is created.
    public static void registerOnCreatedCallback(OnCreatedCallback callback) {
        sOnCreatedCallback = callback;
        // Ideally |callback| would be registered in the Intent, but that isn't possible as to do so
        // |callback| would have to be a Parceable (which doesn't make sense). As at this time each
        // test runs in its own process a static is used, if multiple tests were to run in the same
        // binary, then some state would need to be put in the intent.
    }

    public Tab getTab() {
        return mTab;
    }

    public Fragment getFragment() {
        return mFragment;
    }

    public Browser getBrowser() {
        return mBrowser;
    }

    /**
     * Explicitly destroys the fragment. There is normally no need to call this. It's useful for
     * tests that want to verify destruction.
     */
    public void destroyFragment() {
        removeCallbacks();

        FragmentManager fragmentManager = getSupportFragmentManager();
        FragmentTransaction transaction = fragmentManager.beginTransaction();
        transaction.remove(mFragment);
        transaction.commitNow();
        mFragment = null;
        mBrowser = null;
    }

    /** Interface used to intercept intents for testing. */
    public static interface IntentInterceptor {
        void interceptIntent(Fragment fragment, Intent intent, int requestCode, Bundle options);
    }

    public void setIntentInterceptor(IntentInterceptor interceptor) {
        mIntentInterceptor = interceptor;
    }

    @Override
    public void startActivityFromFragment(
            Fragment fragment, Intent intent, int requestCode, Bundle options) {
        if (mIntentInterceptor != null) {
            mIntentInterceptor.interceptIntent(fragment, intent, requestCode, options);
            return;
        }
        super.startActivityFromFragment(fragment, intent, requestCode, options);
    }

    @Override
    public void startActivity(Intent intent) {
        if (mIntentInterceptor != null) {
            mIntentInterceptor.interceptIntent(null, intent, 0, null);
            return;
        }
        super.startActivity(intent);
    }

    @Override
    public boolean startActivityIfNeeded(Intent intent, int requestCode) {
        if (mIntentInterceptor != null) {
            mIntentInterceptor.interceptIntent(null, intent, requestCode, null);
            return true;
        }
        return super.startActivityIfNeeded(intent, requestCode);
    }

    public View getTopContentsContainer() {
        return mTopContentsContainer;
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        // JaCoCo injects code that does file access, which doesn't work well with strict mode.
        if (!isJaCoCoEnabled()) {
            StrictMode.setThreadPolicy(
                    new ThreadPolicy.Builder().detectAll().penaltyLog().penaltyDeath().build());
            // This doesn't use detectAll() as the untagged sockets policy is encountered in tests
            // using TestServer.
            StrictMode.setVmPolicy(new VmPolicy.Builder()
                                           .detectLeakedSqlLiteObjects()
                                           .detectLeakedClosableObjects()
                                           .penaltyLog()
                                           .penaltyDeath()
                                           .build());
        }
        super.onCreate(savedInstanceState);
        mSavedInstanceState = savedInstanceState;
        LinearLayout mainView = new LinearLayout(this);
        if (savedInstanceState == null) {
            mMainViewId = View.generateViewId();
        } else {
            mMainViewId = savedInstanceState.getInt(KEY_MAIN_VIEW_ID);
        }
        mainView.setId(mMainViewId);
        mMainView = mainView;
        setContentView(mainView);

        // The progress bar sits above the URL bar in Z order and at its bottom in Y.
        mTopContentsContainer = new RelativeLayout(this);

        if (getIntent().getBooleanExtra(EXTRA_CREATE_WEBLAYER, true)) {
            // If activity is re-created during process restart, FragmentManager attaches
            // BrowserFragment immediately, resulting in synchronous init. By the time this line
            // executes, the synchronous init has already happened, so WebLayer#createAsync will
            // deliver WebLayer instance to callbacks immediately.
            createWebLayerAsync();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // When restoring Fragments, FragmentManager tries to put them in the containers with same
        // ids as before.
        outState.putInt(KEY_MAIN_VIEW_ID, mMainViewId);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        removeCallbacks();
    }

    private void removeCallbacks() {
        if (mBrowser != null && mRendererCrashListener != null) {
            for (Tab tab : mBrowser.getTabs()) {
                tab.unregisterTabCallback(mRendererCrashListener);
            }
        }
        if (mTabListCallback != null) {
            mBrowser.unregisterTabListCallback(mTabListCallback);
            mTabListCallback = null;
        }
    }

    private void createWebLayerAsync() {
        try {
            // Get the Context from ContextUtils so tests get the wrapped version.
            WebLayer.loadAsync(ContextUtils.getApplicationContext(), webLayer -> onWebLayerReady());
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    public WebLayer loadWebLayerSync(Context appContext) {
        try {
            WebLayer webLayer = WebLayer.loadSync(appContext);
            onWebLayerReady();
            return webLayer;
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    private void onWebLayerReady() {
        if (mBrowser != null || isFinishing() || isDestroyed()) return;

        mFragment = getOrCreateBrowserFragment();
        mBrowser = Browser.fromFragment(mFragment);
        mProfile = mBrowser.getProfile();

        mBrowser.setTopView(mTopContentsContainer);

        mRendererCrashListener = new TabCallback() {
            @Override
            public void onRenderProcessGone() {
                if (mIgnoreRendererCrashes) return;

                // Throws an exception if a tab crashes. Otherwise tests might pass while ignoring
                // renderer crashes.
                throw new RuntimeException("Unexpected renderer crashed");
            }
        };

        mTabListCallback = new TabListCallback() {
            @Override
            public void onTabAdded(Tab tab) {
                // The first tab can be added asynchronously with session restore enabled.
                if (mTab == null) {
                    setTab(tab);
                }
                setTabCallbacks(tab);
            }

            @Override
            public void onTabRemoved(Tab tab) {
                mPreviousTabList.remove(tab);

                if (mTab == tab) {
                    Tab prevTab = null;
                    if (!mPreviousTabList.isEmpty()) {
                        prevTab = mPreviousTabList.remove(mPreviousTabList.size() - 1);
                    }

                    setTab(prevTab);
                }
                tab.unregisterTabCallback(mRendererCrashListener);
            }
        };

        mBrowser.registerTabListCallback(mTabListCallback);

        if (mBrowser.getActiveTab() == null) {
            // This happens with session restore enabled.
            assert mBrowser.getTabs().size() == 0;
        } else {
            setTabCallbacks(mBrowser.getActiveTab());
            setTab(mBrowser.getActiveTab());
        }

        if (sOnCreatedCallback != null) {
            sOnCreatedCallback.onCreated(mBrowser);
            // Don't reset |sOnCreatedCallback| as it's needed for tests that exercise activity
            // recreation.
        }
    }

    private void setTabCallbacks(Tab tab) {
        tab.registerTabCallback(mRendererCrashListener);

        tab.setFullscreenCallback(new FullscreenCallback() {
            private int mSystemVisibilityToRestore;

            @Override
            public void onEnterFullscreen(Runnable exitFullscreenRunnable) {
                mExitFullscreenRunnable = exitFullscreenRunnable;
                // This comes from Chrome code to avoid an extra resize.
                final WindowManager.LayoutParams attrs = getWindow().getAttributes();
                attrs.flags |= WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
                getWindow().setAttributes(attrs);

                View decorView = getWindow().getDecorView();
                // Caching the system ui visibility is ok for shell, but likely not ok for
                // real code.
                mSystemVisibilityToRestore = decorView.getSystemUiVisibility();
                decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
                        | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
                        | View.SYSTEM_UI_FLAG_LOW_PROFILE | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            }

            @Override
            public void onExitFullscreen() {
                mExitFullscreenRunnable = null;
                View decorView = getWindow().getDecorView();
                decorView.setSystemUiVisibility(mSystemVisibilityToRestore);

                final WindowManager.LayoutParams attrs = getWindow().getAttributes();
                if ((attrs.flags & WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS) != 0) {
                    attrs.flags &= ~WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
                    getWindow().setAttributes(attrs);
                }
            }
        });
    }

    private void createUrlBarView() {
        UrlBarOptions.Builder optionsBuilder = UrlBarOptions.builder()
                                                       .setTextSizeSP(DEFAULT_TEXT_SIZE)
                                                       .setTextColor(android.R.color.black)
                                                       .setIconColor(android.R.color.black);
        if (getIntent().getBooleanExtra(EXTRA_URLBAR_TEXT_CLICKABLE, true)) {
            optionsBuilder = optionsBuilder.showPageInfoWhenTextIsClicked();
        }

        mUrlBarView = mBrowser.getUrlBarController().createUrlBarView(optionsBuilder.build());

        // The background of the top-view must be opaque, otherwise it bleeds through to the
        // cc::Layer that mirrors the contents of the top-view.
        mUrlBarView.setBackgroundColor(0xFFa9a9a9);

        mTopContentsContainer.removeAllViews();
        mTopContentsContainer.addView(mUrlBarView,
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
    }

    // Clears the state associated with |mTab| and sets |tab|, if non-null, as |mTab| and the
    // active tab in the browser.
    private void setTab(Tab tab) {
        if (mTab != null) {
            mTab = null;
        }

        mTab = tab;
        if (mTab == null) return;

        mTab.setNewTabCallback(new NewTabCallback() {
            @Override
            public void onNewTab(Tab newTab, @NewTabType int type) {
                mPreviousTabList.add(mTab);
                setTab(newTab);
            }
        });

        // Creates and adds a new UrlBarView to |mTopContentsContainer|.
        createUrlBarView();

        // Will be a no-op if this tab is already the active tab.
        mBrowser.setActiveTab(mTab);
    }

    private Fragment getOrCreateBrowserFragment() {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (mSavedInstanceState != null) {
            // FragmentManager could have re-created the fragment.
            List<Fragment> fragments = fragmentManager.getFragments();
            if (fragments.size() > 1) {
                if (!sAllowMultipleFragments) {
                    throw new IllegalStateException(
                            "More than one fragment added, shouldn't happen");
                }
                if (sOnCreatedCallback != null) {
                    for (int i = 1; i < fragments.size(); ++i) {
                        sOnCreatedCallback.onCreated(Browser.fromFragment(fragments.get(i)));
                    }
                }
                return fragments.get(0);
            }
            if (fragments.size() > 0) {
                return fragments.get(0);
            }
        }
        return createBrowserFragment(mMainViewId);
    }

    public Fragment createBrowserFragment(int viewId) {
        return createBrowserFragment(viewId, getIntent());
    }

    public Fragment createBrowserFragment(int viewId, Intent intent) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        String profileName = intent.hasExtra(EXTRA_PROFILE_NAME)
                ? intent.getStringExtra(EXTRA_PROFILE_NAME)
                : "DefaultProfile";
        String persistenceId = intent.hasExtra(EXTRA_PERSISTENCE_ID)
                ? intent.getStringExtra(EXTRA_PERSISTENCE_ID)
                : null;
        boolean incognito = intent.hasExtra(EXTRA_IS_INCOGNITO)
                ? intent.getBooleanExtra(EXTRA_IS_INCOGNITO, false)
                : (profileName == null);
        Fragment fragment = incognito
                ? WebLayer.createBrowserFragmentWithIncognitoProfile(profileName, persistenceId)
                : WebLayer.createBrowserFragment(profileName, persistenceId);
        FragmentTransaction transaction = fragmentManager.beginTransaction();
        transaction.add(viewId, fragment);

        // Note the commitNow() instead of commit(). We want the fragment to get attached to
        // activity synchronously, so we can use all the functionality immediately. Otherwise we'd
        // have to wait until the commit is executed.
        transaction.commitNow();

        if (viewId != mMainViewId && sOnCreatedCallback != null) {
            sOnCreatedCallback.onCreated(Browser.fromFragment(fragment));
        }

        return fragment;
    }

    public void loadUrl(String url) {
        mTab.getNavigationController().navigate(Uri.parse(url));
    }

    public void setRetainInstance(boolean retain) {
        mFragment.setRetainInstance(retain);
    }

    public View getUrlBarView() {
        return mUrlBarView;
    }

    public void setIgnoreRendererCrashes() {
        mIgnoreRendererCrashes = true;
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
