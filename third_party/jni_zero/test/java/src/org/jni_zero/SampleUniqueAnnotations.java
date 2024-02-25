// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

class SampleUniqueAnnotations {
    // Poorly spaced intentionally
    private void do_not_match();

    @VisibleForTesting
    @NativeMethods
    @Generated("Test")
    interface Natives {
        @NativeClassQualifiedName("FooAndroid::BarDelegate")
        void foo(long nativePtr, @JniType("std::string") String arg);

        int bar(int x, int y);
    }

    @CalledByNative
    static void useBar1(Bar1.Inner inner) {}

    @NativeClassQualifiedName("Foo::Bar")
    native void nativeCallWithQualifiedObject(long nativePtr);
}
