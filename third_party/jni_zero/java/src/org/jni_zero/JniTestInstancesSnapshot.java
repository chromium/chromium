// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import java.util.ArrayList;

/** Test runner APIs for batch resetting of setInstanceForTesting() overrides. */
public class JniTestInstancesSnapshot {
    private final Object[] mValues;

    private JniTestInstancesSnapshot(Object[] values) {
        this.mValues = values;
    }

    /** Return a snapshot of all current JNI setInstanceForTesting overrides. */
    public static JniTestInstancesSnapshot snapshotOverridesForTesting() {
        synchronized (JniTestInstanceHolder.class) {
            ArrayList<JniTestInstanceHolder> holders = JniTestInstanceHolder.sAllOverrides;
            if (holders == null) {
                return new JniTestInstancesSnapshot(new Object[0]);
            }
            Object[] overrideValues = new Object[holders.size()];
            int size = holders.size();
            for (int i = 0; i < size; ++i) {
                overrideValues[i] = holders.get(i).value;
            }

            return new JniTestInstancesSnapshot(overrideValues);
        }
    }

    /** Restores all JNI setInstanceForTesting overrides to the given snapshot. */
    public static void restoreSnapshotForTesting(JniTestInstancesSnapshot overrides) {
        synchronized (JniTestInstanceHolder.class) {
            Object[] snapshotValues = overrides.mValues;
            ArrayList<JniTestInstanceHolder> holders = JniTestInstanceHolder.sAllOverrides;
            int snapshotSize = snapshotValues.length;
            if (holders == null) {
                assert snapshotSize == 0;
                return;
            }
            int holdersSize = holders.size();
            assert holdersSize >= snapshotSize;
            for (int i = 0; i < snapshotSize; ++i) {
                holders.get(i).value = snapshotValues[i];
            }
            for (int i = snapshotSize; i < holdersSize; ++i) {
                holders.get(i).value = null;
            }
        }
    }

    /** Clears all JNI setInstanceForTesting overrides. */
    public static void clearAllForTesting() {
        restoreSnapshotForTesting(new JniTestInstancesSnapshot(new Object[0]));
    }
}
