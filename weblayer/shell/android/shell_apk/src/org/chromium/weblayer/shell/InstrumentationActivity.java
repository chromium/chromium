// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.text.InputType;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import org.chromium.weblayer.Browser;
import org.chromium.weblayer.ListenableFuture;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.WebLayer;

import java.io.File;
import java.util.List;

/**
 * Activity for running instrumentation tests.
 */
public class InstrumentationActivity extends FragmentActivity {
    private static final String TAG = "WLInstrumentation";

    public static final String EXTRA_PROFILE_NAME = "EXTRA_PROFILE_NAME";

    private Profile mProfile;
    private Browser mBrowser;
    private Tab mTab;
    private EditText mUrlView;
    private View mMainView;
    private int mMainViewId;
    private ViewGroup mTopContentsContainer;
    private IntentInterceptor mIntentInterceptor;

    public Tab getTab() {
        return mTab;
    }

    public Browser getBrowser() {
        return mBrowser;
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

    public View getTopContentsContainer() {
        return mTopContentsContainer;
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LinearLayout mainView = new LinearLayout(this);
        mMainViewId = View.generateViewId();
        mainView.setId(mMainViewId);
        mMainView = mainView;
        setContentView(mainView);

        mUrlView = new EditText(this);
        mUrlView.setId(View.generateViewId());
        mUrlView.setSelectAllOnFocus(true);
        mUrlView.setInputType(InputType.TYPE_TEXT_VARIATION_URI);
        mUrlView.setImeOptions(EditorInfo.IME_ACTION_GO);
        // The background of the top-view must be opaque, otherwise it bleeds through to the
        // cc::Layer that mirrors the contents of the top-view.
        mUrlView.setBackgroundColor(0xFFa9a9a9);

        // The progress bar sits above the URL bar in Z order and at its bottom in Y.
        mTopContentsContainer = new RelativeLayout(this);
        mTopContentsContainer.addView(mUrlView,
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        if (savedInstanceState != null) {
            // If activity is re-created during process restart, FragmentManager attaches
            // BrowserFragment immediately, resulting in synchronous init. By the time this line
            // executes, the synchronous init has already happened.
            createWebLayer(getApplication(), savedInstanceState);
        }
    }

    public ListenableFuture<WebLayer> createWebLayer(
            Context appContext, Bundle savedInstanceState) {
        try {
            ListenableFuture<WebLayer> future = WebLayer.create(appContext);
            future.addCallback(webLayer -> onWebLayerReady(savedInstanceState));
            return future;
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    private void onWebLayerReady(Bundle savedInstanceState) {
        if (isFinishing() || isDestroyed()) return;

        Fragment fragment = getOrCreateBrowserFragment(savedInstanceState);
        mBrowser = Browser.fromFragment(fragment);
        mProfile = mBrowser.getProfile();

        mBrowser.setTopView(mTopContentsContainer);

        mTab = mBrowser.getActiveTab();
        mTab.registerTabCallback(new TabCallback() {
            @Override
            public void onVisibleUrlChanged(Uri uri) {
                mUrlView.setText(uri.toString());
            }
        });
    }

    private Fragment getOrCreateBrowserFragment(Bundle savedInstanceState) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (savedInstanceState != null) {
            // FragmentManager could have re-created the fragment.
            List<Fragment> fragments = fragmentManager.getFragments();
            if (fragments.size() > 1) {
                throw new IllegalStateException("More than one fragment added, shouldn't happen");
            }
            if (fragments.size() == 1) {
                return fragments.get(0);
            }
        }
        return createBrowserFragment(mMainViewId, savedInstanceState);
    }

    public Fragment createBrowserFragment(int viewId, Bundle savedInstanceState) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        String profileName = getIntent().hasExtra(EXTRA_PROFILE_NAME)
                ? getIntent().getStringExtra(EXTRA_PROFILE_NAME)
                : "DefaultProfile";
        String profilePath = null;
        if (!TextUtils.isEmpty(profileName)) {
            profilePath = new File(getFilesDir(), profileName).getPath();
        } // else create an in-memory Profile.

        Fragment fragment = WebLayer.createBrowserFragment(profilePath);
        FragmentTransaction transaction = fragmentManager.beginTransaction();
        transaction.add(viewId, fragment);

        // Note the commitNow() instead of commit(). We want the fragment to get attached to
        // activity synchronously, so we can use all the functionality immediately. Otherwise we'd
        // have to wait until the commit is executed.
        transaction.commitNow();
        return fragment;
    }

    public void loadUrl(String url) {
        mTab.getNavigationController().navigate(Uri.parse(url));
        mUrlView.clearFocus();
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
