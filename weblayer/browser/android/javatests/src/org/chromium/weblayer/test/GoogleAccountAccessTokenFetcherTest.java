// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Callback;
import org.chromium.weblayer.GoogleAccountAccessTokenFetcher;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/**
 * Tests to ensure that access token fetching in WebLayer works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class GoogleAccountAccessTokenFetcherTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    @Before
    public void setUp() throws Exception {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
    }

    @Test
    @SmallTest
    // Ensures the viability of setting the fetcher to null.
    public void testSetFetcherToNull() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getBrowser()
                    .getProfile()
                    .setGoogleAccountAccessTokenFetcher(null);
        });
    }

    @Test
    @SmallTest
    // Tests that fetching access tokens from within WebLayer returns the value from the embedder.
    public void testFetchAccessToken() throws Exception {
        Profile profile = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return mActivityTestRule.getActivity().getBrowser().getProfile(); });

        final Set<String> expectedScopes = new HashSet<String>(Arrays.asList("scope1", "scope2"));
        final String expectedToken = "accessToken1";

        GoogleAccountAccessTokenFetcherEmbedderImpl fetcherImpl =
                new GoogleAccountAccessTokenFetcherEmbedderImpl();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { profile.setGoogleAccountAccessTokenFetcher(fetcherImpl); });

        final CallbackHelper callbackHelper = new CallbackHelper();
        final Callback<String> onTokenFetchedCallback = new Callback<String>() {
            @Override
            public void onResult(String value) {
                Assert.assertEquals(expectedToken, value);
                callbackHelper.notifyCalled();
            }
        };

        TestWebLayer testWebLayer = TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
        testWebLayer.fetchAccessToken(profile, expectedScopes, onTokenFetchedCallback);
        Assert.assertEquals(1, fetcherImpl.getNumOutstandingRequests());
        Assert.assertEquals(expectedScopes, fetcherImpl.getMostRecentRequestScopes());

        fetcherImpl.respondWithTokenForRequest(fetcherImpl.getMostRecentRequestId(), expectedToken);
        callbackHelper.waitForFirst();
    }

    @Test
    @SmallTest
    // Tests handling of multiple ongoing requests.
    public void testMultipleAccessTokenRequests() throws Exception {
        Profile profile = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return mActivityTestRule.getActivity().getBrowser().getProfile(); });

        GoogleAccountAccessTokenFetcherEmbedderImpl fetcherImpl =
                new GoogleAccountAccessTokenFetcherEmbedderImpl();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { profile.setGoogleAccountAccessTokenFetcher(fetcherImpl); });

        final String expectedToken1 = "accessToken1";
        final String expectedToken2 = "accessToken2";
        final String expectedToken3 = "accessToken3";

        final Set<String> expectedScopes1 = new HashSet<String>(Arrays.asList("scope1", "scope2"));
        final Set<String> expectedScopes2 = new HashSet<String>(Arrays.asList("scope2", "scope3"));
        final Set<String> expectedScopes3 = new HashSet<String>(Arrays.asList("scope4", "scope5"));

        final String[] accessTokens = {"", "", ""};

        final CallbackHelper callbackHelper1 = new CallbackHelper();
        final Callback<String> onTokenFetchedCallback1 = new Callback<String>() {
            @Override
            public void onResult(String value) {
                accessTokens[0] = value;
                callbackHelper1.notifyCalled();
            }
        };

        final CallbackHelper callbackHelper2 = new CallbackHelper();
        final Callback<String> onTokenFetchedCallback2 = new Callback<String>() {
            @Override
            public void onResult(String value) {
                accessTokens[1] = value;
                callbackHelper2.notifyCalled();
            }
        };

        final CallbackHelper callbackHelper3 = new CallbackHelper();
        final Callback<String> onTokenFetchedCallback3 = new Callback<String>() {
            @Override
            public void onResult(String value) {
                accessTokens[2] = value;
                callbackHelper3.notifyCalled();
            }
        };

        TestWebLayer testWebLayer = TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());

        // Make the first two access token requests.
        testWebLayer.fetchAccessToken(profile, expectedScopes1, onTokenFetchedCallback1);
        Assert.assertEquals(1, fetcherImpl.getNumOutstandingRequests());
        Assert.assertEquals(expectedScopes1, fetcherImpl.getMostRecentRequestScopes());
        int requestId1 = fetcherImpl.getMostRecentRequestId();

        testWebLayer.fetchAccessToken(profile, expectedScopes2, onTokenFetchedCallback2);
        Assert.assertEquals(2, fetcherImpl.getNumOutstandingRequests());
        Assert.assertEquals(expectedScopes2, fetcherImpl.getMostRecentRequestScopes());
        int requestId2 = fetcherImpl.getMostRecentRequestId();

        // Resolve the second request.
        fetcherImpl.respondWithTokenForRequest(requestId2, expectedToken2);
        callbackHelper2.waitForFirst();
        Assert.assertEquals("", accessTokens[0]);
        Assert.assertEquals(expectedToken2, accessTokens[1]);
        Assert.assertEquals("", accessTokens[2]);

        // Make the third request.
        testWebLayer.fetchAccessToken(profile, expectedScopes3, onTokenFetchedCallback3);
        Assert.assertEquals(2, fetcherImpl.getNumOutstandingRequests());
        Assert.assertEquals(expectedScopes3, fetcherImpl.getMostRecentRequestScopes());
        int requestId3 = fetcherImpl.getMostRecentRequestId();

        // Resolve the first request.
        fetcherImpl.respondWithTokenForRequest(requestId1, expectedToken1);
        callbackHelper1.waitForFirst();
        Assert.assertEquals(expectedToken1, accessTokens[0]);
        Assert.assertEquals(expectedToken2, accessTokens[1]);
        Assert.assertEquals("", accessTokens[2]);

        // Resolve the third request.
        fetcherImpl.respondWithTokenForRequest(requestId3, expectedToken3);
        callbackHelper2.waitForFirst();
        Assert.assertEquals(expectedToken1, accessTokens[0]);
        Assert.assertEquals(expectedToken2, accessTokens[1]);
        Assert.assertEquals(expectedToken3, accessTokens[2]);
    }

    @Test
    @SmallTest
    // Tests that WebLayer forwards invalid access token notifications to the embedder.
    public void testOnAccessTokenIdentifiedAsInvalid() throws Exception {
        Profile profile = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return mActivityTestRule.getActivity().getBrowser().getProfile(); });

        final Set<String> scopesForInvalidToken =
                new HashSet<String>(Arrays.asList("scope1", "scope2"));
        final String invalidToken = "accessToken1";

        GoogleAccountAccessTokenFetcherEmbedderImpl fetcherImpl =
                new GoogleAccountAccessTokenFetcherEmbedderImpl();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { profile.setGoogleAccountAccessTokenFetcher(fetcherImpl); });

        Assert.assertNull(fetcherImpl.getScopesForMostRecentInvalidToken());
        Assert.assertNull(fetcherImpl.getMostRecentInvalidToken());

        TestWebLayer testWebLayer = TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
        testWebLayer.fireOnAccessTokenIdentifiedAsInvalid(
                profile, scopesForInvalidToken, invalidToken);

        Assert.assertEquals(
                scopesForInvalidToken, fetcherImpl.getScopesForMostRecentInvalidToken());
        Assert.assertEquals(invalidToken, fetcherImpl.getMostRecentInvalidToken());
    }

    private class GoogleAccountAccessTokenFetcherEmbedderImpl
            extends GoogleAccountAccessTokenFetcher {
        private HashMap<Integer, Callback<String>> mOutstandingRequests =
                new HashMap<Integer, Callback<String>>();
        private int mMostRecentRequestId;
        private Set<String> mMostRecentRequestScopes;
        private Set<String> mScopesForMostRecentInvalidToken;
        private String mMostRecentInvalidToken;

        @Override
        public void fetchAccessToken(Set<String> scopes, Callback<String> onTokenFetched) {
            mMostRecentRequestScopes = scopes;
            mMostRecentRequestId++;
            mOutstandingRequests.put(mMostRecentRequestId, onTokenFetched);
        }

        @Override
        public void onAccessTokenIdentifiedAsInvalid(Set<String> scopes, String token) {
            mScopesForMostRecentInvalidToken = scopes;
            mMostRecentInvalidToken = token;
        }

        int getMostRecentRequestId() {
            return mMostRecentRequestId;
        }

        Set<String> getMostRecentRequestScopes() {
            return mMostRecentRequestScopes;
        }

        int getNumOutstandingRequests() {
            return mOutstandingRequests.size();
        }

        Set<String> getScopesForMostRecentInvalidToken() {
            return mScopesForMostRecentInvalidToken;
        }

        String getMostRecentInvalidToken() {
            return mMostRecentInvalidToken;
        }

        void respondWithTokenForRequest(int requestId, String token) {
            Callback<String> callback = mOutstandingRequests.get(requestId);
            assert callback != null;
            mOutstandingRequests.remove(requestId);

            callback.onResult(token);
        }
    }
}
