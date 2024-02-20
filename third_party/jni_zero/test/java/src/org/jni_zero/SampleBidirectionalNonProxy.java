// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import org.jni_zero.Boolean;

class SampleBidirectionalNonProxy {
    /** The pointer to the native Test. */
    public long nativeTest;
    private static native int nativeStaticMethod(long nativeTest, int arg1);
    private native int nativeMethod(long nativeTest, int arg1);
    @CalledByNative
    private void testMethodWithParam(int iParam) {}
    @CalledByNative
    private String testMethodWithParamAndReturn(int iParam) {
        return null;
    }
    @CalledByNative
    private static int testStaticMethodWithParam(int iParam) {
        return 0;
    }
    @CalledByNative
    private static double testMethodWithNoParam() {
        return 0;
    }
    @CalledByNative
    private static String testStaticMethodWithNoParam() {}

    // Tests passing a nested class from another class in the same package.
    @CalledByNative
    void addStructB(SampleForTests caller, SampleForTests.InnerStructB b) {}

    // Tests a java.lang class.
    @CalledByNative
    void setStringBuilder(StringBuilder sb) {}

    // Tests name collisions with java.lang classes.
    @CalledByNative
    void setBool(Boolean b, Integer i) {}

    class MyInnerClass {
        @NativeCall("MyInnerClass")
        private native int nativeInit();
    }
    class MyOtherInnerClass {
        @NativeCall("MyOtherInnerClass")
        private native int nativeInit();
    }
}
