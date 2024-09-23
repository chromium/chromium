// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import org.jni_zero.internal.Boolean;

import java.util.Map;

@SomeAnnotation("that contains class Foo ")
class SampleProxyEdgeCases<E extends Enum<E>> {
    enum Integer {}
    @interface ShouldBeIgnored {}
    static class OtherInnerClass {}

    public @interface ShouldBeIgnored {}

    @NativeMethods
    interface Natives {
        void foo__weirdly__escaped_name1();
        String[][] arrayTypes1(int[] a, Object[][] b);
        int[] arrayTypes2(int[] a, Throwable[][] b);
        void fooForTest();
        void fooForTests();
        void fooForTesting();
        Map<OtherInnerClass, OtherInnerClass[]>[] genericsWithNestedClassArray(
                Map<OtherInnerClass, OtherInnerClass[]>[] arg);

        // Tests passing a nested class from another class in the same package.
        int addStructB(SampleForTests caller, SampleForTests.InnerStructB b);

        // Tests a java.lang class.
        public boolean setStringBuilder(StringBuilder sb);

        // Tests name collisions with java.lang classes.
        public void setBool(Boolean b, Integer i);

        // Test IntDef
        public @SomeClass.EnumType int setStringBuilder(@MyThing int sb);
    }

    // Non-proxy natives in same file.
    native void nativeInstanceMethod(long nativeInstance);
    static native void nativeStaticMethod();
}
