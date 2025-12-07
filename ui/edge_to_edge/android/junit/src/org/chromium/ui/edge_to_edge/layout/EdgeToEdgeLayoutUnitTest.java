// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge.layout;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.WindowManager.LayoutParams;
import android.widget.FrameLayout;

import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgeFieldTrial;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30)
public class EdgeToEdgeLayoutUnitTest {
    private static final int STATUS_BAR_SIZE = 100;
    private static final int NAV_BAR_SIZE = 150;
    private static final int CAPTION_BAR_SIZE = 180;
    private static final int CUTOUT_SIZE = 75;
    private static final int SMALL_CUTOUT_SIZE = 16;
    private static final int IME_SIZE = 320;

    private static final int STATUS_BARS = WindowInsetsCompat.Type.statusBars();
    private static final int NAVIGATION_BARS = WindowInsetsCompat.Type.navigationBars();
    private static final int CAPTION_BAR = WindowInsetsCompat.Type.captionBar();
    private static final int SYSTEM_BARS = WindowInsetsCompat.Type.systemBars();
    private static final int DISPLAY_CUTOUT = WindowInsetsCompat.Type.displayCutout();
    private static final int TAPPABLE_ELEMENT = WindowInsetsCompat.Type.tappableElement();
    private static final int MANDATORY_SYSTEM_GESTURES =
            WindowInsetsCompat.Type.mandatorySystemGestures();
    private static final int SYSTEM_GESTURES = WindowInsetsCompat.Type.systemGestures();
    private static final int IME = WindowInsetsCompat.Type.ime();
    private static final int ALL_SUPPORTED_INSETS = SYSTEM_BARS + DISPLAY_CUTOUT + IME;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock InsetObserver mInsetObserver;
    @Mock EdgeToEdgeFieldTrial mUseBackupNavbarFieldTrial;

