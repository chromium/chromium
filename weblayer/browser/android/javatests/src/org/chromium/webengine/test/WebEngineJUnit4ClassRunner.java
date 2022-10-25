// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import org.junit.runners.model.InitializationError;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.SkipCheck;

import java.util.List;

/**
 * A custom runner for //weblayer JUnit4 tests.
 */
public class WebEngineJUnit4ClassRunner extends BaseJUnit4ClassRunner {
    /**
     * Create a WebEngineJUnit4ClassRunner to run {@code klass} and initialize values.
     *
     * @throws InitializationError if the test class malformed
     */
    public WebEngineJUnit4ClassRunner(final Class<?> klass) throws InitializationError {
        super(klass);
    }

    @Override
    protected List<SkipCheck> getSkipChecks() {
        // TODO(rayankans): Add SkipChecks.
        return super.getSkipChecks();
    }
}
