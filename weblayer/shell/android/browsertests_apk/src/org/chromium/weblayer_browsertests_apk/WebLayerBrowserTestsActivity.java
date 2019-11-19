// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_browsertests_apk;

import android.net.Uri;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentTransaction;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.native_test.NativeBrowserTest;
import org.chromium.native_test.NativeBrowserTestActivity;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.WebLayer;

import java.io.File;

/** An Activity base class for running browser tests against WebLayerShell. */
public class WebLayerBrowserTestsActivity extends NativeBrowserTestActivity {
    private static final String TAG = "native_test";

    private WebLayer mWebLayer;
    private Profile mProfile;
    private Browser mBrowser;
    private Tab mTab;
    private EditText mUrlView;
    private View mMainView;

    @Override
    protected void initializeBrowserProcess() {
        BrowserStartupController.get(LibraryProcessType.PROCESS_WEBLAYER)
                .setContentMainCallbackForTests(() -> {
                    // This jumps into C++ to set up and run the test harness. The test harness runs
                    // ContentMain()-equivalent code, and then waits for javaStartupTasksComplete()
                    // to be called.
                    runTests();
                });

        try {
            WebLayer.create(getApplication()).addCallback((WebLayer webLayer) -> {
                mWebLayer = webLayer;
                createShell();
            });
        } catch (Exception e) {
            throw new RuntimeException("failed loading WebLayer", e);
        }

        NativeBrowserTest.javaStartupTasksComplete();
    }

    protected void createShell() {
        LinearLayout mainView = new LinearLayout(this);
        int viewId = View.generateViewId();
        mainView.setId(viewId);
        mMainView = mainView;
        setContentView(mainView);

        mUrlView = new EditText(this);
        mUrlView.setId(View.generateViewId());
        // The background of the top-view must be opaque, otherwise it bleeds through to the
        // cc::Layer that mirrors the contents of the top-view.
        mUrlView.setBackgroundColor(0xFFa9a9a9);

        RelativeLayout topContentsContainer = new RelativeLayout(this);
        topContentsContainer.addView(mUrlView,
                new RelativeLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        Fragment fragment = WebLayer.createBrowserFragment(null);

        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();
        transaction.add(viewId, fragment);
        transaction.commitNow();

        mBrowser = Browser.fromFragment(fragment);
        mProfile = mBrowser.getProfile();
        mBrowser.setTopView(topContentsContainer);

        mTab = mBrowser.getActiveTab();
        mTab.registerTabCallback(new TabCallback() {
            @Override
            public void onVisibleUrlChanged(Uri uri) {
                mUrlView.setText(uri.toString());
            }
        });
    }

    @Override
    protected File getPrivateDataDirectory() {
        return new File(UrlUtils.getIsolatedTestRoot(),
                WebLayerBrowserTestsApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
    }
}
