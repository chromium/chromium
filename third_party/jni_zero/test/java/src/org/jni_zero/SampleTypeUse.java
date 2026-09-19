// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

public class SampleTypeUse {
    @CalledByNative
    public static void foo(JniPtr<SampleTypeDefine> ptr) {}

    @CalledByNative
    public static void bar(JniRawPtr<SampleTypeDefine> ptr) {}

    @CalledByNative
    public static void baz(JniPtr<SampleTypeDefine.Nested> ptr) {}

    @CalledByNative
    public static void qux(JniUniquePtr<SampleTypeDefine> ptr) {}

    @CalledByNative
    public static JniPtr<SampleTypeDefine> lendIt() {
        return null;
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<SampleTypeDefine> makeOwned();

        JniRawPtr<SampleTypeDefine> makeRaw();

        void useIt(JniPtr<SampleTypeDefine> ptr);

        // Parameter name "self" triggers C++ member-function dispatch.
        void doThing(JniPtr<SampleTypeDefine> self);
    }
}
