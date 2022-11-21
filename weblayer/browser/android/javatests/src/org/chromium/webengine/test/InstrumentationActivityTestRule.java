// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import android.content.ComponentName;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import org.junit.Rule;

import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.shell.InstrumentationActivity;

/**
 * ActivityTestRule for InstrumentationActivity.
 *
 * Test can use this ActivityTestRule to launch or get InstrumentationActivity.
 */
public class InstrumentationActivityTestRule
        extends WebEngineActivityTestRule<InstrumentationActivity> {
    @Rule
    private EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    public InstrumentationActivityTestRule() {
        super(InstrumentationActivity.class);
    }

    /**
     * Starts the WebEngine Instrumentation activity.
     */
    public InstrumentationActivity launchShell() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(
                new ComponentName(InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        InstrumentationActivity.class));
        launchActivity(intent);
        return getActivity();
    }

    public EmbeddedTestServer getTestServer() {
        return mTestServerRule.getServer();
    }

    public String getTestDataURL(String path) {
        return getTestServer().getURL("/weblayer/test/data/" + path);
    }
}
