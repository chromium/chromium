// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import static org.robolectric.Shadows.shadowOf;

import android.os.Process;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.digital_asset_links.OriginVerifier;
import org.chromium.components.digital_asset_links.OriginVerifier.OriginVerificationListener;
import org.chromium.components.digital_asset_links.OriginVerifierJni;
import org.chromium.components.digital_asset_links.OriginVerifierUnitTestSupport;
import org.chromium.components.digital_asset_links.RelationshipCheckResult;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.CountDownLatch;

/** Tests for WebLayerOriginVerifier. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(WebLayerOriginVerifierTest.TEST_BATCH_NAME)
public class WebLayerOriginVerifierTest {
    public static final String TEST_BATCH_NAME = "weblayer_origin_verifier";

    private static final String PACKAGE_NAME = "org.chromium.weblayer_private";
    private int mUid = Process.myUid();
    private Origin mHttpsOrigin = Origin.create("https://www.example.com");
    private Origin mHttpOrigin = Origin.create("http://www.android.com");
    private Origin mHttpLocalhostOrigin = Origin.create("http://localhost:1234/");
    private boolean mStrictLocalhostVerification;

    private WebLayerOriginVerifier mHandleAllUrlsVerifier;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private OriginVerifier.Natives mMockOriginVerifierJni;

    private CountDownLatch mVerificationResultLatch = new CountDownLatch(1);
    private CountDownLatch mVerificationResultLatch2 = new CountDownLatch(1);

    private static class TestOriginVerificationListener implements OriginVerificationListener {
        private CountDownLatch mLatch;
        private boolean mVerified;

        TestOriginVerificationListener(CountDownLatch latch) {
            mLatch = latch;
        }

        @Override
        public void onOriginVerified(
                String packageName, Origin origin, boolean verified, Boolean online) {
            mVerified = verified;
            mLatch.countDown();
        }

        public boolean isVerified() {
            return mVerified;
        }
    }

    private class TestWebLayerOriginVerifier extends WebLayerOriginVerifier {
        public TestWebLayerOriginVerifier(String packageName, String relationship,
                WebLayerVerificationResultStore verificationResultStore) {
            super(packageName, relationship, verificationResultStore);
        }

        @Override
        boolean getStrictLocalhostVerificationFromManifest() {
            return mStrictLocalhostVerification;
        }
    }

    @Before
    public void setUp() throws Exception {
        OriginVerifierUnitTestSupport.registerPackageWithSignature(
                shadowOf(ApplicationProvider.getApplicationContext().getPackageManager()),
                PACKAGE_NAME, mUid);

        mHandleAllUrlsVerifier = new TestWebLayerOriginVerifier(PACKAGE_NAME,
                "delegate_permission/common.handle_all_urls",
                WebLayerVerificationResultStore.getInstance());

        mJniMocker.mock(OriginVerifierJni.TEST_HOOKS, mMockOriginVerifierJni);
        Mockito.doAnswer(args -> { return 100L; })
                .when(mMockOriginVerifierJni)
                .init(Mockito.any(), Mockito.any());
        Mockito.doAnswer(args -> {
                   mHandleAllUrlsVerifier.onOriginVerificationResult(
                           args.getArgument(4), RelationshipCheckResult.SUCCESS);
                   return true;
               })
                .when(mMockOriginVerifierJni)
                .verifyOrigin(ArgumentMatchers.anyLong(), Mockito.any(),
                        ArgumentMatchers.anyString(), Mockito.any(), ArgumentMatchers.anyString(),
                        ArgumentMatchers.anyString(), Mockito.any());
    }

    @Test
    public void testHttpsVerification() throws Exception {
        TestOriginVerificationListener resultListener =
                new TestOriginVerificationListener(mVerificationResultLatch);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHandleAllUrlsVerifier.start(resultListener, null, mHttpsOrigin));
        mVerificationResultLatch.await();
        Assert.assertTrue(resultListener.isVerified());
    }

    @Test
    public void testHttpVerificationNotAllowed() throws Exception {
        TestOriginVerificationListener resultListener =
                new TestOriginVerificationListener(mVerificationResultLatch);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHandleAllUrlsVerifier.start(resultListener, null, mHttpOrigin));
        mVerificationResultLatch.await();
        Assert.assertFalse(resultListener.isVerified());
    }

    @Test
    public void testHttpLocalhostVerificationAllowed() throws Exception {
        Mockito.doAnswer(args -> {
                   Assert.fail("verifyOrigin was unexpectedly called.");
                   return true;
               })
                .when(mMockOriginVerifierJni)
                .verifyOrigin(ArgumentMatchers.anyLong(), Mockito.any(),
                        ArgumentMatchers.anyString(), Mockito.any(), ArgumentMatchers.anyString(),
                        ArgumentMatchers.anyString(), Mockito.any());
        TestOriginVerificationListener resultListener =
                new TestOriginVerificationListener(mVerificationResultLatch);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHandleAllUrlsVerifier.start(resultListener, null, mHttpLocalhostOrigin));
        mVerificationResultLatch.await();
        Assert.assertTrue(resultListener.isVerified());
    }

    @Test
    public void testHttpLocalhostVerificationNotSkippedWithFlag() throws Exception {
        mStrictLocalhostVerification = true;
        Mockito.doAnswer(args -> {
                   mHandleAllUrlsVerifier.onOriginVerificationResult(
                           args.getArgument(4), RelationshipCheckResult.SUCCESS);
                   return true;
               })
                .when(mMockOriginVerifierJni)
                .verifyOrigin(ArgumentMatchers.anyLong(), Mockito.any(),
                        ArgumentMatchers.anyString(), Mockito.any(), ArgumentMatchers.anyString(),
                        ArgumentMatchers.anyString(), Mockito.any());

        TestOriginVerificationListener resultListener =
                new TestOriginVerificationListener(mVerificationResultLatch);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHandleAllUrlsVerifier.start(resultListener, null, mHttpLocalhostOrigin));
        mVerificationResultLatch.await();
        Assert.assertTrue(resultListener.isVerified());
    }

    @Test
    public void testConcurrentVerifications() throws Exception {
        Mockito.doAnswer(args -> {
                   // Don't call onOriginVerificationResult to simulate a long request.
                   return true;
               })
                .when(mMockOriginVerifierJni)
                .verifyOrigin(ArgumentMatchers.anyLong(), Mockito.any(),
                        ArgumentMatchers.anyString(), Mockito.any(), ArgumentMatchers.anyString(),
                        ArgumentMatchers.anyString(), Mockito.any());
        TestOriginVerificationListener verificationResult1 =
                new TestOriginVerificationListener(mVerificationResultLatch);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHandleAllUrlsVerifier.start(verificationResult1, null, mHttpsOrigin));
        Assert.assertFalse(verificationResult1.isVerified());
        Assert.assertEquals(mHandleAllUrlsVerifier.getNumListeners(mHttpsOrigin), 1);

        // Never called, but if it was called, the following asserts would break.
        Mockito.doAnswer(args -> {
                   mHandleAllUrlsVerifier.onOriginVerificationResult(
                           args.getArgument(4), RelationshipCheckResult.SUCCESS);
                   return true;
               })
                .when(mMockOriginVerifierJni)
                .verifyOrigin(ArgumentMatchers.anyLong(), Mockito.any(),
                        ArgumentMatchers.anyString(), Mockito.any(), ArgumentMatchers.anyString(),
                        ArgumentMatchers.anyString(), Mockito.any());

        TestOriginVerificationListener verificationResult2 =
                new TestOriginVerificationListener(mVerificationResultLatch2);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHandleAllUrlsVerifier.start(verificationResult2, null, mHttpsOrigin));

        Assert.assertFalse(verificationResult2.isVerified());
        // Check that both requests are registered as Listeners.
        Assert.assertEquals(mHandleAllUrlsVerifier.getNumListeners(mHttpsOrigin), 2);

        // Call the {@link OriginVerifier#onOriginVerificationResult} callback and verify that
        // both listeners receive the result.
        mHandleAllUrlsVerifier.onOriginVerificationResult(
                mHttpsOrigin.toString(), RelationshipCheckResult.SUCCESS);

        mVerificationResultLatch.await();
        mVerificationResultLatch2.await();

        Assert.assertTrue(verificationResult1.isVerified());
        Assert.assertTrue(verificationResult2.isVerified());
    }
}