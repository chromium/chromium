// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import org.jni_zero.internal.JniOverrideHolder;

/** Test runner APIs for batch resetting of setInstanceForTesting() overrides. */
public class JniTestOverrides {
    private final Object[] mValues;

    private JniTestOverrides(Object[] values) {
        this.mValues = values;
    }

    /** Return a snapshot of all current JNI setInstanceForTesting overrides. */
    public static JniTestOverrides snapshotOverridesForTesting() {
        return new JniTestOverrides(JniOverrideHolder.createSnapshot());
    }

    /** Restores all JNI setInstanceForTesting overrides to the given snapshot. */
    public static void restoreSnapshotForTesting(JniTestOverrides overrides) {
        JniOverrideHolder.restoreSnapshot(overrides.mValues);
    }

    /** Clears all JNI setInstanceForTesting overrides. */
    public static void clearAllForTesting() {
        JniOverrideHolder.restoreSnapshot(new Object[0]);
    }
}
