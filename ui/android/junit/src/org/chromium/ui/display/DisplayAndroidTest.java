// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.display;

import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.view.Display;

import androidx.annotation.Nullable;

import com.google.common.collect.Lists;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowDisplay;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;

import java.util.List;

/** Tests logic in the {@link DisplayAndroid} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class DisplayAndroidTest {
    @Rule public final MockitoRule rule = MockitoJUnit.rule();

    private final DisplayAndroid mDisplay =
            DisplayAndroid.getNonMultiDisplay(ContextUtils.getApplicationContext());

    @Mock private DisplayAndroidObserver mObserver;

    private Display.Mode mMode;

    @Before
    public void setUp() {
        // Display.Mode doesn't have a public constructor. This is the safest way to create a mode.
        mMode = ShadowDisplay.getDefaultDisplay().getMode();
    }

    @After
    public void tearDown() {
        mDisplay.removeObserver(mObserver);
    }

    @Test
    public void testUpdate_nullDisplayModes_shouldNotNotify() {
        // Put DisplayAndroid in a known initial state.
        mDisplay.addObserver(mObserver);
        List<Display.Mode> defaultDisplayModes = Lists.newArrayList(mMode);
        updateDisplayWithSupportedModes(defaultDisplayModes, /* arrInfo= */ null);
        reset(mObserver);

        updateDisplayWithSupportedModes(null, /* arrInfo= */ null);

        verifyNoInteractions(mObserver);
    }

    @Test
    public void testUpdate_equalDisplayModes_shouldNotNotify() {
        // Put DisplayAndroid in a known initial state.
        mDisplay.addObserver(mObserver);
        List<Display.Mode> defaultDisplayModes = Lists.newArrayList(mMode);
        updateDisplayWithSupportedModes(defaultDisplayModes, /* arrInfo= */ null);
        reset(mObserver);

        List<Display.Mode> equalDisplayModes = Lists.newArrayList(mMode);
        updateDisplayWithSupportedModes(equalDisplayModes, /* arrInfo= */ null);

        verifyNoInteractions(mObserver);
    }

    @Test
    public void testUpdate_differentDisplayModes_shouldNotify() {
        // Put DisplayAndroid in a known initial state.
        mDisplay.addObserver(mObserver);
        List<Display.Mode> defaultDisplayModes = Lists.newArrayList(mMode);
        updateDisplayWithSupportedModes(defaultDisplayModes, /* arrInfo= */ null);
        reset(mObserver);

        List<Display.Mode> differentDisplayModes = Lists.newArrayList(mMode, mMode);
        updateDisplayWithSupportedModes(differentDisplayModes, /* arrInfo= */ null);

        verify(mObserver).onDisplayModesChanged(differentDisplayModes);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    public void testUpdate_arrInfo() {
        // Put DisplayAndroid in a known initial state.
        mDisplay.addObserver(mObserver);
        DisplayAndroid.AdaptiveRefreshRateInfo arrInfo =
                new DisplayAndroid.AdaptiveRefreshRateInfo(true, 120.0f);
        updateDisplayWithSupportedModes(null, arrInfo);

        verify(mObserver).onAdaptiveRefreshRateInfoChanged(arrInfo);
        verifyNoMoreInteractions(mObserver);
    }

    private void updateDisplayWithSupportedModes(
            @Nullable List<Display.Mode> supportedModes,
            @Nullable DisplayAndroid.AdaptiveRefreshRateInfo arrInfo) {
        mDisplay.update(
                /* name= */ null,
                /* bounds= */ null,
                /* workArea= */ null,
                /* width= */ null,
                /* height= */ null,
                /* dipScale= */ null,
                /* xdpi= */ null,
                /* ydpi= */ null,
                /* bitsPerPixel= */ null,
                /* bitsPerComponent= */ null,
                /* rotation= */ null,
                /* isDisplayWideColorGamut= */ null,
                /* isDisplayServerWideColorGamut= */ null,
                /* refreshRate= */ null,
                /* currentMode= */ null,
                supportedModes,
                /* isHdr= */ null,
                /* hdrMaxLuminanceRatio= */ null,
                /* isInternal= */ null,
                arrInfo);
    }
}
