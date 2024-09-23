// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

// To regenerate .class file:
//   javac java/src/org/jni_zero/JavapClass.java
public class JavapClass<T> {
    public static final int CONST_INT = 3;
    public static boolean sBoolValue = true;
    public static final String CONST_STR = "VaLuE";

    public final int ignoredField = 7;

    public JavapClass() {}
    public JavapClass(int a) {}
    public JavapClass(boolean b) {}

    private void ignore(int thing) {}
    int intMethod(String value) {
        return 0;
    }
    static int[][] staticIntMethod(String arg) {
        return null;
    }
    static int staticIntMethod(String arg1, JavapClass arg2) {
        return 0;
    }
    static void needsMangling(int a) {}
    static void needsMangling(String s) {}
    static void needsMangling(java.util.ArrayList<String> x){}

    <T2 extends Runnable> Class objTest(T thing, T2[] other) {
        return null;
    }
}
