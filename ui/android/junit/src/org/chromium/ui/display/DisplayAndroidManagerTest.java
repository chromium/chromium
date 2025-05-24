// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.display;

import static org.junit.Assert.assertEquals;

import android.os.Build;
import android.util.SparseArray;
import android.view.Display;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDisplayManager;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.base.UiAndroidFeatures;

import java.util.HashSet;

/** Tests logic in the {@link DisplayAndroidManager} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.S)
@EnableFeatures({UiAndroidFeatures.ANDROID_WINDOW_MANAGEMENT_WEB_API})
public class DisplayAndroidManagerTest {
    @After
    public void after() {
        DisplayAndroidManager.resetInstanceForTesting();
    }

    @Test
    @DisableFeatures({UiAndroidFeatures.ANDROID_WINDOW_MANAGEMENT_WEB_API})
    public void testDisplayAndroidManagerMainFlowWithDisabledDisplayRegistration() {
        final HashSet<Integer> expectedDisplayIds = new HashSet<Integer>();

        // DisplayManager has a default display with the Display.DEFAULT_DISPLAY id.
        expectedDisplayIds.add(Display.DEFAULT_DISPLAY);

        // Display won't be registered.
        final Integer firstExternalDisplay = ShadowDisplayManager.addDisplay("");

        final DisplayAndroidManager displayAndroidManager = DisplayAndroidManager.getInstance();

        // Check initialization with multiple displays.
        checkDisplays(
                "The initialization of DisplayAndroidManager with disabled display registration is"
                        + " incorrect.",
                expectedDisplayIds,
                displayAndroidManager.mIdMap);

        // Display won't be registered.
        ShadowDisplayManager.addDisplay("");
        checkDisplays(
                "Display adding with disabled display registration is incorrect.",
                expectedDisplayIds,
                displayAndroidManager.mIdMap);

        // Display wasn't registered, so nothing should be changed.
        ShadowDisplayManager.removeDisplay(firstExternalDisplay);
        checkDisplays(
                "Display removing with disabled display registration is incorrect.",
                expectedDisplayIds,
                displayAndroidManager.mIdMap);

        // Display won't be registered.
        ShadowDisplayManager.addDisplay("");
        checkDisplays(
                "Display adding after removing with disabled display registration is incorrect.",
                expectedDisplayIds,
                displayAndroidManager.mIdMap);
    }

    @Test
    public void testDisplayAndroidManagerMainFlow() {
        final HashSet<Integer> expectedDisplayIds = new HashSet<Integer>();

        // DisplayManager has a default display with the Display.DEFAULT_DISPLAY id.
        expectedDisplayIds.add(Display.DEFAULT_DISPLAY);

        final Integer firstExternalDisplay = ShadowDisplayManager.addDisplay("");
        expectedDisplayIds.add(firstExternalDisplay);

        final DisplayAndroidManager displayAndroidManager = DisplayAndroidManager.getInstance();

        // Check initialization with multiple displays.
        checkDisplays(
                "The initialization of DisplayAndroidManager is incorrect.",
                expectedDisplayIds,
                displayAndroidManager.mIdMap);

        // Check display adding.
        final Integer secondExternalDisplay = ShadowDisplayManager.addDisplay("");
        expectedDisplayIds.add(secondExternalDisplay);
        checkDisplays(
                "Display adding is incorrect.", expectedDisplayIds, displayAndroidManager.mIdMap);

        // Check display removing.
        ShadowDisplayManager.removeDisplay(firstExternalDisplay);
        expectedDisplayIds.remove(firstExternalDisplay);
        checkDisplays(
                "Display removing is incorrect.", expectedDisplayIds, displayAndroidManager.mIdMap);

        // Check display adding after removing.
        final Integer thirdExternalDisplay = ShadowDisplayManager.addDisplay("");
        expectedDisplayIds.add(thirdExternalDisplay);
        checkDisplays(
                "Display adding after removing is incorrect.",
                expectedDisplayIds,
                displayAndroidManager.mIdMap);
    }

    @Test
    public void testDisplayAddHistogram() {
        final DisplayAndroidManager displayAndroidManager = DisplayAndroidManager.getInstance();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                DisplayAndroidManager.DisplayListenerBackend
                                        .IS_NULL_DISPLAY_REMOVED_HISTOGRAM_NAME)
                        .build();

        // Adding correct display.
        ShadowDisplayManager.addDisplay("");
        ShadowLooper.runMainLooperToNextTask();

        histogramWatcher.assertExpected("Adding correct display. Shouldn't record anything.");

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                DisplayAndroidManager.DisplayListenerBackend
                                        .IS_NULL_DISPLAY_REMOVED_HISTOGRAM_NAME,
                                false)
                        .expectBooleanRecord(
                                DisplayAndroidManager.DisplayListenerBackend
                                        .IS_NULL_DISPLAY_REMOVED_HISTOGRAM_NAME,
                                true)
                        .build();

        final int incorrectDisplayId = -1;

        // Adding incorrect display
        displayAndroidManager.mBackend.onDisplayAdded(incorrectDisplayId);
        ShadowLooper.runMainLooperToNextTask();

        // Adding incorrect display and deleting it immediately.
        displayAndroidManager.mBackend.onDisplayAdded(incorrectDisplayId);
        displayAndroidManager.mBackend.onDisplayRemoved(incorrectDisplayId);
        ShadowLooper.runMainLooperToNextTask();

        histogramWatcher.assertExpected("Incorrect histogram values.");
    }

    private void checkDisplays(
            String message,
            HashSet<Integer> expectedDisplayIds,
            SparseArray<DisplayAndroid> actualDisplays) {
        HashSet<Integer> actualDisplayIds = new HashSet<Integer>();
        for (int i = 0; i < actualDisplays.size(); ++i) {
            actualDisplayIds.add(actualDisplays.valueAt(i).getDisplayId());
        }

        assertEquals(message, expectedDisplayIds, actualDisplayIds);
    }
}
