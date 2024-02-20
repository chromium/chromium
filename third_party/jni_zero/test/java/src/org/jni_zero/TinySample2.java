// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import org.jni_zero.NativeMethods;

class TinySample2 {
    @NativeMethods()
    interface Natives {
        void test();
    }
}
