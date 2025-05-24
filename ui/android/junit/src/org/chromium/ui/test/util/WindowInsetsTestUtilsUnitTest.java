// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import static org.junit.Assert.assertEquals;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.test.util.WindowInsetsTestUtils.SpyWindowInsetsBuilder;

/** Test for {@link WindowInsetsTestUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WindowInsetsTestUtilsUnitTest {
    private static final int STATUS_BARS = WindowInsetsCompat.Type.statusBars();
    private static final int NAVIGATION_BARS = WindowInsetsCompat.Type.navigationBars();
    private static final int CAPTION_BAR = WindowInsetsCompat.Type.captionBar();
    private static final int SYSTEM_BARS = WindowInsetsCompat.Type.systemBars();
    private static final int DISPLAY_CUTOUT = WindowInsetsCompat.Type.displayCutout();
    private static final int IME = WindowInsetsCompat.Type.ime();
    private static final int ALL_INSETS = SYSTEM_BARS + DISPLAY_CUTOUT + IME;

    @Test
    public void spyWindowInsetsBuilder_navBars() {
        WindowInsetsCompat insets =
                new SpyWindowInsetsBuilder()
                        .setInsets(NAVIGATION_BARS, Insets.of(1, 2, 3, 4))
                        .build();

        assertEquals(Insets.of(1, 2, 3, 4), insets.getInsets(NAVIGATION_BARS));
        assertEquals(Insets.of(1, 2, 3, 4), insets.getInsets(SYSTEM_BARS));
        assertEquals(Insets.of(1, 2, 3, 4), insets.getInsets(ALL_INSETS));
    }

    @Test
    public void spyWindowInsetsBuilder_statusBars() {
        WindowInsetsCompat insets =
                new SpyWindowInsetsBuilder().setInsets(STATUS_BARS, Insets.of(2, 3, 4, 5)).build();

        assertEquals(Insets.of(2, 3, 4, 5), insets.getInsets(STATUS_BARS));
        assertEquals(Insets.of(2, 3, 4, 5), insets.getInsets(SYSTEM_BARS));
        assertEquals(Insets.of(2, 3, 4, 5), insets.getInsets(ALL_INSETS));

        assertEquals(Insets.NONE, insets.getInsets(NAVIGATION_BARS));
    }

    @Test
    public void spyWindowInsetsBuilder_captionBar() {
        WindowInsetsCompat insets =
                new SpyWindowInsetsBuilder().setInsets(CAPTION_BAR, Insets.of(3, 4, 5, 6)).build();

        assertEquals(Insets.of(3, 4, 5, 6), insets.getInsets(CAPTION_BAR));
        assertEquals(Insets.of(3, 4, 5, 6), insets.getInsets(SYSTEM_BARS));
        assertEquals(Insets.of(3, 4, 5, 6), insets.getInsets(ALL_INSETS));

        assertEquals(Insets.NONE, insets.getInsets(NAVIGATION_BARS));
    }

    @Test
    public void spyWindowInsetsBuilder_displayCutout() {
        WindowInsetsCompat insets =
                new SpyWindowInsetsBuilder()
                        .setInsets(DISPLAY_CUTOUT, Insets.of(4, 5, 6, 7))
                        .build();

        assertEquals(Insets.of(4, 5, 6, 7), insets.getInsets(DISPLAY_CUTOUT));
        assertEquals(Insets.NONE, insets.getInsets(SYSTEM_BARS));
        assertEquals(Insets.of(4, 5, 6, 7), insets.getInsets(ALL_INSETS));

        assertEquals(Insets.NONE, insets.getInsets(NAVIGATION_BARS));
    }

    @Test
    public void spyWindowInsetsBuilder_systemBars() {
        WindowInsetsCompat insets =
                new SpyWindowInsetsBuilder()
                        .setInsets(STATUS_BARS, Insets.of(0, 5, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 10))
                        .build();

        assertEquals(Insets.of(0, 5, 0, 0), insets.getInsets(STATUS_BARS));
        assertEquals(Insets.of(0, 0, 0, 10), insets.getInsets(NAVIGATION_BARS));
        assertEquals(Insets.of(0, 5, 0, 10), insets.getInsets(SYSTEM_BARS));
        assertEquals(Insets.of(0, 5, 0, 10), insets.getInsets(ALL_INSETS));
    }

    @Test
    public void spyWindowInsetsBuilder_systemBarsAndDisplayCutout() {
        WindowInsetsCompat insets =
                new SpyWindowInsetsBuilder()
                        .setInsets(STATUS_BARS, Insets.of(0, 5, 0, 0))
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 10))
                        .setInsets(DISPLAY_CUTOUT, Insets.of(2, 0, 0, 0))
                        .build();

        assertEquals(Insets.of(0, 5, 0, 0), insets.getInsets(STATUS_BARS));
        assertEquals(Insets.of(0, 0, 0, 10), insets.getInsets(NAVIGATION_BARS));
        assertEquals(Insets.of(2, 0, 0, 0), insets.getInsets(DISPLAY_CUTOUT));
        assertEquals(Insets.of(0, 5, 0, 10), insets.getInsets(SYSTEM_BARS));
        assertEquals(Insets.of(2, 5, 0, 10), insets.getInsets(ALL_INSETS));
    }

    @Test
    public void spyWindowInsetsBuilder_ImeWithBottomNavBar() {
        WindowInsetsCompat insets =
                new SpyWindowInsetsBuilder()
                        .setInsets(NAVIGATION_BARS, Insets.of(0, 0, 0, 10))
                        .setInsets(IME, Insets.of(0, 0, 0, 22))
                        .build();

        assertEquals(Insets.of(0, 0, 0, 10), insets.getInsets(NAVIGATION_BARS));
        assertEquals(Insets.of(0, 0, 0, 22), insets.getInsets(IME));
        assertEquals(Insets.of(0, 0, 0, 10), insets.getInsets(SYSTEM_BARS));
        assertEquals(Insets.of(0, 0, 0, 22), insets.getInsets(ALL_INSETS));
    }

    @Test
    public void spyWindowInsetsBuilder_ImeWithLeftNavBar() {
        WindowInsetsCompat insets =
                new SpyWindowInsetsBuilder()
                        .setInsets(NAVIGATION_BARS, Insets.of(10, 0, 0, 0))
                        .setInsets(IME, Insets.of(0, 0, 0, 22))
                        .build();

        assertEquals(Insets.of(10, 0, 0, 0), insets.getInsets(NAVIGATION_BARS));
        assertEquals(Insets.of(0, 0, 0, 22), insets.getInsets(IME));
        assertEquals(Insets.of(10, 0, 0, 0), insets.getInsets(SYSTEM_BARS));
        assertEquals(Insets.of(10, 0, 0, 22), insets.getInsets(ALL_INSETS));
    }
}
