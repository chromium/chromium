// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import org.junit.rules.ExternalResource;

/**
 * JUnit rule to have FooJni.setInstanceForTesting() overrides reset to their pre-test state.
 *
 * <p>Use this when resetting is not directly done by test runners (as it is in Chrome).
 */
public class JniResetterRule extends ExternalResource {
    private JniTestInstancesSnapshot mSnapshot;

    @Override
    protected void before() {
        // Use a snapshot rather than clearing to allow for overrides set during @BeforeClass.
        mSnapshot = JniTestInstancesSnapshot.snapshotOverridesForTesting();
    }

    @Override
    protected void after() {
        JniTestInstancesSnapshot.restoreSnapshotForTesting(mSnapshot);
    }
}
