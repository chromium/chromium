// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero.internal;

import java.util.ArrayList;

/**
 * Static methods to track which instances have been overridden by FooJni.setInstanceForTesting().
 *
 * <p>Non-static part is to provide an object that can be reset by the static methods.
 */
public class JniOverrideHolder {

    public Object value;

    private JniOverrideHolder() {}

    public static ArrayList<JniOverrideHolder> sOverrides;

    public static synchronized JniOverrideHolder create() {
        // Lazy initialization to avoid a class initializer.
        if (sOverrides == null) {
            sOverrides = new ArrayList<>();
        }
        JniOverrideHolder ret = new JniOverrideHolder();
        sOverrides.add(ret);
        return ret;
    }

    public static synchronized Object[] createSnapshot() {
        ArrayList<JniOverrideHolder> holders = sOverrides;
        if (holders == null) {
            return new Object[0];
        }
        Object[] ret = new Object[holders.size()];
        int size = holders.size();
        for (int i = 0; i < size; ++i) {
            ret[i] = holders.get(i).value;
        }
        return ret;
    }

    public static synchronized void restoreSnapshot(Object[] snapshot) {
        ArrayList<JniOverrideHolder> holders = sOverrides;
        int snapshotSize = snapshot.length;
        if (holders == null) {
            assert snapshotSize == 0;
            return;
        }
        int holdersSize = holders.size();
        assert holdersSize >= snapshotSize;
        for (int i = 0; i < snapshotSize; ++i) {
            holders.get(i).value = snapshot[i];
        }
        for (int i = snapshotSize; i < holdersSize; ++i) {
            holders.get(i).value = null;
        }
    }
}
