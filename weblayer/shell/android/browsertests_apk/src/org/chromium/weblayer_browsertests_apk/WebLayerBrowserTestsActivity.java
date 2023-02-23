// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_browsertests_apk;

import android.view.View;
import android.widget.LinearLayout;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.native_test.NativeBrowserTest;
import org.chromium.native_test.NativeBrowserTestActivity;
import org.chromium.weblayer.TestWebLayer;

import java.io.File;

/** An Activity base class for running browser tests against WebLayerShell. */
public class WebLayerBrowserTestsActivity extends NativeBrowserTestActivity {
    private static final String TAG = "native_test";

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
            TestWebLayer.setupWeblayerForBrowserTest(getApplication(), (contentView) -> {
                LinearLayout mainView = new LinearLayout(this);
                int viewId = View.generateViewId();
                mainView.setId(viewId);
                setContentView(mainView);

                mainView.addView(contentView);
                NativeBrowserTest.javaStartupTasksComplete();
            });
        } catch (Exception e) {
            throw new RuntimeException("failed loading WebLayer", e);
        }
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
        return "webengine-user-data-dir";
    }
}
