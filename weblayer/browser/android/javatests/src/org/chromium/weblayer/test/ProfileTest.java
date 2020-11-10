// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.webkit.ValueCallback;

import androidx.fragment.app.FragmentManager;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Callback;
import org.chromium.weblayer.CookieManager;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.Arrays;
import java.util.Collection;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests that Profile works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
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

        Profile secondProfile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return weblayer.getProfile("second_test"); });

        {
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertEquals(2, profiles.size());
            Assert.assertTrue(profiles.contains(firstProfile));
            Assert.assertTrue(profiles.contains(secondProfile));
        }
    }

    @Test
    @SmallTest
    public void testDestroyAndDeleteDataFromDiskWhenInUse() {
        WebLayer weblayer = mActivityTestRule.getWebLayer();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowser().getProfile());

        try {
            Callable<Void> c = () -> {
                profile.destroyAndDeleteDataFromDisk(null);
                return null;
            };
            TestThreadUtils.runOnUiThreadBlocking(c);
            Assert.fail();
        } catch (ExecutionException e) {
            // Expected.
        }
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(87)
    public void testDestroyAndDeleteDataFromDiskSoonWhenInUse() throws Exception {
        WebLayer weblayer = mActivityTestRule.getWebLayer();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        final CallbackHelper callbackHelper = new CallbackHelper();
        Profile profile =
                TestThreadUtils.runOnUiThreadBlocking(() -> activity.getBrowser().getProfile());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            profile.destroyAndDeleteDataFromDiskSoon(callbackHelper::notifyCalled);
            FragmentManager fm = activity.getSupportFragmentManager();
            fm.beginTransaction()
                    .remove(fm.getFragments().get(0))
                    .runOnCommit(callbackHelper::notifyCalled)
                    .commit();
        });
        callbackHelper.waitForCallback(0, 2);
        Collection<Profile> profiles = getAllProfiles();
        Assert.assertFalse(profiles.contains(profile));
    }

    @Test
    @SmallTest
    public void testDestroyAndDeleteDataFromDisk() throws Exception {
        doTestDestroyAndDeleteDataFromDiskIncognito("testDestroyAndDeleteDataFromDisk");
    }

    @Test
    @SmallTest
    public void testDestroyAndDeleteDataFromDiskIncognito() throws Exception {
        doTestDestroyAndDeleteDataFromDiskIncognito(null);
    }

    private void doTestDestroyAndDeleteDataFromDiskIncognito(final String name) throws Exception {
        WebLayer weblayer = mActivityTestRule.getWebLayer();
        final Profile profile =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> weblayer.getProfile(name));

        {
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertTrue(profiles.contains(profile));
        }

        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> profile.destroyAndDeleteDataFromDisk(callbackHelper::notifyCalled));
        {
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertFalse(profiles.contains(profile));
        }
        callbackHelper.waitForFirst();
    }

    @Test
    @SmallTest
    public void testEnumerateAllProfileNames() throws Exception {
        final String profileName = "TestEnumerateAllProfileNames";
        final InstrumentationActivity activity = mActivityTestRule.launchWithProfile(profileName);
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowser().getProfile());

        Assert.assertTrue(Arrays.asList(enumerateAllProfileNames()).contains(profileName));

        TestThreadUtils.runOnUiThreadBlocking(() -> activity.finish());
        CriteriaHelper.pollUiThread(activity::isDestroyed);
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> profile.destroyAndDeleteDataFromDisk(callbackHelper::notifyCalled));
        callbackHelper.waitForFirst();

        Assert.assertFalse(Arrays.asList(enumerateAllProfileNames()).contains(profileName));
    }

    private Profile launchAndDestroyActivity(
            String profileName, ValueCallback<InstrumentationActivity> callback) {
        final InstrumentationActivity activity = mActivityTestRule.launchWithProfile(profileName);
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowser().getProfile());

        callback.onReceiveValue(activity);

        TestThreadUtils.runOnUiThreadBlocking(() -> activity.finish());
        CriteriaHelper.pollUiThread(activity::isDestroyed);
        return profile;
    }

    private void destroyAndDeleteDataFromDisk(Profile profile) throws Exception {
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> profile.destroyAndDeleteDataFromDisk(callbackHelper::notifyCalled));
        callbackHelper.waitForFirst();
    }

    @Test
    @SmallTest
    public void testReuseProfile() throws Exception {
        final String profileName = "ReusedProfile";
        final Uri uri = Uri.parse("https://foo.bar");
        final String expectedCookie = "foo=bar";
        {
            // Create profile and activity and set a cookie.
            launchAndDestroyActivity(profileName, (InstrumentationActivity activity) -> {
                CookieManager cookieManager = TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> { return activity.getBrowser().getProfile().getCookieManager(); });
                try {
                    boolean result =
                            mActivityTestRule.setCookie(cookieManager, uri, expectedCookie);
                    Assert.assertTrue(result);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            });
        }

        {
            // Without deleting proifle data, create profile and activity again, and check the
            // cookie is there.
            Profile profile =
                    launchAndDestroyActivity(profileName, (InstrumentationActivity activity) -> {
                        CookieManager cookieManager =
                                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                                    return activity.getBrowser().getProfile().getCookieManager();
                                });
                        try {
                            String cookie = mActivityTestRule.getCookie(cookieManager, uri);
                            Assert.assertEquals(expectedCookie, cookie);
                        } catch (Exception e) {
                            throw new RuntimeException(e);
                        }
                    });

            destroyAndDeleteDataFromDisk(profile);
        }

        {
            // After deleting profile data, create profile and activity again, and check the cookie
            // is deleted as well.
            launchAndDestroyActivity(profileName, (InstrumentationActivity activity) -> {
                CookieManager cookieManager = TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> { return activity.getBrowser().getProfile().getCookieManager(); });
                try {
                    String cookie = mActivityTestRule.getCookie(cookieManager, uri);
                    Assert.assertEquals("", cookie);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            });
        }
    }

    private String[] enumerateAllProfileNames() throws Exception {
        final String[][] holder = new String[1][];
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Callback<String[]> callback = new Callback<String[]>() {
                @Override
                public void onResult(String[] names) {
                    holder[0] = names;
                    callbackHelper.notifyCalled();
                }
            };
            mActivityTestRule.getWebLayer().enumerateAllProfileNames(callback);
        });
        callbackHelper.waitForFirst();
        return holder[0];
    }

    private static Collection<Profile> getAllProfiles() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> Profile.getAllProfiles());
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(87)
    public void testMultipleIncognitoProfiles() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebLayer weblayer = mActivityTestRule.getWebLayer();
            Profile profile = activity.getBrowser().getProfile();
            Assert.assertFalse(profile.isIncognito());
            final String incognitoProfileName1 = "incognito1";
            Profile incognitoProfile1 = weblayer.getIncognitoProfile(incognitoProfileName1);
            Assert.assertTrue(incognitoProfile1.isIncognito());
            Assert.assertEquals(incognitoProfileName1, incognitoProfile1.getName());

            final String incognitoProfileName2 = "incognito2";
            Profile incognitoProfile2 = weblayer.getIncognitoProfile(incognitoProfileName2);
            Assert.assertTrue(incognitoProfile2.isIncognito());
            Assert.assertEquals(incognitoProfileName2, incognitoProfile2.getName());
            Assert.assertNotEquals(incognitoProfile1, incognitoProfile2);

            // Calling getIncognitoProfile() should return the same Profile.
            Assert.assertEquals(
                    incognitoProfile1, weblayer.getIncognitoProfile(incognitoProfileName1));
            Assert.assertEquals(
                    incognitoProfile2, weblayer.getIncognitoProfile(incognitoProfileName2));

            // getAllProfiles() should return the incognito profiles.
            Collection<Profile> allProfiles = Profile.getAllProfiles();
            Assert.assertEquals(3, allProfiles.size());
            Assert.assertTrue(allProfiles.contains(profile));
            Assert.assertTrue(allProfiles.contains(incognitoProfile1));
            Assert.assertTrue(allProfiles.contains(incognitoProfile2));
        });

        // Incognito profiles should not be returned from enumerateAllProfileNames().
        String[] profileNames = enumerateAllProfileNames();
        Assert.assertEquals(1, profileNames.length);
        Assert.assertEquals(profileNames[0],
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> activity.getBrowser().getProfile().getName()));
    }
}
