// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine;

import android.os.Looper;
import android.util.AndroidRuntimeException;

class ThreadCheck {
    static void ensureOnUiThread() {
        if (Looper.getMainLooper() != Looper.myLooper()) {
            throw new AndroidRuntimeException("This method needs to be called on the main thread");
        }
    }
}
