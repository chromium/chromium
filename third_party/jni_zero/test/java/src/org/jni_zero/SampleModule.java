// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;


class SampleModule {
    void test() {
        if (SampleForAnnotationProcessorJni.get().bar(true)) {
            SampleForAnnotationProcessorJni.get().foo();
        }
    }

    @NativeMethods("module")
    interface Natives {
        void foo();

        boolean bar(boolean a);
    }
}
