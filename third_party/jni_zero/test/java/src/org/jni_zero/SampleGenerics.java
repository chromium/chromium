// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import java.util.List;

@JNINamespace("jni_zero")
class SampleGenerics<T extends List<String>> {
    public T field;

    @CalledByNative
    SampleGenerics(int noMethodGenerics) {}

    @CalledByNative
    <T2> SampleGenerics(T2 mustMergeGenerics) {}

    @CalledByNative
    T someMethod(T someArg) {
        return someArg;
    }

    @NativeMethods
    interface Natives {
        SampleGenerics<List<String>> someMethod(
                SampleGenerics<List<String>> arg, List<String> arg2);

        void methodWithGenericParam(List<String> arg);
    }
}
