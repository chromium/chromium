// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import java.util.ArrayList;

/**
 * Static methods to track which instances have been overridden by FooJni.setInstanceForTesting().
 *
 * <p>Non-static part is to provide an object that can be reset by the static methods.
 */
public class JniTestInstanceHolder {

    /** References to all instances ever created. Never shrinks. */
    static ArrayList<JniTestInstanceHolder> sAllOverrides;

    public Object value;

    private JniTestInstanceHolder() {}

    public static synchronized JniTestInstanceHolder create() {
        // Lazy initialization to avoid a class initializer.
        if (sAllOverrides == null) {
            sAllOverrides = new ArrayList<>();
        }
        JniTestInstanceHolder ret = new JniTestInstanceHolder();
        sAllOverrides.add(ret);
        return ret;
    }
}
