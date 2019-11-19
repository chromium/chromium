// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.Looper;
import android.util.AndroidRuntimeException;

/* package */ class ThreadCheck {
    /* package */ static void ensureOnUiThread() {
        if (Looper.getMainLooper() != Looper.myLooper()) {
            throw new AndroidRuntimeException("This method needs to be called on the main thread");
        }
    }
}
