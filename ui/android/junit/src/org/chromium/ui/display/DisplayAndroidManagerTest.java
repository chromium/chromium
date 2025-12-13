// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.display;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.RectF;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.util.DisplayMetrics;
import android.util.SparseArray;
import android.view.Display;
import android.view.WindowManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDisplay;
import org.robolectric.shadows.ShadowDisplayManager;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.base.UiAndroidFeatures;

import java.util.HashSet;

/** Tests logic in the {@link DisplayAndroidManager} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.S)
@EnableFeatures({UiAndroidFeatures.ANDROID_USE_DISPLAY_TOPOLOGY})
public class DisplayAndroidManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AconfigFlaggedApiDelegate mAconfigFlaggedApiDelegate;

    private final DisplayManager mDisplayManager =
            (DisplayManager)
                    ContextUtils.getApplicationContext().getSystemService(Context.DISPLAY_SERVICE);
    private final SparseArray<RectF> mBounds = new SparseArray<RectF>();

    @Before
    public void setup() {
        // DisplayManager has a default display with the Display.DEFAULT_DISPLAY id.
        final ShadowDisplay defaultDisplay =
                Shadows.shadowOf(mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY));
        defaultDisplay.setWidth(1920);
        defaultDisplay.setHeight(1080);

        mBounds.put(Display.DEFAULT_DISPLAY, new RectF(0, 0, 1920, 1080));

        AconfigFlaggedApiDelegate.setInstanceForTesting(mAconfigFlaggedApiDelegate);
        doReturn(true).when(mAconfigFlaggedApiDelegate).isDisplayTopologyAvailable(mDisplayManager);

        doReturn(mBounds.clone())
                .when(mAconfigFlaggedApiDelegate)
                .getAbsoluteBounds(mDisplayManager);
    }

    @After
    public void teardown() {
        DisplayAndroidManager.resetInstanceForTesting();
        DisplayAndroidManager.resetIsDisplayTopologyAvailableForTesting();
        ShadowDisplayManager.reset();
        mBounds.clear();
    }

    @Test
    @DisableFeatures({UiAndroidFeatures.ANDROID_USE_DISPLAY_TOPOLOGY})
    public void testDisplayAndroidManager() {
        // Display won't be registered.
        final int firstExternalDisplay = ShadowDisplayManager.addDisplay("");

        final DisplayAndroidManager displayAndroidManager = DisplayAndroidManager.getInstance();

        // Check initialization with multiple displays.
        checkDisplays(
                "The initialization of DisplayAndroidManager with disabled display registration is"
                        + " incorrect.",
                displayAndroidManager.mIdMap);

        // Display won't be registered.
        ShadowDisplayManager.addDisplay("");
        checkDisplays(
                "Display adding with disabled display registration is incorrect.",
                displayAndroidManager.mIdMap);

        // Display wasn't registered, so nothing should be changed.
        ShadowDisplayManager.removeDisplay(firstExternalDisplay);
        checkDisplays(
                "Display removing with disabled display registration is incorrect.",
                displayAndroidManager.mIdMap);

        // Display won't be registered.
        ShadowDisplayManager.addDisplay("");
        checkDisplays(
                "Display adding after removing with disabled display registration is incorrect.",
                displayAndroidManager.mIdMap);
    }

    @Test
    public void testDisplayTopology() {
        final int firstExternalDisplayId =
                addDisplay(3840, 2160, DisplayMetrics.DENSITY_HIGH, "firstExternalDisplay");
        mBounds.put(firstExternalDisplayId, new RectF(0, -1440, 2560, 0));

        doReturn(mBounds.clone())
                .when(mAconfigFlaggedApiDelegate)
                .getAbsoluteBounds(mDisplayManager);

        final DisplayAndroidManager displayAndroidManager = DisplayAndroidManager.getInstance();

        // Check initialization with multiple displays.
        checkDisplays(
                "The initialization of DisplayAndroidManager is incorrect.",
                displayAndroidManager.mIdMap);

        // Display shouldn't be added through DisplayListener.
        final int secondExternalDisplayId =
                addDisplay(1280, 720, DisplayMetrics.DENSITY_200, "secondExternalDisplay");
        checkDisplays(
                "Display shouldn't be added through DisplayListener.",
                displayAndroidManager.mIdMap);

        // Display should be added through DisplayTopologyListener.
        mBounds.put(secondExternalDisplayId, new RectF(-1024, -288, 0, 288));
        updateDisplayTopology(displayAndroidManager);
        checkDisplays(
                "Display should be added through DisplayTopologyListener.",
                displayAndroidManager.mIdMap);

        // Display shouldn't be removed through DisplayListener.
        ShadowDisplayManager.removeDisplay(secondExternalDisplayId);
        checkDisplays(
                "Display shouldn't be removed through DisplayListener.",
                displayAndroidManager.mIdMap);

        // Display should be removed through DisplayTopologyListener.
        mBounds.remove(secondExternalDisplayId);
        updateDisplayTopology(displayAndroidManager);
        checkDisplays(
                "Display should be removed through DisplayTopologyListener.",
                displayAndroidManager.mIdMap);

        // Check display adding after removing.
        final int thirdExternalDisplayId =
                addDisplay(1280, 720, DisplayMetrics.DENSITY_200, "thirdExternalDisplay");
        mBounds.put(thirdExternalDisplayId, new RectF(-1024, -288, 0, 288));
        updateDisplayTopology(displayAndroidManager);
        checkDisplays("Display adding after removing is incorrect.", displayAndroidManager.mIdMap);
    }

    @Test
    public void testDisplayTopologyUpdates() {
        final int externalDisplayId =
                addDisplay(3840, 2160, DisplayMetrics.DENSITY_HIGH, "externalDisplay");
        mBounds.put(externalDisplayId, new RectF(0, -1440, 2560, 0));

        doReturn(mBounds.clone())
                .when(mAconfigFlaggedApiDelegate)
                .getAbsoluteBounds(mDisplayManager);

        final DisplayAndroidManager displayAndroidManager = DisplayAndroidManager.getInstance();

        assertEquals(
                "BuiltIn Display bounds incorrect after initialization.",
                new Rect(0, 0, 1920, 1080),
                displayAndroidManager.mIdMap.get(Display.DEFAULT_DISPLAY).getBounds());
        assertEquals(
                "External Display bounds incorrect after initialization.",
                new Rect(0, -1440, 2560, 0),
                displayAndroidManager.mIdMap.get(externalDisplayId).getBounds());

        mBounds.put(externalDisplayId, new RectF(0, -1080, 1920, 0));

        // Update bounds.
        updateDisplayTopology(displayAndroidManager);

        assertEquals(
                "BuiltIn Display absolute bounds incorrect after bounds update.",
                new Rect(0, 0, 1920, 1080),
                displayAndroidManager.mIdMap.get(Display.DEFAULT_DISPLAY).getBounds());
        assertEquals(
                "External Display absolute bounds incorrect after bounds update.",
                new Rect(0, -1080, 1920, 0),
                displayAndroidManager.mIdMap.get(externalDisplayId).getBounds());

        // DisplayListenerBackend.onDisplayChanged should process this update.
        ShadowDisplayManager.changeDisplay(externalDisplayId, "+");

        assertEquals(
                "BuiltIn Display absolute bounds incorrect after density update.",
                new Rect(0, 0, 1920, 1080),
                displayAndroidManager.mIdMap.get(Display.DEFAULT_DISPLAY).getBounds());
        assertEquals(
                "External Display absolute bounds incorrect after density update.",
                new Rect(0, -1080, 1920, 0),
                displayAndroidManager.mIdMap.get(externalDisplayId).getBounds());
    }

    @Test
    public void testIsDisplayTopologyAvailableHistogram() {
        final HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                DisplayAndroidManager.IS_DISPLAY_TOPOLOGY_AVAILABLE_HISTOGRAM_NAME,
                                false)
                        .expectBooleanRecord(
                                DisplayAndroidManager.IS_DISPLAY_TOPOLOGY_AVAILABLE_HISTOGRAM_NAME,
                                true)
                        .build();

        // ANDROID_USE_DISPLAY_TOPOLOGY is enabled and
        // mAconfigFlaggedApiDelegate.isDisplayTopologyAvailable() is true
        DisplayAndroidManager displayAndroidManager = DisplayAndroidManager.getInstance();

        DisplayAndroidManager.resetInstanceForTesting();
        DisplayAndroidManager.resetIsDisplayTopologyAvailableForTesting();
        doReturn(false)
                .when(mAconfigFlaggedApiDelegate)
                .isDisplayTopologyAvailable(mDisplayManager);

        // ANDROID_USE_DISPLAY_TOPOLOGY is enabled, but
        // mAconfigFlaggedApiDelegate.isDisplayTopologyAvailable() is false
        displayAndroidManager = DisplayAndroidManager.getInstance();

        histogramWatcher.assertExpected("Incorrect histogram values.");
    }

    @Test
    public void testDisplayAddHistogram() {
        final DisplayAndroidManager displayAndroidManager = DisplayAndroidManager.getInstance();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                DisplayAndroidManager.IS_NULL_DISPLAY_REMOVED_HISTOGRAM_NAME)
                        .build();

        // Adding correct display.
        final int correctDisplayId = ShadowDisplayManager.addDisplay("");
        mBounds.put(correctDisplayId, new RectF(0, -1440, 2560, 0));
        updateDisplayTopology(displayAndroidManager);

        ShadowLooper.runMainLooperToNextTask();

        histogramWatcher.assertExpected("Adding correct display. Shouldn't record anything.");

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                DisplayAndroidManager.IS_NULL_DISPLAY_REMOVED_HISTOGRAM_NAME, false)
                        .expectBooleanRecord(
                                DisplayAndroidManager.IS_NULL_DISPLAY_REMOVED_HISTOGRAM_NAME, true)
                        .build();

        final int firstIncorrectDisplayId = -1;
        mBounds.put(firstIncorrectDisplayId, new RectF(0, -1440, 2560, 0));

        // Adding incorrect display
        updateDisplayTopology(displayAndroidManager);
        ShadowLooper.runMainLooperToNextTask();

        // Reset state
        mBounds.remove(firstIncorrectDisplayId);
        updateDisplayTopology(displayAndroidManager);

        final int secondIncorrectDisplayId = -2;
        // Adding incorrect display and deleting it immediately.
        mBounds.put(secondIncorrectDisplayId, new RectF(0, -1440, 2560, 0));
        updateDisplayTopology(displayAndroidManager);
        mBounds.remove(secondIncorrectDisplayId);
        updateDisplayTopology(displayAndroidManager);
        ShadowLooper.runMainLooperToNextTask();

        histogramWatcher.assertExpected("Incorrect histogram values.");
    }

    private int addDisplay(int width, int height, int densityDpi, String name) {
        final int displayId =
                ShadowDisplayManager.addDisplay(String.format("w%ddp-h%ddp", width, height), name);
        updateDensity(displayId, densityDpi);

        return displayId;
    }

    private void updateDensity(int displayId, int densityDpi) {
        // Density can be changed only with an explicit configuration update.
        final Context displayWindowContext =
                ContextUtils.getApplicationContext()
                        .createWindowContext(
                                mDisplayManager.getDisplay(displayId),
                                WindowManager.LayoutParams.TYPE_APPLICATION,
                                null);
        final Resources resources = displayWindowContext.getResources();
        final Configuration configuration = resources.getConfiguration();

        configuration.densityDpi = densityDpi;

        resources.updateConfiguration(configuration, resources.getDisplayMetrics());
    }

    private void updateDisplayTopology(DisplayAndroidManager displayAndroidManager) {
        displayAndroidManager.mDisplayTopologyListenerBackend.onDisplayTopologyChanged(
                mBounds.clone());
    }

    private void checkDisplays(String message, SparseArray<DisplayAndroid> mIdMap) {
        final HashSet<Integer> actualDisplayIds = new HashSet<>();
        for (int i = 0; i < mIdMap.size(); ++i) {
            actualDisplayIds.add(mIdMap.valueAt(i).getDisplayId());
        }

        final HashSet<Integer> expectedDisplayIds = new HashSet<>();
        for (int i = 0; i < mBounds.size(); ++i) {
            expectedDisplayIds.add(mBounds.keyAt(i));
        }

        assertEquals(message, expectedDisplayIds, actualDisplayIds);
    }
}
