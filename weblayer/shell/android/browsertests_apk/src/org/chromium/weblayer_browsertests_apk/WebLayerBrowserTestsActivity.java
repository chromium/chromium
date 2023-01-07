// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_browsertests_apk;

import android.net.Uri;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.native_test.NativeBrowserTest;
import org.chromium.native_test.NativeBrowserTestActivity;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.NewTabType;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TestWebLayer;
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
        BrowserStartupController.getInstance().setContentMainCallbackForTests(() -> {
            // This jumps into C++ to set up and run the test harness. The test harness runs
            // ContentMain()-equivalent code, and then waits for javaStartupTasksComplete()
            // to be called.
            runTests();
        });

        try {
            // Browser tests cannot be run in WebView compatibility mode since the class loader
            // WebLayer uses needs to match the class loader used for setup.
            TestWebLayer.disableWebViewCompatibilityMode();
            WebLayer.loadAsync(getApplication(), webLayer -> {
                mWebLayer = webLayer;
                createShell();

                NativeBrowserTest.javaStartupTasksComplete();
            });
        } catch (Exception e) {
            throw new RuntimeException("failed loading WebLayer", e);
        }
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

        CommandLine commandLine = CommandLine.getInstance();
        String path = (commandLine.hasSwitch("start-in-incognito")) ? null : "BrowserTestProfile";

        Fragment fragment = WebLayer.createBrowserFragment(path);

        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();
        transaction.add(viewId, fragment);
        transaction.commitNow();

        mBrowser = Browser.fromFragment(fragment);
        mProfile = mBrowser.getProfile();
        mBrowser.setTopView(topContentsContainer);

        mTab = mBrowser.getActiveTab();
        mTab.registerTabCallback(new TabCallback() {
            @Override
            public void onVisibleUriChanged(Uri uri) {
                mUrlView.setText(uri.toString());
            }
        });
        // Set a new tab callback to make sure popups are added.
        mTab.setNewTabCallback(new NewTabCallback() {
            @Override
            public void onNewTab(Tab tab, @NewTabType int type) {}

            @Override
            public void onCloseTab() {}
        });
    }

    @Override
    protected File getPrivateDataDirectory() {
        return new File(UrlUtils.getIsolatedTestRoot(),
                WebLayerBrowserTestsApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
    }

    @Override
    /**
     * Ensure that the user data directory gets overridden to getPrivateDataDirectory() (which is
     * cleared at the start of every run); the directory that ANDROID_APP_DATA_DIR is set to in the
     * context of Java browsertests is not cleared as it also holds persistent state, which
     * causes test failures due to state bleedthrough. See crbug.com/617734 for details.
     */
    protected String getUserDataDirectoryCommandLineSwitch() {
        return "weblayer-user-data-dir";
    }
}
