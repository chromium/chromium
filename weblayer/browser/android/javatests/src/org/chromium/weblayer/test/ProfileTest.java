// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.io.File;
import java.util.Collection;

/**
 * Tests that Profile works as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ProfileTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void testCreateAndGetAllProfiles() {
        WebLayer weblayer = mActivityTestRule.getWebLayer();
        {
            // Start with empty profile.
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertTrue(profiles.isEmpty());
        }

        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        Profile firstProfile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowser().getProfile());
        {
            // Launching an activity with a fragment creates one profile.
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertEquals(1, profiles.size());
            Assert.assertTrue(profiles.contains(firstProfile));
        }

        Profile secondProfile = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            String profilePath = new File(activity.getFilesDir(), "second_test").getPath();
            return weblayer.getProfile(profilePath);
        });

        {
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertEquals(2, profiles.size());
            Assert.assertTrue(profiles.contains(firstProfile));
            Assert.assertTrue(profiles.contains(secondProfile));
        }
    }

    private static Collection<Profile> getAllProfiles() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> Profile.getAllProfiles());
    }
}
