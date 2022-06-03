// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import androidx.annotation.CallSuper;
import androidx.annotation.StyleRes;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.BaseActivityTestRule;

/**
 * A wrapper around {@link BaseActivityTestRule} that sets the {@link DummyUiActivity} theme. This
 * test rule only allows setting one theme at a time. If you need to set more than one theme, at
 * this time, you need to create test-only theme in resources.
 * @param <T> Activity must subclass DummyUiActivity to allow setting the theme.
 * */
public class ThemedDummyUiActivityTestRule<T extends DummyUiActivity>
        extends BaseActivityTestRule<T> {
    private final @StyleRes int mThemeId;

    /**
     * @param activityClass The class of the activity the test rule will use.
     * @param themeId The theme to apply to the activity.
     */
    public ThemedDummyUiActivityTestRule(Class<T> activityClass, @StyleRes int themeId) {
        super(activityClass);
        mThemeId = themeId;
    }

    @Override
    @CallSuper
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                DummyUiActivity.setTestTheme(mThemeId);
                try {
                    base.evaluate();
                } finally {
                    DummyUiActivity.setTestTheme(0);
                }
            }
        }, desc);
    }
}
