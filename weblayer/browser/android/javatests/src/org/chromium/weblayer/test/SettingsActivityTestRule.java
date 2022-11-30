// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.SettingsActivity;
import org.chromium.weblayer.WebLayer;

/**
 * ActivityTestRule for SettingsActivity.
 *
 * Test can use this ActivityTestRule to launch SettingsActivity.
 */
public class SettingsActivityTestRule extends WebLayerActivityTestRule<SettingsActivity> {
    private Context mAppContext;

    public SettingsActivityTestRule() {
        super(SettingsActivity.class);
    }

    @Override
    public void launchActivity(Intent settingsIntent) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // We load WebLayer here so it gets initialized with the test/instrumentation Context,
            // which has an in-memory SharedPreferences. Without this call, WebLayer gets
            // initialized with the Application as its appContext, which breaks tests because a
            // SharedPreferences xml file is persisted to disk.
            WebLayer.loadSync(getContext());
        });
        super.launchActivity(settingsIntent);
    }

    public Context getContext() {
        if (mAppContext == null) {
            mAppContext = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();
        }
        return mAppContext;
    }
}
