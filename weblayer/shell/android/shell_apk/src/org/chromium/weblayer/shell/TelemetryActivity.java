// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.net.Uri;
import android.os.Bundle;
import android.os.Trace;
import android.text.InputType;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.WebLayer;

import java.util.List;

/**
 * Activity for running performance tests.
 *
 * There's no visible chrome, just content.
 */
public class TelemetryActivity extends FragmentActivity {
    private static final String KEY_MAIN_VIEW_ID = "mainViewId";

    private static final String START_UP_TRACE_TAG = "WebLayerStartupInterval";
    private static final String LOAD_URL_TRACE_TAG = "WebLayerBlankUrlLoadInterval";
    private static final String DUMMY_TRACE_TAG = "WebLayerDummyInterval";

    private Profile mProfile;
    private Fragment mFragment;
    private Browser mBrowser;
    private Tab mTab;
    private EditText mUrlView;
    private View mMainView;
    private int mMainViewId;
    private ViewGroup mTopContentsContainer;
    private Bundle mSavedInstanceState;
    private TabCallback mTabCallback;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
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

        mUrlView = new EditText(this);
        mUrlView.setId(View.generateViewId());
        mUrlView.setSelectAllOnFocus(true);
        mUrlView.setInputType(InputType.TYPE_TEXT_VARIATION_URI);
        mUrlView.setImeOptions(EditorInfo.IME_ACTION_GO);
        // The background of the top-view must be opaque, otherwise it bleeds through to the
        // cc::Layer that mirrors the contents of the top-view.
        mUrlView.setBackgroundColor(0xFFa9a9a9);

        // Top contents container is zero height so changes to the UI don't affect viewport size and
        // performance.
        mTopContentsContainer = new RelativeLayout(this);
        mTopContentsContainer.addView(
                mUrlView, new RelativeLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0));

        Trace.beginSection(START_UP_TRACE_TAG);

        // TODO(aluo): Use async tracing to avoid having to do this
        // dummyTraceTag is needed here to prevent code in Android intended to
        // end activityStart from ending loadUrlTraceTag prematurely,
        // see crbug/919221
        Trace.beginSection(DUMMY_TRACE_TAG);

        // If activity is re-created during process restart, FragmentManager attaches
        // BrowserFragment immediately, resulting in synchronous init. By the time this line
        // executes, the synchronous init has already happened, so WebLayer#createAsync will
        // deliver WebLayer instance to callbacks immediately.
        createWebLayerAsync();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mTabCallback != null) {
            mTab.unregisterTabCallback(mTabCallback);
            mTabCallback = null;
        }
    }

    private void createWebLayerAsync() {
        try {
            WebLayer.loadAsync(getApplicationContext(), webLayer -> onWebLayerReady(webLayer));
        } catch (UnsupportedVersionException e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    private void onWebLayerReady(WebLayer webLayer) {
        // Ends START_UP_TRACE_TAG
        Trace.endSection();
        // Ends activityStart
        Trace.endSection();

        if (mBrowser != null || isFinishing() || isDestroyed()) return;

        webLayer.setRemoteDebuggingEnabled(true);

        mFragment = getOrCreateBrowserFragment();
        mBrowser = Browser.fromFragment(mFragment);
        mProfile = mBrowser.getProfile();

        mBrowser.setTopView(mTopContentsContainer);
        setTab(mBrowser.getActiveTab());

        Trace.beginSection(LOAD_URL_TRACE_TAG);
        mTab.getNavigationController().registerNavigationCallback(new NavigationCallback() {
            @Override
            public void onNavigationCompleted(Navigation navigation) {
                // Ends LOAD_URL_TRACE_TAG
                Trace.endSection();
            }
        });
        if (getIntent() != null) {
            mTab.getNavigationController().navigate(Uri.parse(getIntent().getDataString()));
        }
    }

    private void setTab(Tab tab) {
        assert mTab == null;
        mTab = tab;
        mTabCallback = new TabCallback() {
            @Override
            public void onVisibleUriChanged(Uri uri) {
                mUrlView.setText(uri.toString());
            }
        };
        mTab.registerTabCallback(mTabCallback);
    }

    private Fragment getOrCreateBrowserFragment() {
        FragmentManager fragmentManager = getSupportFragmentManager();
        if (mSavedInstanceState != null) {
            // FragmentManager could have re-created the fragment.
            List<Fragment> fragments = fragmentManager.getFragments();
            if (fragments.size() > 1) {
                throw new IllegalStateException("More than one fragment added, shouldn't happen");
            }
            if (fragments.size() == 1) {
                return fragments.get(0);
            }
        }
        return createBrowserFragment(mMainViewId);
    }

    private Fragment createBrowserFragment(int viewId) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        Fragment fragment = WebLayer.createBrowserFragment("DefaultProfile", null);
        FragmentTransaction transaction = fragmentManager.beginTransaction();
        transaction.add(viewId, fragment);

        // Note the commitNow() instead of commit(). We want the fragment to get attached to
        // activity synchronously, so we can use all the functionality immediately. Otherwise we'd
        // have to wait until the commit is executed.
        transaction.commitNow();
        return fragment;
    }
}
