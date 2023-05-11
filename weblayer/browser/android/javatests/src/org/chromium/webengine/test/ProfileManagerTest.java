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
import org.chromium.webengine.WebEngineParams;
import org.chromium.webengine.WebSandbox;

import java.util.List;

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
    public void profileManagerIsAvailable() {
        Assert.assertNotNull(mProfileManager);
    }

    @Test
    @SmallTest
    public void getAllProfileNamesReturnsNames() throws Exception {
        String name1 = "TestProfile1";
        WebEngineParams params1 = new WebEngineParams.Builder().setProfileName(name1).build();
        runOnUiThreadBlocking(() -> mSandbox.createWebEngine(params1));

        List<String> initialNames =
                runOnUiThreadBlocking(() -> mProfileManager.getAllProfileNames()).get();
        Assert.assertEquals(1, initialNames.size());
        Assert.assertTrue(initialNames.contains(name1));

        String profileName2 = "TestProfile2";
        WebEngineParams params2 =
                new WebEngineParams.Builder().setProfileName(profileName2).build();
        runOnUiThreadBlocking(() -> mSandbox.createWebEngine(params2));

        List<String> newNames =
                runOnUiThreadBlocking(() -> mProfileManager.getAllProfileNames()).get();
        Assert.assertEquals(2, newNames.size());
        Assert.assertTrue(newNames.contains(profileName2));
    }

    @Test
    @SmallTest
    public void createProfileWithIdenticalName() throws Exception {
        String profileName = "TestProfile";
        WebEngineParams params = new WebEngineParams.Builder().setProfileName(profileName).build();
        runOnUiThreadBlocking(() -> mSandbox.createWebEngine(params, "tag-1"));
        runOnUiThreadBlocking(() -> mSandbox.createWebEngine(params, "tag-2"));

        List<String> names =
                runOnUiThreadBlocking(() -> mProfileManager.getAllProfileNames()).get();

        Assert.assertEquals(1, names.size());
        Assert.assertTrue(names.contains(profileName));
    }

    @Test
    @SmallTest
    public void noProfilesOnWebSandboxStartup() throws Exception {
        List<String> names =
                runOnUiThreadBlocking(() -> mProfileManager.getAllProfileNames()).get();

        Assert.assertEquals(0, names.size());
    }
}
