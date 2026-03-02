// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import static android.util.Log.ASSERT;

import android.util.Log;

/** Test for NoAndroidLog checker. */
public class NoAndroidLogNoCompileSources {
    public void testAll() {
        Log.d("TAG", "msg");
        android.util.Log.e("TAG", "msg");
        int i = Log.VERBOSE;
        int j = android.util.Log.INFO;
        int k = ASSERT;
        Class c = Log.class;
    }
}
