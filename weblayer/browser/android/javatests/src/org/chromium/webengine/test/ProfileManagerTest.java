// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.webengine.ProfileManager;
import org.chromium.webengine.WebSandbox;

/**
 * Tests functions called on the ProfileManager object.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class ProfileManagerTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    WebSandbox mSandbox;
    ProfileManager mProfileManager;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();

        mSandbox = mActivityTestRule.getWebSandbox();
        mProfileManager = mSandbox.getProfileManager();
    }

    @After
    public void shutdown() throws Exception {
        if (mSandbox != null) {
            runOnUiThreadBlocking(() -> mSandbox.shutdown());
        }
        mActivityTestRule.finish();
    }

    @Test
    @SmallTest
    public void profileManagerIsAvailable() throws Exception {
        Assert.assertNotNull(mProfileManager);
    }
}
