// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import static org.mockito.Mockito.doReturn;

import android.graphics.Rect;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.mockito.Mockito;

/** Util class used to manipulate window insets. */
public class WindowInsetsTestUtils {

    /**
     * Insets builder that allows building insets that don't exists on compat versions.
     *
     * <p>Note this class does not work well when test scenario involved display cutout heavily.
     *
     * @deprecated Use WindowInsetCompat.Builder from sdk=30+ instead.
     */
    @Deprecated(forRemoval = true)
    public static class SpyWindowInsetsBuilder {
        private static final int STATUS_BARS = WindowInsetsCompat.Type.statusBars();
        private static final int NAVIGATION_BARS = WindowInsetsCompat.Type.navigationBars();
        private static final int CAPTION_BAR = WindowInsetsCompat.Type.captionBar();
        private static final int SYSTEM_BARS = WindowInsetsCompat.Type.systemBars();
        private static final int IME = WindowInsetsCompat.Type.ime();
        private static final int DISPLAY_CUTOUT = WindowInsetsCompat.Type.displayCutout();

        private final WindowInsetsCompat mSpyWindowInsets;

        // Temporary storing all insets as Rect since Insets are mutable.
        private final Rect mAllInsets = new Rect();
        private final Rect mSystemBarInsets = new Rect();
        private final Rect mKeyboardInsets = new Rect();

        public SpyWindowInsetsBuilder() {
            mSpyWindowInsets = Mockito.spy(new WindowInsetsCompat.Builder().build());
        }

        public SpyWindowInsetsBuilder setInsets(int typeMask, Insets insets) {
            doReturn(insets).when(mSpyWindowInsets).getInsets(typeMask);
            if (typeMask == IME) {
                // Keyboard could overlap with nav bar.
                mKeyboardInsets.set(0, 0, 0, insets.bottom);
            } else {
                insetRect(mAllInsets, insets);
            }

            if (typeMask == STATUS_BARS || typeMask == NAVIGATION_BARS || typeMask == CAPTION_BAR) {
                insetRect(mSystemBarInsets, insets);
            }

            return this;
        }

        public WindowInsetsCompat build() {
            mAllInsets.bottom = Math.max(mAllInsets.bottom, mKeyboardInsets.bottom);
            doReturn(Insets.of(mAllInsets))
                    .when(mSpyWindowInsets)
                    .getInsets(SYSTEM_BARS + DISPLAY_CUTOUT + IME);
            doReturn(Insets.of(mSystemBarInsets)).when(mSpyWindowInsets).getInsets(SYSTEM_BARS);
            return mSpyWindowInsets;
        }
    }

    /**
     * Compat version of {@code Rect.inset()} which is available on API 31+. Assuming |insetRect|
     * represents an Insets.
     *
     * @param insetRect Rect represent an {@link Insets}.
     * @param insets Insets as the input of Rect.inset()
     * @see Rect#inset(android.graphics.Insets)
     */
    public static void insetRect(Rect insetRect, Insets insets) {
        insetRect.left += insets.left;
        insetRect.top += insets.top;
        insetRect.right += insets.right;
        insetRect.bottom += insets.bottom;
    }
}
