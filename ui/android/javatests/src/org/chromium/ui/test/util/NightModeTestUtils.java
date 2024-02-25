// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import androidx.appcompat.app.AppCompatDelegate;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;

import java.util.Arrays;
import java.util.List;

/** Helper methods to be used in tests to specify night mode state. */
public class NightModeTestUtils {
    /** {@link ParameterProvider} used for parameterized test that provides the night mode state. */
    public static class NightModeParams implements ParameterProvider {
        private static List<ParameterSet> sNightModeParams =
                Arrays.asList(
                        new ParameterSet().value(false).name("NightModeDisabled"),
                        new ParameterSet().value(true).name("NightModeEnabled"));

        @Override
        public List<ParameterSet> getParameters() {
            return sNightModeParams;
        }
    }

    /**
     * Sets up the night mode state for {@link BlankUiTestActivity}.
     * @param nightModeEnabled Whether night mode should be enabled.
     */
    public static void setUpNightModeForBlankUiTestActivity(boolean nightModeEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppCompatDelegate.setDefaultNightMode(
                            nightModeEnabled
                                    ? AppCompatDelegate.MODE_NIGHT_YES
                                    : AppCompatDelegate.MODE_NIGHT_NO);
                });
    }

    /** Resets the night mode state for {@link BlankUiTestActivity}. */
    public static void tearDownNightModeForBlankUiTestActivity() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppCompatDelegate.setDefaultNightMode(
                            AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM);
                });
    }
}
