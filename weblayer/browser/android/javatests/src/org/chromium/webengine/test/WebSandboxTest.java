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
import org.chromium.webengine.WebSandbox;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Tests the WebSandbox API.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class WebSandboxTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();
    }

    @After
    public void shutdown() throws Exception {
        mActivityTestRule.finish();
    }

    @Test
    @SmallTest
    public void canStartSandbox() throws Exception {
        Assert.assertNotNull(mActivityTestRule.getWebSandbox());
    }

    @Test
    @SmallTest
    public void onlyOneSandboxIsCreated() throws Exception {
        WebSandbox sandbox1 = mActivityTestRule.getWebSandbox();
        WebSandbox sandbox2 =
                runOnUiThreadBlocking(() -> WebSandbox.create(mActivityTestRule.getContext()))
                        .get();

        Assert.assertEquals(sandbox1, sandbox2);
    }

    @Test
    @SmallTest
    public void returnsVersion() throws Exception {
        String returnedVersion =
                runOnUiThreadBlocking(() -> WebSandbox.getVersion(mActivityTestRule.getContext()))
                        .get();
        Assert.assertNotNull(returnedVersion);

        Pattern expectedVersionPattern =
                Pattern.compile("[0-9]*[.][0-9]*[.][0-9]*", Pattern.CASE_INSENSITIVE);
        Matcher returnedVersionMatcher = expectedVersionPattern.matcher(returnedVersion);
        Assert.assertTrue(returnedVersionMatcher.find());
    }

    @Test
    @SmallTest
    public void returnsIfIsAvailable() throws Exception {
        boolean isAvailable =
                runOnUiThreadBlocking(() -> WebSandbox.isAvailable(mActivityTestRule.getContext()))
                        .get();
        Assert.assertTrue(isAvailable);
    }

    @Test
    @SmallTest
    public void returnsProviderPackageName() throws Exception {
        String providerPackageName = runOnUiThreadBlocking(
                () -> WebSandbox.getProviderPackageName(mActivityTestRule.getContext()))
                                             .get();
        Assert.assertEquals("org.chromium.weblayer.support", providerPackageName);
    }
}