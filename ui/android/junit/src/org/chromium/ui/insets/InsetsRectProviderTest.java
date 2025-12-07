// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.insets;

import static androidx.core.view.WindowInsetsCompat.Type.captionBar;
import static androidx.core.view.WindowInsetsCompat.Type.navigationBars;
import static androidx.core.view.WindowInsetsCompat.Type.statusBars;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.WindowInsets;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsCompat.Type.InsetsType;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;
import org.chromium.ui.insets.InsetsRectProvider.Consumer;
import org.chromium.ui.insets.InsetsRectProviderTest.ShadowWindowInsetsUtils;
import org.chromium.ui.insets.WindowInsetsUtils.UnoccludedRegion;

import java.util.List;

/**
 * Unit test for {@link InsetsRectProvider}. Since most of the calculations were done in {@link
 * WindowInsetsUtils}, this test is mostly used to test if rects are up-to-date for observation when
 * certain window insets has an update.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30, shadows = ShadowWindowInsetsUtils.class)
public class InsetsRectProviderTest {
    private static final int WINDOW_WIDTH = 600;
    private static final int WINDOW_HEIGHT = 800;
    private static final Size INSETS_FRAME_SIZE = new Size(WINDOW_WIDTH, WINDOW_HEIGHT);

    private InsetsRectProvider mInsetsRectProvider;
    private final CallbackHelper mConsumerCallback = new CallbackHelper();

    @Mock private View mView;
    @Mock private InsetObserver mInsetObserver;

    private final Answer<Object> mBuildNewMockInsets =
            (invocation) -> {
                WindowInsetsCompat windowInsetsCompat =
                        (WindowInsetsCompat) invocation.getArgument(0);
                WindowInsetsCompat newWindowInsetsCompat =
                        deepCopyMockWindowInsetsCompat(windowInsetsCompat);

                int insetsType = (int) invocation.getArgument(1);
                Insets insets = (Insets) invocation.getArgument(2);
                doReturn(insets).when(newWindowInsetsCompat).getInsets(eq(insetsType));
                return newWindowInsetsCompat;
            };

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
    }

    @After
    public void tearDown() {
        ShadowWindowInsetsUtils.reset();
    }

    @Test
    public void testInitialization() {
        // Assume a top insets
        int type = WindowInsetsCompat.Type.captionBar();
        Insets insets = Insets.of(0, 10, 0, 0);
        List<Rect> blockingRects =
                List.of(new Rect(0, 0, 10, 10), new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 10));
        Rect availableArea = new Rect(10, 0, WINDOW_WIDTH - 20, 10);

        WindowInsetsCompat windowInsets =
                buildTestWindowInsets(
                        type, insets, availableArea, INSETS_FRAME_SIZE, blockingRects);
        mInsetsRectProvider =
                new InsetsRectProvider(
                        mInsetObserver, type, windowInsets, InsetConsumerSource.TEST_SOURCE);
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ true));

        assertSuppliedValues(insets, availableArea, blockingRects);
    }

    @Test
    public void testInitializationEmpty() {
        int type = WindowInsetsCompat.Type.captionBar();
        mInsetsRectProvider =
                new InsetsRectProvider(mInsetObserver, type, null, InsetConsumerSource.TEST_SOURCE);
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ false));

        assertSuppliedValues(Insets.NONE, new Rect(), List.of());
    }

    @Test
    public void testObservation_UpdateConsumed() {
        // Assume inset is at the bottom for this test.
        int type = WindowInsetsCompat.Type.navigationBars();
        Insets insets = Insets.of(0, 0, 0, 10);
        List<Rect> blockingRects =
                List.of(
                        new Rect(0, WINDOW_HEIGHT - 10, 10, WINDOW_HEIGHT),
                        new Rect(
                                WINDOW_WIDTH - 20,
                                WINDOW_HEIGHT - 10,
                                WINDOW_WIDTH,
                                WINDOW_HEIGHT));
        Rect availableArea = new Rect(10, WINDOW_HEIGHT - 10, WINDOW_WIDTH - 20, WINDOW_HEIGHT);

        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider =
                new InsetsRectProvider(
                        mInsetObserver, type, emptyWindowInsets, InsetConsumerSource.TEST_SOURCE);
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ true));
        assertSuppliedValues(Insets.NONE, new Rect(), List.of());

        WindowInsetsCompat windowInsets =
                buildTestWindowInsets(
                        type, insets, availableArea, INSETS_FRAME_SIZE, blockingRects);
        mInsetsRectProvider.onApplyWindowInsets(mView, windowInsets);

        assertEquals("Consumer callback not called.", 1, mConsumerCallback.getCallCount());
        assertSuppliedValues(insets, availableArea, blockingRects);
    }

    @Test
    public void testObservation_UpdateNotConsumed() {
        int type = WindowInsetsCompat.Type.captionBar();
        Insets insets = Insets.of(0, 30, 0, 0);
        List<Rect> blockingRects = List.of(new Rect(10, 0, 20, WINDOW_HEIGHT));
        Rect widestArea = new Rect(20, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider =
                new InsetsRectProvider(
                        mInsetObserver, type, emptyWindowInsets, InsetConsumerSource.TEST_SOURCE);
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ false));
        assertSuppliedValues(Insets.NONE, new Rect(), List.of());

        WindowInsetsCompat windowInsets =
                buildTestWindowInsets(type, insets, widestArea, INSETS_FRAME_SIZE, blockingRects);
        var appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, windowInsets);

        assertEquals("Insets should not be consumed.", windowInsets, appliedInsets);
        assertEquals("Consumer callback should be called.", 1, mConsumerCallback.getCallCount());
        assertSuppliedValues(insets, widestArea, blockingRects);
    }

    @Test
    public void testInsetRemoved() {
        // Assume inset is at the top for this test.
        int type = WindowInsetsCompat.Type.statusBars();
        Insets insets = Insets.of(0, 10, 0, 0);
        List<Rect> blockingRects =
                List.of(
                        new Rect(0, WINDOW_HEIGHT - 10, 10, WINDOW_HEIGHT),
                        new Rect(
                                WINDOW_WIDTH - 20,
                                WINDOW_HEIGHT - 10,
                                WINDOW_WIDTH,
                                WINDOW_HEIGHT));
        Rect availableArea = new Rect(10, WINDOW_HEIGHT - 10, WINDOW_WIDTH - 20, WINDOW_HEIGHT);

        // Initialize with valid insets.
        WindowInsetsCompat windowInsets =
                buildTestWindowInsets(
                        type, insets, availableArea, INSETS_FRAME_SIZE, blockingRects);
        mInsetsRectProvider =
                new InsetsRectProvider(
                        mInsetObserver, type, windowInsets, InsetConsumerSource.TEST_SOURCE);
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ true));
        assertSuppliedValues(insets, availableArea, blockingRects);

        // Create an insets with a different type so it removes the exists insets.
        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(
                        WindowInsetsCompat.Type.systemBars(),
                        Insets.NONE,
                        new Rect(),
                        INSETS_FRAME_SIZE,
                        List.of());
        mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);

        // Callback should be called during initialization and when insets are applied again.
        assertEquals("Consumer callback not called.", 2, mConsumerCallback.getCallCount());
        assertSuppliedValues(Insets.NONE, new Rect(), List.of());
    }

    @Test
    public void testAppliedInsetsNotConsumed_EmptyFrame() {
        // Assume caption bar has top insets.
        int type = WindowInsetsCompat.Type.captionBar();
        Insets insets = Insets.of(0, 10, 0, 0);

        // Insets frame / bounding rects will be empty on a device that does not support the
        // corresponding OS APIs.
        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider =
                new InsetsRectProvider(
                        mInsetObserver, type, emptyWindowInsets, InsetConsumerSource.TEST_SOURCE);
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ true));

        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(type, insets, new Rect(), new Size(0, 0), List.of());
        var appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals("Insets should not be consumed.", appliedInsets, newWindowInsets);
        assertEquals(
                "Consumer callback should not be called.", 0, mConsumerCallback.getCallCount());
        assertSuppliedValues(Insets.NONE, new Rect(), List.of());
    }

    @Test
    public void testAppliedInsetsNotConsumed_ComplexUnoccludedRegion() {
        // Assume caption bar has top insets.
        int type = WindowInsetsCompat.Type.captionBar();
        Insets insets = Insets.of(0, 10, 0, 0);

        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider =
                new InsetsRectProvider(
                        mInsetObserver, type, emptyWindowInsets, InsetConsumerSource.TEST_SOURCE);
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ false));

        // Simulate a complex unoccluded region.
        ShadowWindowInsetsUtils.sUnoccludedRegionComplex = true;
        List<Rect> blockingRects = List.of(new Rect(10, 0, 20, WINDOW_HEIGHT));
        Rect availableArea = new Rect(20, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(
                        type, insets, availableArea, INSETS_FRAME_SIZE, blockingRects);
        var appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);

        assertEquals("Input insets should not be consumed.", newWindowInsets, appliedInsets);
        assertTrue("Region should be complex.", mInsetsRectProvider.isUnoccludedRegionComplex());
        assertEquals("Consumer callback should be called.", 1, mConsumerCallback.getCallCount());
        assertSuppliedValues(insets, availableArea, blockingRects);
    }

    @Test
    public void testAppliedInsetsConsumed_SameAsCachedInsets_UnoccludedAreaAvailable() {
        // Assume caption bar has top insets.
        int type = WindowInsetsCompat.Type.captionBar();
        Insets insets = Insets.of(0, 10, 0, 0);

        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider =
                new InsetsRectProvider(
                        mInsetObserver, type, emptyWindowInsets, InsetConsumerSource.TEST_SOURCE);
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ true));

        // Verify that new insets are processed once, with back to back updates. Also verify that
        // the insets are consumed in both cases.
        Rect availableArea = new Rect(0, 0, WINDOW_WIDTH - 20, 10);
        List<Rect> blockingRects = List.of(new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 10));
        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(
                        type, insets, availableArea, INSETS_FRAME_SIZE, blockingRects);
        var appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals(
                "Input insets should be consumed.", Insets.NONE, appliedInsets.getInsets(type));

        appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals(
                "Input insets should be consumed.", Insets.NONE, appliedInsets.getInsets(type));

        assertEquals(
                "Consumer callback should be called once.", 1, mConsumerCallback.getCallCount());
        assertSuppliedValues(insets, availableArea, blockingRects);
    }

    @Test
    public void testAppliedInsetsNotConsumed_SameAsCachedInsets_UnoccludedAreaUnavailable() {
        // Assume caption bar has top insets.
        int type = WindowInsetsCompat.Type.captionBar();
        Insets insets = Insets.of(0, 10, 0, 0);

        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider =
                new InsetsRectProvider(
                        mInsetObserver, type, emptyWindowInsets, InsetConsumerSource.TEST_SOURCE);
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ false));

        // Verify that new insets are processed once but never consumed, with back to back updates,
        // assuming that the consumer does not consume the rect update due to no unoccluded area.
        // Also verify that the insets are not consumed in both cases.
        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(type, insets, new Rect(), INSETS_FRAME_SIZE, List.of());
        var appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals("Input insets should not be consumed.", newWindowInsets, appliedInsets);

        appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals("Input insets should not be consumed.", newWindowInsets, appliedInsets);

        assertEquals(
                "Consumer callback should be called once.", 1, mConsumerCallback.getCallCount());
        assertSuppliedValues(insets, new Rect(), List.of());
    }

    @Test
    public void testCaptionBarInsetsRectProvider_captionBarNoOverlap() {
        // Assume caption bar has top insets.
        Insets captionBarInsets = Insets.of(0, 30, 0, 0);
        Insets statusBarInsets = Insets.of(0, 0, 0, 0);
        Insets navigationBarInsets = Insets.of(0, 0, 0, 15);

        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider =
                Mockito.spy(
                        new CaptionBarInsetsRectProvider(
                                mInsetObserver,
                                emptyWindowInsets,
                                InsetConsumerSource.TEST_SOURCE));
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ true));
        doAnswer(mBuildNewMockInsets).when(mInsetsRectProvider).buildInsets(any(), anyInt(), any());

        Rect availableArea = new Rect(0, 0, WINDOW_WIDTH - 20, 30);
        List<Rect> blockingRects = List.of(new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 30));
        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(
                        captionBar(),
                        captionBarInsets,
                        availableArea,
                        INSETS_FRAME_SIZE,
                        blockingRects);
        doReturn(statusBarInsets).when(newWindowInsets).getInsets(eq(statusBars()));
        doReturn(navigationBarInsets).when(newWindowInsets).getInsets(eq(navigationBars()));

        var appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals(
                "Caption bar insets should be consumed.",
                Insets.NONE,
                appliedInsets.getInsets(captionBar()));
        assertEquals(
                "There are no status bar insets.",
                Insets.NONE,
                appliedInsets.getInsets(statusBars()));
        assertEquals(
                "Navigation bar insets should be unaffected.",
                navigationBarInsets,
                appliedInsets.getInsets(navigationBars()));

        appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals(
                "Caption bar insets should be consumed.",
                Insets.NONE,
                appliedInsets.getInsets(captionBar()));
        assertEquals(
                "There are no status bar insets.",
                Insets.NONE,
                appliedInsets.getInsets(statusBars()));
        assertEquals(
                "Navigation bar insets should be unaffected.",
                navigationBarInsets,
                appliedInsets.getInsets(navigationBars()));

        assertEquals(
                "Consumer callback should be called once.", 1, mConsumerCallback.getCallCount());
        assertSuppliedValues(captionBarInsets, availableArea, blockingRects);
    }

    @Test
    public void testCaptionBarInsetsRectProvider_captionBarCoversStatusBar() {
        // Declare insets.
        Insets captionBarInsets = Insets.of(0, 30, 0, 0);
        Insets statusBarInsets = Insets.of(0, 10, 0, 0);
        Insets navigationBarInsets = Insets.of(0, 0, 0, 15);

        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider =
                Mockito.spy(
                        new CaptionBarInsetsRectProvider(
                                mInsetObserver,
                                emptyWindowInsets,
                                InsetConsumerSource.TEST_SOURCE));
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ true));
        doAnswer(mBuildNewMockInsets).when(mInsetsRectProvider).buildInsets(any(), anyInt(), any());

        Rect availableArea = new Rect(0, 10, WINDOW_WIDTH - 20, 30);
        List<Rect> blockingRects = List.of(new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 30));
        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(
                        captionBar(),
                        captionBarInsets,
                        availableArea,
                        INSETS_FRAME_SIZE,
                        blockingRects);
        doReturn(statusBarInsets).when(newWindowInsets).getInsets(eq(statusBars()));
        doReturn(navigationBarInsets).when(newWindowInsets).getInsets(eq(navigationBars()));

        var appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals(
                "Caption bar insets should be consumed.",
                Insets.NONE,
                appliedInsets.getInsets(captionBar()));
        assertEquals(
                "Status bar insets should be consumed.",
                Insets.NONE,
                appliedInsets.getInsets(statusBars()));
        assertEquals(
                "Navigation bar insets should be unaffected.",
                navigationBarInsets,
                appliedInsets.getInsets(navigationBars()));

        appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals(
                "Caption bar insets should be consumed.",
                Insets.NONE,
                appliedInsets.getInsets(captionBar()));
        assertEquals(
                "Status bar insets should be consumed.",
                Insets.NONE,
                appliedInsets.getInsets(statusBars()));
        assertEquals(
                "Navigation bar insets should be unaffected.",
                navigationBarInsets,
                appliedInsets.getInsets(navigationBars()));

        assertEquals(
                "Consumer callback should be called once.", 1, mConsumerCallback.getCallCount());
        var finalBlockedRects =
                List.of(
                        new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 30),
                        new Rect(0, 0, WINDOW_WIDTH, 10));
        assertSuppliedValues(captionBarInsets, availableArea, finalBlockedRects);
    }

    @Test
    public void testCaptionBarInsetsRectProvider_captionBarCoversStatusBarCompletely() {
        // Declare insets.
        Insets captionBarInsets = Insets.of(0, 30, 0, 0);
        Insets statusBarInsets = Insets.of(0, 30, 0, 0);
        Insets navigationBarInsets = Insets.of(0, 0, 0, 15);

        // Initialize with empty window insets.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        // Simulate the consumer not consuming the rect update due to the status-caption overlap.
        mInsetsRectProvider =
                Mockito.spy(
                        new CaptionBarInsetsRectProvider(
                                mInsetObserver,
                                emptyWindowInsets,
                                InsetConsumerSource.TEST_SOURCE));
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ false));
        doAnswer(mBuildNewMockInsets).when(mInsetsRectProvider).buildInsets(any(), anyInt(), any());

        // Available area should be empty because of the status-caption overlap.
        Rect availableArea = new Rect(0, 0, 0, 0);
        List<Rect> blockingRects = List.of(new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 30));
        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(
                        captionBar(),
                        captionBarInsets,
                        availableArea,
                        INSETS_FRAME_SIZE,
                        blockingRects);
        doReturn(statusBarInsets).when(newWindowInsets).getInsets(eq(statusBars()));
        doReturn(navigationBarInsets).when(newWindowInsets).getInsets(eq(navigationBars()));

        var appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals(
                "Caption bar insets should not be consumed.",
                captionBarInsets,
                appliedInsets.getInsets(captionBar()));
        assertEquals(
                "Status bar insets should be unaffected.",
                statusBarInsets,
                appliedInsets.getInsets(statusBars()));
        assertEquals(
                "Navigation bar insets should be unaffected.",
                navigationBarInsets,
                appliedInsets.getInsets(navigationBars()));

        var finalBlockedRects =
                List.of(
                        new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 30),
                        new Rect(0, 0, WINDOW_WIDTH, 30));
        assertSuppliedValues(captionBarInsets, availableArea, finalBlockedRects);
    }

    @Test
    public void testCaptionBarInsetsRectProvider_statusBarCoversCaptionBar() {
        // Declare insets.
        Insets captionBarInsets = Insets.of(0, 10, 0, 0);
        Insets statusBarInsets = Insets.of(0, 30, 0, 0);
        Insets navigationBarInsets = Insets.of(0, 0, 0, 15);

        // Initialize with empty window insets. Simulate the consumer not consuming the rect update
        // due to the status-caption overlap.
        WindowInsetsCompat emptyWindowInsets = new WindowInsetsCompat.Builder().build();
        mInsetsRectProvider =
                Mockito.spy(
                        new CaptionBarInsetsRectProvider(
                                mInsetObserver,
                                emptyWindowInsets,
                                InsetConsumerSource.TEST_SOURCE));
        mInsetsRectProvider.setConsumer(createInsetsRectConsumer(/* consumeRectUpdate= */ false));
        doAnswer(mBuildNewMockInsets).when(mInsetsRectProvider).buildInsets(any(), anyInt(), any());

        // Available area should be empty because of the status-caption overlap.
        Rect availableArea = new Rect(0, 0, 0, 0);
        List<Rect> blockingRects = List.of(new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 10));

        WindowInsetsCompat newWindowInsets =
                buildTestWindowInsets(
                        captionBar(),
                        captionBarInsets,
                        availableArea,
                        INSETS_FRAME_SIZE,
                        blockingRects);
        doReturn(statusBarInsets).when(newWindowInsets).getInsets(eq(statusBars()));
        doReturn(navigationBarInsets).when(newWindowInsets).getInsets(eq(navigationBars()));

        var appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals(
                "Caption bar insets should not be consumed.",
                captionBarInsets,
                appliedInsets.getInsets(captionBar()));
        assertEquals(
                "Status bar insets should be unaffected.",
                statusBarInsets,
                appliedInsets.getInsets(statusBars()));
        assertEquals(
                "Navigation bar insets should be unaffected.",
                navigationBarInsets,
                appliedInsets.getInsets(navigationBars()));

        appliedInsets = mInsetsRectProvider.onApplyWindowInsets(mView, newWindowInsets);
        assertEquals(
                "Caption bar insets should not be consumed.",
                captionBarInsets,
                appliedInsets.getInsets(captionBar()));
        assertEquals(
                "Status bar insets should be unaffected.",
                statusBarInsets,
                appliedInsets.getInsets(statusBars()));
        assertEquals(
                "Navigation bar insets should be unaffected.",
                navigationBarInsets,
                appliedInsets.getInsets(navigationBars()));

        assertEquals(
                "Consumer callback should be called once.", 1, mConsumerCallback.getCallCount());
        var finalBlockedRects =
                List.of(
                        new Rect(WINDOW_WIDTH - 20, 0, WINDOW_WIDTH, 10),
                        new Rect(0, 0, WINDOW_WIDTH, 30));
        assertSuppliedValues(captionBarInsets, availableArea, finalBlockedRects);
    }

    private WindowInsetsCompat buildTestWindowInsets(
            @InsetsType int type,
            Insets insets,
            Rect availableArea,
            Size frameSize,
            List<Rect> blockingRects) {
        // WindowInsetsCompat.Builder does not work in robolectric (always yield an empty Inset).
        WindowInsetsCompat windowInsetsCompat = Mockito.mock(WindowInsetsCompat.class);
        doReturn(Insets.NONE).when(windowInsetsCompat).getInsets(anyInt());
        doReturn(insets).when(windowInsetsCompat).getInsets(eq(type));

        ShadowWindowInsetsUtils.sWidestUnoccludedRect = availableArea;
        ShadowWindowInsetsUtils.sFrame = frameSize;
        ShadowWindowInsetsUtils.sTestRects = blockingRects != null ? blockingRects : List.of();

        return windowInsetsCompat;
    }

    /**
     * Create a "deep" copy of a mock {@link WindowInsetsCompat} object. This is to replicate the
     * {@link WindowInsetsCompat.Builder} functionality, since the builder does not work in
     * Robolectric tests.
     */
    private static WindowInsetsCompat deepCopyMockWindowInsetsCompat(
            WindowInsetsCompat windowInsetsCompat) {
        int statusBars = WindowInsetsCompat.Type.statusBars();
        int navigationBars = WindowInsetsCompat.Type.navigationBars();
        int captionBar = WindowInsetsCompat.Type.captionBar();

        WindowInsetsCompat newWindowInsetsCompat = Mockito.mock(WindowInsetsCompat.class);
        doReturn(Insets.NONE).when(newWindowInsetsCompat).getInsets(anyInt());
        doReturn(windowInsetsCompat.getInsets(statusBars))
                .when(newWindowInsetsCompat)
                .getInsets(eq(statusBars));
        doReturn(windowInsetsCompat.getInsets(navigationBars))
                .when(newWindowInsetsCompat)
                .getInsets(eq(navigationBars));
        doReturn(windowInsetsCompat.getInsets(captionBar))
                .when(newWindowInsetsCompat)
                .getInsets(eq(captionBar));
        return newWindowInsetsCompat;
    }

    private void assertSuppliedValues(Insets insets, Rect availableArea, List<Rect> blockingRects) {
        assertEquals(
                "Supplied #getBoundingRects is different.",
                blockingRects,
                mInsetsRectProvider.getBoundingRects());
        assertEquals(
                "Supplied #getWidestUnoccludedRect is different.",
                availableArea,
                mInsetsRectProvider.getWidestUnoccludedRect());
        assertEquals(
                "Supplied #getCachedInset is different.",
                insets,
                mInsetsRectProvider.getCachedInset());
    }

    private Consumer createInsetsRectConsumer(boolean consumeRectUpdate) {
        return widestUnoccludedRect -> {
            mConsumerCallback.notifyCalled();
            return consumeRectUpdate;
        };
    }

    /** Helper class to get the results using test values. */
    @Implements(WindowInsetsUtils.class)
    public static class ShadowWindowInsetsUtils {
        static Rect sWidestUnoccludedRect;
        static Size sFrame = new Size(0, 0);
        static List<Rect> sTestRects = List.of();
        static boolean sUnoccludedRegionComplex;

        private static void reset() {
            sWidestUnoccludedRect = null;
            sFrame = new Size(0, 0);
            sTestRects = List.of();
            sUnoccludedRegionComplex = false;
        }

        @Implementation
        protected static UnoccludedRegion getUnoccludedRegion(
                Rect regionRect, List<Rect> blockedRects) {
            Rect widestRect = sWidestUnoccludedRect != null ? sWidestUnoccludedRect : new Rect();
            return new UnoccludedRegion(widestRect, sUnoccludedRegionComplex);
        }

        @Implementation
        protected static Size getFrameFromInsets(WindowInsets windowInsets) {
            return sFrame;
        }

        @Implementation
        protected static List<Rect> getBoundingRectsFromInsets(
                WindowInsets windowInsets, @InsetsType int insetType) {
            return sTestRects;
        }
    }
}
