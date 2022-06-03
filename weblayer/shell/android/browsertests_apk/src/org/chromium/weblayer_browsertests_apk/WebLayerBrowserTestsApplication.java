// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_browsertests_apk;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.native_test.NativeBrowserTestApplication;

/**
 * A basic weblayer_public.browser.tests {@link android.app.Application}.
 */
public class WebLayerBrowserTestsApplication extends NativeBrowserTestApplication {
    static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "weblayer_shell";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);

        if (isBrowserProcess()) {
            // Test-only stuff, see also NativeUnitTest.java.
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        }
    }

    @Override
    protected void setLibraryProcessType() {}

    @Override
    protected void initApplicationContext() {
        // Matches the initApplicationContext call in WebLayerImpl.minimalInitForContext.
        ContextUtils.initApplicationContext(ClassLoaderContextWrapperFactory.get(this));
    }
}