    private View mOriginalContentView;
    private EdgeToEdgeLayoutCoordinator mEdgeToEdgeLayoutCoordinator;
    private Activity mActivity;
    private EdgeToEdgeBaseLayout mEdgeToEdgeLayout;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mOriginalContentView = new FrameLayout(mActivity);
        doReturn(true).when(mInsetObserver).hasSeenNonZeroNavigationBarInsets();
        doReturn(true).when(mUseBackupNavbarFieldTrial).isEnabledForManufacturerVersion();
    }

    @Test
    public void testInitialize() {
        initialize(null, /* useBackupNavbarInsets= */ true);
        assertEquals(mEdgeToEdgeLayout, mOriginalContentView.getParent());
    }

    @Test
    public void testInitialize_withInsetObserver() {
        initialize(mInsetObserver, /* useBackupNavbarInsets= */ true);
        assertEquals(mEdgeToEdgeLayout, mOriginalContentView.getParent());
        verify(mInsetObserver)
                .addInsetsConsumer(any(), eq(InsetConsumerSource.EDGE_TO_EDGE_LAYOUT_COORDINATOR));
    }

    // ┌────────┐
    // ├────────┤
    // │        │
    // │        │
    // ├────────┤
    // └────────┘
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_TopBottomSystemBar() {
        initializePortraitLayout();

        WindowInsetsCompat topBottomInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, topBottomInsets);
        assertInsetsConsumed(newInsets, STATUS_BARS + NAVIGATION_BARS);

        measureAndLayoutRootView(400, 600);
        assertPaddings(
                /* left= */ 0,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ 0,
                /* bottom= */ NAV_BAR_SIZE);

        // status bar is with Rect(0,0, WINDOW_WIDTH, STATUS_BAR_SIZE)
        assertEquals(
                "Status bar is at the top of the window.",
                new Rect(0, 0, 400, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        // Nav bar is with Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar is at the bottom of the screen.",
                new Rect(0, 450, 400, 600),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        // Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar divider is the top 1px height for the nav bar.",
                new Rect(0, 450, 400, 451),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌───────┐
    // │       │
    // │       │
    // │       │
    // │-------│
    // │ X X X │
    // └───────┘
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_Fullscreen_TappableInsetsNotUsed() {
        initializePortraitLayout();

        WindowInsetsCompat topBottomInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, 0, 0, 0))
                        .setInsetsIgnoringVisibility(
                                STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 0))
                        .setInsetsIgnoringVisibility(
                                NAVIGATION_BARS, Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(TAPPABLE_ELEMENT, Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(SYSTEM_GESTURES, Insets.of(STATUS_BAR_SIZE, 0, 0, NAV_BAR_SIZE))
                        .setInsets(
                                MANDATORY_SYSTEM_GESTURES,
                                Insets.of(STATUS_BAR_SIZE, 0, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, topBottomInsets);
        assertInsetsConsumed(newInsets, STATUS_BARS + NAVIGATION_BARS);

        measureAndLayoutRootView(400, 600);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);

        assertEquals(
                "Status bar insets don't exist.",
                new Rect(),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Nav bar insets don't exist.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Nav bar divider doesn't exist.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌───────┐
    // │       │
    // │       │
    // │       │
    // │-------│
    // │ X X X │
    // └───────┘
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_Fullscreen_GestureInsetsNotUsed() {
        initializePortraitLayout();

        WindowInsetsCompat topBottomInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, 0, 0, 0))
                        .setInsetsIgnoringVisibility(
                                STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 0))
                        .setInsetsIgnoringVisibility(
                                NAVIGATION_BARS, Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(SYSTEM_GESTURES, Insets.of(STATUS_BAR_SIZE, 0, 0, NAV_BAR_SIZE))
                        .setInsets(
                                MANDATORY_SYSTEM_GESTURES,
                                Insets.of(STATUS_BAR_SIZE, 0, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, topBottomInsets);
        assertInsetsConsumed(newInsets, STATUS_BARS + NAVIGATION_BARS);

        measureAndLayoutRootView(400, 600);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);

        assertEquals(
                "Status bar insets don't exist.",
                new Rect(),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Nav bar insets don't exist.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Nav bar divider doesn't exist.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌────────┐
    // ├────────┤
    // │        │
    // │        │
    // └────────┘
    // Case when window is on split screen top.
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_NoNavigationBar() {
        initializePortraitLayout();

        WindowInsetsCompat topBottomInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, topBottomInsets);
        assertInsetsConsumed(newInsets, STATUS_BARS + NAVIGATION_BARS);

        measureAndLayoutRootView(400, 600);
        assertPaddings(/* left= */ 0, /* top= */ STATUS_BAR_SIZE, /* right= */ 0, /* bottom= */ 0);

        // status bar is with Rect(0,0, WINDOW_WIDTH, STATUS_BAR_SIZE)
        assertEquals(
                "Status bar is at the top of the window.",
                new Rect(0, 0, 400, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Nav bar insets doesn't exists.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Nav bar divider doesn't exists.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌────────┐
    // │        │
    // │        │
    // ├────────┤
    // └────────┘
    // Case when window is on bottom split screen bottom.
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_NoStatusBar() {
        initializePortraitLayout();

        WindowInsetsCompat topBottomInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(NAV_BAR_SIZE, Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, topBottomInsets);
        assertInsetsConsumed(newInsets, STATUS_BARS + NAVIGATION_BARS);

        measureAndLayoutRootView(400, 600);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ NAV_BAR_SIZE);

        assertEquals(
                "Status bar insets should be empty .",
                new Rect(),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        // Nav bar is with Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar is at the bottom of the screen.",
                new Rect(0, 450, 400, 600),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        // Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar divider is the top 1px height for the nav bar.",
                new Rect(0, 450, 400, 451),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌────────┐
    // │        │
    // │        │
    // ├────────┤
    // │keyboard│
    // └────────┘
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_Ime() {
        initialize(null, /* useBackupNavbarInsets= */ true);
        measureAndLayoutRootView(400, 600);

        WindowInsetsCompat withImeInset =
                new WindowInsetsCompat.Builder()
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(IME, Insets.of(0, 0, 0, IME_SIZE))
                        .build();

        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, withImeInset);
        assertInsetsConsumed(newInsets, NAVIGATION_BARS);
        assertInsetsConsumed(newInsets, IME);

        measureAndLayoutRootView(400, 600);
        // The padding should take the higher value of IME / nav bar.
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ IME_SIZE);
        // Nav bar exists, so its size should still be counted.
        // Nav bar is with Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar is at the bottom of the screen.",
                new Rect(0, 450, 400, 600),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        // Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar divider is the top 1px height for the nav bar.",
                new Rect(0, 450, 400, 451),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌───────┐
    // ├───────┤
    // │       │
    // │       │
    // ├-------┤
    // │missing│
    // └───────┘
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_backupInsetsDisabled() {
        initializePortraitLayout(/* useBackupNavbarInsets= */ false);

        WindowInsetsCompat missingNavbarInset =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 0))
                        .setInsets(TAPPABLE_ELEMENT, Insets.of(0, 0, 0, 0))
                        .setInsets(
                                MANDATORY_SYSTEM_GESTURES,
                                Insets.of(0, STATUS_BAR_SIZE, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, missingNavbarInset);
        assertInsetsConsumed(
                newInsets,
                STATUS_BARS + NAVIGATION_BARS + TAPPABLE_ELEMENT + MANDATORY_SYSTEM_GESTURES);

        measureAndLayoutRootView(400, 600);
        assertPaddings(/* left= */ 0, /* top= */ STATUS_BAR_SIZE, /* right= */ 0, /* bottom= */ 0);

        // status bar is with Rect(0,0, WINDOW_WIDTH, STATUS_BAR_SIZE)
        assertEquals(
                "Status bar is at the top of the window.",
                new Rect(0, 0, 400, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Navbar insets are missing, and backup navbar insets are disabled.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Navbar insets are missing, and backup navbar insets are disabled.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌───────┐
    // ├───────┤
    // │       │
    // │       │
    // ├-------┤
    // │missing│
    // └───────┘
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_useTappableElementForBackupInsets() {
        initializePortraitLayout(/* useBackupNavbarInsets= */ true);

        WindowInsetsCompat missingNavbarInset =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 0))
                        .setInsets(TAPPABLE_ELEMENT, Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(
                                MANDATORY_SYSTEM_GESTURES,
                                Insets.of(0, STATUS_BAR_SIZE, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, missingNavbarInset);
        assertInsetsConsumed(
                newInsets,
                STATUS_BARS + NAVIGATION_BARS + TAPPABLE_ELEMENT + MANDATORY_SYSTEM_GESTURES);

        measureAndLayoutRootView(400, 600);
        assertPaddings(
                /* left= */ 0,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ 0,
                /* bottom= */ NAV_BAR_SIZE);

        // status bar is with Rect(0,0, WINDOW_WIDTH, STATUS_BAR_SIZE)
        assertEquals(
                "Status bar is at the top of the window.",
                new Rect(0, 0, 400, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        // Nav bar is with Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar is at the bottom of the screen.",
                new Rect(0, 450, 400, 600),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        // Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar divider is the top 1px height for the nav bar.",
                new Rect(0, 450, 400, 451),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌───────┐
    // ├───────┤
    // │       │
    // │       │
    // ├-------┤
    // │missing│
    // └───────┘
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_hasSeenNonZeroNavBar_doNotUseBackupInsets() {
        initializePortraitLayout(/* useBackupNavbarInsets= */ true);

        WindowInsetsCompat missingNavbarInset =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 0))
                        .setInsets(TAPPABLE_ELEMENT, Insets.of(0, 0, 0, 0))
                        .setInsets(
                                MANDATORY_SYSTEM_GESTURES,
                                Insets.of(0, STATUS_BAR_SIZE, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, missingNavbarInset);
        assertInsetsConsumed(
                newInsets,
                STATUS_BARS + NAVIGATION_BARS + TAPPABLE_ELEMENT + MANDATORY_SYSTEM_GESTURES);

        measureAndLayoutRootView(400, 600);
        assertPaddings(/* left= */ 0, /* top= */ STATUS_BAR_SIZE, /* right= */ 0, /* bottom= */ 0);

        // status bar is with Rect(0,0, WINDOW_WIDTH, STATUS_BAR_SIZE)
        assertEquals(
                "Status bar is at the top of the window.",
                new Rect(0, 0, 400, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Navbar insets are missing, and weaker signals for backup navbar insets should not"
                        + " be used as non-zero navbar insets have been seen.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Navbar insets are missing, and weaker signals for backup navbar insets should not"
                        + " be used as non-zero navbar insets have been seen.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌───────┐
    // ├───────┤
    // │       │
    // │       │
    // ├-------┤
    // │missing│
    // └───────┘
    @Test
    @Config(qualifiers = "w400dp-h600dp")
    public void testPortrait_useMandatoryGesturesForBackupInsets() {
        doReturn(false).when(mInsetObserver).hasSeenNonZeroNavigationBarInsets();
        initializePortraitLayout(/* useBackupNavbarInsets= */ true);

        WindowInsetsCompat missingNavbarInset =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 0))
                        .setInsets(TAPPABLE_ELEMENT, Insets.of(0, 0, 0, 0))
                        .setInsets(
                                MANDATORY_SYSTEM_GESTURES,
                                Insets.of(0, STATUS_BAR_SIZE, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, missingNavbarInset);
        assertInsetsConsumed(
                newInsets,
                STATUS_BARS + NAVIGATION_BARS + TAPPABLE_ELEMENT + MANDATORY_SYSTEM_GESTURES);

        measureAndLayoutRootView(400, 600);
        assertPaddings(
                /* left= */ 0,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ 0,
                /* bottom= */ NAV_BAR_SIZE);

        // status bar is with Rect(0,0, WINDOW_WIDTH, STATUS_BAR_SIZE)
        assertEquals(
                "Status bar is at the top of the window.",
                new Rect(0, 0, 400, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        // Nav bar is with Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar is at the bottom of the screen.",
                new Rect(0, 450, 400, 600),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        // Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar divider is the top 1px height for the nav bar.",
                new Rect(0, 450, 400, 451),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌────────┐
    // ├────────┤
    // │        │
    // │        │
    // └────────┘
    // Case when window is in freeform on certain OEMs. captionBar is introduced in API 30.
    @Test
    @Config(qualifiers = "w400dp-h600dp", sdk = 30)
    public void testCaptionBar() {
        initializePortraitLayout();

        WindowInsetsCompat captionBarInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(CAPTION_BAR, Insets.of(0, CAPTION_BAR_SIZE, 0, 0))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, captionBarInsets);
        assertInsetsConsumed(newInsets, CAPTION_BAR);

        measureAndLayoutRootView(400, 600);
        assertPaddings(/* left= */ 0, /* top= */ CAPTION_BAR_SIZE, /* right= */ 0, /* bottom= */ 0);

        assertEquals(
                "Caption bar rect should be showing.",
                new Rect(0, 0, 400, CAPTION_BAR_SIZE),
                mEdgeToEdgeLayout.getCaptionBarRectForTesting());

        // Both status bar and navigation bar should be empty.
        assertEquals(
                "Status bar insets should be empty.",
                new Rect(),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Nav bar insets should be empty.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Nav bar divider rect should be empty.",
                new Rect(),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    @Test
    @Config(qualifiers = "w400dp-h600dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeDefaultAlways() {
        initializePortraitLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        WindowInsetsCompat cutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(0, CUTOUT_SIZE, 0, 0)).build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, cutoutInsets);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);
    }

    @Test
    @Config(qualifiers = "w400dp-h600dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeNever() {
        initializePortraitLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
        WindowInsetsCompat topCutout =
                newWindowInsetsBuilderWithCutout(new Rect(0, CUTOUT_SIZE, 0, 0)).build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, topCutout);
        assertPaddings(/* left= */ 0, /* top= */ CUTOUT_SIZE, /* right= */ 0, /* bottom= */ 0);

        WindowInsetsCompat leftCutout =
                newWindowInsetsBuilderWithCutout(new Rect(CUTOUT_SIZE, 0, 0, 0)).build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, leftCutout);
        assertPaddings(/* left= */ CUTOUT_SIZE, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);
    }

    @Test
    @Config(qualifiers = "w400dp-h600dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeShortEdge_OnShortEdge() {
        initializePortraitLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        WindowInsetsCompat topCutout =
                newWindowInsetsBuilderWithCutout(new Rect(0, CUTOUT_SIZE, 0, 0)).build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, topCutout);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);
    }

    @Test
    @Config(qualifiers = "w600dp-h400dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeShortEdge_OnShortEdge_Landscape() {
        initializeLandscapeLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        WindowInsetsCompat cutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(CUTOUT_SIZE, 0, 0, 0)).build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, cutoutInsets);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);
    }

    @Test
    @Config(qualifiers = "w400dp-h600dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeShortEdge_OnLongEdge() {
        initializePortraitLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        WindowInsetsCompat rightCutout =
                newWindowInsetsBuilderWithCutout(new Rect(0, 0, CUTOUT_SIZE, 0)).build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, rightCutout);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ CUTOUT_SIZE, /* bottom= */ 0);
    }

    @Test
    @Config(qualifiers = "w600dp-h400dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeShortEdge_OnLongEdge_Landscape() {
        initializeLandscapeLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        WindowInsetsCompat cutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(0, CUTOUT_SIZE, 0, 0)).build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, cutoutInsets);
        assertPaddings(/* left= */ 0, /* top= */ CUTOUT_SIZE, /* right= */ 0, /* bottom= */ 0);
    }

    @Test
    @Config(qualifiers = "w400dp-h600dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeDefault_InsetOverlap() {
        initializePortraitLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        WindowInsetsCompat topStatusAndCutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                mEdgeToEdgeLayout, topStatusAndCutoutInsets);
        assertPaddings(/* left= */ 0, /* top= */ STATUS_BAR_SIZE, /* right= */ 0, /* bottom= */ 0);
    }

    @Test
    @Config(qualifiers = "w400dp-h600dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeDefault_FullscreenPortrait() {
        initializePortraitLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        // Status Bar and Navigation Bar insets are zeros when in fullscreen.
        WindowInsetsCompat topCutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(0, CUTOUT_SIZE, 0, 0))
                        .setInsets(STATUS_BARS, Insets.of(0, 0, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 0))
                        .build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, topCutoutInsets);
        assertPaddings(/* left= */ 0, /* top= */ CUTOUT_SIZE, /* right= */ 0, /* bottom= */ 0);
    }

    @Test
    @Config(qualifiers = "w600dp-h400dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeDefault_FullscreenLandscape() {
        initializeLandscapeLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        // Status Bar and Navigation Bar insets are zeros when in fullscreen.
        WindowInsetsCompat leftCutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(CUTOUT_SIZE, 0, 0, 0))
                        .setInsets(STATUS_BARS, Insets.of(0, 0, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 0))
                        .build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, leftCutoutInsets);
        assertPaddings(/* left= */ CUTOUT_SIZE, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);

        // Status Bar and Navigation Bar insets are zeros when in fullscreen.
        WindowInsetsCompat rightCutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(0, 0, CUTOUT_SIZE, 0))
                        .setInsets(STATUS_BARS, Insets.of(0, 0, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 0))
                        .build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, rightCutoutInsets);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ CUTOUT_SIZE, /* bottom= */ 0);
    }

    @Test
    @Config(qualifiers = "w600dp-h400dp")
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeDefault_InsetNotOverlap() {
        initializeLandscapeLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        WindowInsetsCompat topStatusLeftCutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(CUTOUT_SIZE, 0, 0, 0))
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                mEdgeToEdgeLayout, topStatusLeftCutoutInsets);
        assertPaddings(
                /* left= */ CUTOUT_SIZE,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ 0,
                /* bottom= */ 0);
    }

    @Test
    @SuppressLint("NewApi") // layoutInDisplayCutoutMode required sdk 28+
    public void testDisplayCutoutModeDefault_SmallCutoutInset() {
        initializePortraitLayout();

        mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        WindowInsetsCompat cutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(SMALL_CUTOUT_SIZE, 0, 0, 0)).build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, cutoutInsets);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);

        cutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(0, SMALL_CUTOUT_SIZE, 0, 0)).build();
        mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, cutoutInsets);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);
    }

    // ┌───┬─────────────┐
    // │   ├─────────────┤
    // │   │             │
    // │   │             │
    // └───┴─────────────┘
    @Test
    @Config(qualifiers = "w600dp-h400dp")
    public void testLandscape_LeftNavBar() {
        initializeLandscapeLayout();

        WindowInsetsCompat topLeftInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(NAV_BAR_SIZE, 0, 0, 0))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, topLeftInsets);
        assertInsetsConsumed(newInsets, STATUS_BAR_SIZE + NAV_BAR_SIZE);

        measureAndLayoutRootView(600, 400);
        assertPaddings(
                /* left= */ NAV_BAR_SIZE,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ 0,
                /* bottom= */ 0);

        mEdgeToEdgeLayout.measure(-1, -1);
        // status bar is with Rect(NAV_BAR_SIZE, 0, WINDOW_WIDTH, STATUS_BAR_SIZE)
        assertEquals(
                "Status bar is at the top of the window, avoid overlap with nav bar.",
                new Rect(150, 0, 600, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        // Nav bar is with Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar is at the left of the screen.",
                new Rect(0, 0, 150, 400),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        // Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar divider is the right most 1px for the nav bar.",
                new Rect(149, 0, 150, 400),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┌───────────────┬───┐
    // ├───────────────┤   │
    // │               │   │
    // │               │   │
    // └───────────────┴───┘
    @Test
    @Config(qualifiers = "w600dp-h400dp")
    public void testLandscape_RightNavBar() {
        initializeLandscapeLayout();

        WindowInsetsCompat topRightInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, NAV_BAR_SIZE, 0))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, topRightInsets);
        assertInsetsConsumed(newInsets, STATUS_BAR_SIZE + NAV_BAR_SIZE);
        measureAndLayoutRootView(600, 400);
        assertPaddings(
                /* left= */ 0,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ NAV_BAR_SIZE,
                /* bottom= */ 0);

        assertEquals(
                "Status bar is at the top of the window, avoid overlap with nav bar.",
                new Rect(0, 0, 450, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Nav bar is at the right of the screen.",
                new Rect(450, 0, 600, 400),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Nav bar divider is the left most 1px for the nav bar.",
                new Rect(450, 0, 451, 400),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    // ┏━┱───────────────┬───┐
    // ┃╳┠───────────────┤   │
    // ┃╳┃               │   │
    // ┃╳┃               │   │
    // ┗━┹───────────────┴───┘
    @Test
    @Config(qualifiers = "w600dp-h400dp")
    public void testLandscape_RightNavBarLeftCutout() {
        initializeLandscapeLayout();

        WindowInsetsCompat topRightSysBarsLeftCutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(CUTOUT_SIZE, 0, 0, 0))
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, NAV_BAR_SIZE, 0))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, topRightSysBarsLeftCutoutInsets);
        assertInsetsConsumed(newInsets, ALL_SUPPORTED_INSETS);

        measureAndLayoutRootView(600, 400);
        assertPaddings(
                /* left= */ CUTOUT_SIZE,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ NAV_BAR_SIZE,
                /* bottom= */ 0);

        assertEquals(
                "Status bar is at the top of the window, avoid overlap with display cutout and nav"
                        + " bar.",
                new Rect(75, 0, 450, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Nav bar is at the right of the screen.",
                new Rect(450, 0, 600, 400),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Nav bar divider is the left most 1px for the nav bar.",
                new Rect(450, 0, 451, 400),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
        assertEquals(
                "Display cutout is at the left of the window.",
                new Rect(0, 0, 75, 400),
                mEdgeToEdgeLayout.getCutoutRectLeftForTesting());
    }

    // ┌───┬─────────────┲━┓
    // │   ├─────────────┨╳┃
    // │   │             ┃╳┃
    // │   │             ┃╳┃
    // └───┴─────────────┺━┛
    @Test
    @Config(qualifiers = "w600dp-h400dp")
    public void testLandscape_LeftNavBarRightCutout() {
        initializeLandscapeLayout();

        WindowInsetsCompat topRightSysBarsLeftCutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(0, 0, CUTOUT_SIZE, 0))
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(NAV_BAR_SIZE, 0, 0, 0))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, topRightSysBarsLeftCutoutInsets);
        assertInsetsConsumed(newInsets, ALL_SUPPORTED_INSETS);

        measureAndLayoutRootView(600, 400);
        assertPaddings(
                /* left= */ NAV_BAR_SIZE,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ CUTOUT_SIZE,
                /* bottom= */ 0);

        assertEquals(
                "Status bar is at the top of the window, avoid overlap with display cutout and nav"
                        + " bar.",
                new Rect(150, 0, 525, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Nav bar is at the right of the screen.",
                new Rect(0, 0, 150, 400),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Nav bar divider is the left most 1px for the nav bar.",
                new Rect(149, 0, 150, 400),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());

        assertEquals(
                "Display cutout left is empty.",
                new Rect(),
                mEdgeToEdgeLayout.getCutoutRectLeftForTesting());
        assertEquals(
                "Display cutout is at the right of the window.",
                new Rect(525, 0, 600, 400),
                mEdgeToEdgeLayout.getCutoutRectRightForTesting());
    }

    // ┏━┱──────────────────┐
    // ┃╳┠──────────────────┤
    // ┃╳┃                  │
    // ┃╳┠──────────────────┤
    // ┗━┹──────────────────┘
    @Test
    @Config(qualifiers = "w600dp-h400dp")
    public void testLandscape_GestureModeLeftCutout() {
        initializeLandscapeLayout();

        WindowInsetsCompat topBottomSysBarsLeftCutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(CUTOUT_SIZE, 0, 0, 0))
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, topBottomSysBarsLeftCutoutInsets);
        assertEquals(
                "Window insets should be consumed",
                Insets.NONE,
                newInsets.getInsets(SYSTEM_BARS + DISPLAY_CUTOUT));
        assertInsetsConsumed(newInsets, ALL_SUPPORTED_INSETS);

        measureAndLayoutRootView(600, 400);
        assertPaddings(
                /* left= */ CUTOUT_SIZE,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ 0,
                /* bottom= */ NAV_BAR_SIZE);

        assertEquals(
                "Status bar is at the top of the window, avoid overlap with display cutout.",
                new Rect(75, 0, 600, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Nav bar is at the bottom of the screen, avoid overlap with display cutout.",
                new Rect(75, 250, 600, 400),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Nav bar divider is the left most 1px for the nav bar.",
                new Rect(75, 250, 600, 251),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
        assertEquals(
                "Display cutout is at the left of the window.",
                new Rect(0, 0, 75, 400),
                mEdgeToEdgeLayout.getCutoutRectLeftForTesting());
        assertEquals(
                "Display cutout is at the left of the window.",
                new Rect(),
                mEdgeToEdgeLayout.getCutoutRectRightForTesting());
    }

    // ┌─────────────────┲━┓
    // ├─────────────────┨╳┃
    // │                 ┃╳┃
    // ├─────────────────┨╳┃
    // └─────────────────┺━┛
    @Test
    @Config(qualifiers = "w600dp-h400dp")
    public void testLandscape_GestureModeRightCutout() {
        initializeLandscapeLayout();

        WindowInsetsCompat topBottomSysBarsLeftCutoutInsets =
                newWindowInsetsBuilderWithCutout(new Rect(0, 0, CUTOUT_SIZE, 0))
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                        mEdgeToEdgeLayout, topBottomSysBarsLeftCutoutInsets);
        assertInsetsConsumed(newInsets, ALL_SUPPORTED_INSETS);

        measureAndLayoutRootView(600, 400);
        assertPaddings(
                /* left= */ 0,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ CUTOUT_SIZE,
                /* bottom= */ NAV_BAR_SIZE);

        assertEquals(
                "Status bar is at the top of the window, avoid overlap with display cutout.",
                new Rect(0, 0, 525, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        assertEquals(
                "Nav bar is at the bottom of the screen, avoid overlap with display cutout.",
                new Rect(0, 250, 525, 400),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        assertEquals(
                "Nav bar divider is the left most 1px for the nav bar.",
                new Rect(0, 250, 525, 251),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
        assertEquals(
                "Display cutout left is empty.",
                new Rect(),
                mEdgeToEdgeLayout.getCutoutRectLeftForTesting());
        assertEquals(
                "Display cutout is at the right of the window.",
                new Rect(525, 0, 600, 400),
                mEdgeToEdgeLayout.getCutoutRectRightForTesting());
    }

    // ┌───┬─────────────┐
    // │   ├─────────────┤
    // │   │-------------│
    // │   │  keyboard   │
    // └───┴─────────────┘
    @Test
    @Config(qualifiers = "w600dp-h400dp")
    public void testLandscape_ImeWithLeftNavBar() {
        initializeLandscapeLayout();

        WindowInsetsCompat topLeftInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(STATUS_BARS, Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(NAV_BAR_SIZE, 0, 0, 0))
                        .setInsets(IME, Insets.of(0, 0, 0, IME_SIZE))
                        .build();
        WindowInsetsCompat newInsets =
                mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mEdgeToEdgeLayout, topLeftInsets);
        assertInsetsConsumed(newInsets, ALL_SUPPORTED_INSETS);

        measureAndLayoutRootView(600, 400);
        assertPaddings(
                /* left= */ NAV_BAR_SIZE,
                /* top= */ STATUS_BAR_SIZE,
                /* right= */ 0,
                /* bottom= */ IME_SIZE);

        mEdgeToEdgeLayout.measure(-1, -1);
        // status bar is with Rect(NAV_BAR_SIZE, 0, WINDOW_WIDTH, STATUS_BAR_SIZE)
        assertEquals(
                "Status bar is at the top of the window, avoid overlap with nav bar.",
                new Rect(150, 0, 600, 100),
                mEdgeToEdgeLayout.getStatusBarRectForTesting());
        // Nav bar is with Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar is at the left of the screen.",
                new Rect(0, 0, 150, 400),
                mEdgeToEdgeLayout.getNavigationBarRectForTesting());
        // Rect(0, WINDOW_SIZE - NAV_BAR_SIZE, WINDOW_WIDTH, WINDOW_SIZE)
        assertEquals(
                "Nav bar divider is the right most 1px for the nav bar.",
                new Rect(149, 0, 150, 400),
                mEdgeToEdgeLayout.getNavigationBarDividerRectForTesting());
    }

    private void initialize(InsetObserver insetObserver, boolean useBackupNavbarInsets) {
        mEdgeToEdgeLayoutCoordinator =
                new EdgeToEdgeLayoutCoordinator(
                        mActivity,
                        insetObserver,
                        /* useBackupNavbarInsetsEnabled= */ useBackupNavbarInsets,
                        /* useBackupNavbarInsetsFieldTrial= */ mUseBackupNavbarFieldTrial,
                        /* canUseMandatoryGesturesInsets= */ true);
        mEdgeToEdgeLayout =
                (EdgeToEdgeBaseLayout)
                        mEdgeToEdgeLayoutCoordinator.wrapContentView(mOriginalContentView);
        mActivity.setContentView(mEdgeToEdgeLayout);
    }

    private void initializePortraitLayout() {
        initialize(mInsetObserver, true);
        measureAndLayoutRootView(400, 600);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);
    }

    private void initializePortraitLayout(boolean useBackupNavbarInsets) {
        initialize(mInsetObserver, /* useBackupNavbarInsets= */ useBackupNavbarInsets);
        measureAndLayoutRootView(400, 600);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);
    }

    private void initializeLandscapeLayout() {
        initialize(null, true);
        measureAndLayoutRootView(600, 400);
        assertPaddings(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 0);
    }

    private void assertPaddings(int left, int top, int right, int bottom) {
        assertEquals("Padding left is wrong.", left, mEdgeToEdgeLayout.getPaddingLeft());
        assertEquals("Padding top is wrong.", top, mEdgeToEdgeLayout.getPaddingTop());
        assertEquals("Padding right is wrong.", right, mEdgeToEdgeLayout.getPaddingRight());
        assertEquals("Padding bottom is wrong.", bottom, mEdgeToEdgeLayout.getPaddingBottom());
    }

    private void assertInsetsConsumed(WindowInsetsCompat windowInsets, int type) {
        assertEquals(
                "Window insets should be consumed for type: " + type,
                Insets.NONE,
                windowInsets.getInsets(type));
    }

    private void measureAndLayoutRootView(int width, int height) {
        mEdgeToEdgeLayout.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);
        mEdgeToEdgeLayout.layout(0, 0, width, height);
    }

    private WindowInsetsCompat.Builder newWindowInsetsBuilderWithCutout(Rect displayCutout) {
        return new WindowInsetsCompat.Builder()
                .setInsets(DISPLAY_CUTOUT, Insets.of(displayCutout))
                .setDisplayCutout(new DisplayCutoutCompat(displayCutout, null));
    }
}
