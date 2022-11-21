// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.InstrumentationRegistry;

import org.junit.runners.model.InitializationError;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.SkipCheck;
import org.chromium.ui.test.util.UiDisableIfSkipCheck;

import java.util.List;

/**
 * A custom runner for //weblayer JUnit4 tests.
 */
public class WebLayerJUnit4ClassRunner extends BaseJUnit4ClassRunner {
    /**
     * Create a WebLayerJUnit4ClassRunner to run {@code klass} and initialize values
     *
     * @throws InitializationError if the test class malformed
     */
    public WebLayerJUnit4ClassRunner(final Class<?> klass) throws InitializationError {
        super(klass);
    }

    @Override
    protected List<SkipCheck> getSkipChecks() {
        return addToList(super.getSkipChecks(), new MinWebLayerVersionSkipCheck(),
                new UiDisableIfSkipCheck(InstrumentationRegistry.getContext()));
    }
}
