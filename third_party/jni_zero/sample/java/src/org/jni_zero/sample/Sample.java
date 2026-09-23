// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero.sample;

import android.util.Log;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;
import org.jni_zero.internal.Nullable;

import java.util.Arrays;

@JNINamespace("jni_zero::sample")
public class Sample {
    private static String TAG = "jni_zero";
    private static boolean sDidStaticCallWork;
    private boolean mDidCallWork;
    public static int sLastReceivedValue;
    public static JniPtr<NativeObjectToken> sLastReceivedPtr;
    public static JniRawPtr<NativeObjectToken> sLastReceivedRawPtr;
    public static JniUniquePtr<NativeObjectToken> sLastReceivedUniquePtr;

    @JniType("::jni_zero::sample::NativeObject")
    public interface NativeObjectToken extends JniTypeToken {}

    @CalledByNative
    static void acceptSafePtrFromCpp(JniPtr<NativeObjectToken> ptr) {
        sLastReceivedValue = SampleJni.get().getValue(ptr);
        sLastReceivedPtr = ptr;
    }

    @CalledByNative
    static void acceptRawPtrFromCpp(JniRawPtr<NativeObjectToken> ptr) {
        sLastReceivedRawPtr = ptr;
    }

    @CalledByNative
    static void acceptUniquePtrFromCpp(JniUniquePtr<NativeObjectToken> ptr) {
        sLastReceivedUniquePtr = ptr;
    }

    @CalledByNative
    static @Nullable JniPtr<NativeObjectToken> getHeldPtrForCpp() {
        return sLastReceivedUniquePtr;
    }

    public static JniUniquePtr<NativeObjectToken> createNativeObject(int value) {
        return SampleJni.get().createNativeObject(value);
    }

    public static @Nullable JniUniquePtr<NativeObjectToken> createNullNativeObject() {
        return SampleJni.get().createNullNativeObject();
    }

    public static JniRawPtr<NativeObjectToken> borrowNativeObject(int value) {
        return SampleJni.get().borrowNativeObject(value);
    }

    public static boolean isNullPtr(@Nullable JniPtr<NativeObjectToken> ptr) {
        return SampleJni.get().isNullPtr(ptr);
    }

    public static int readHeldPtrFromCpp() {
        return SampleJni.get().readHeldPtrFromCpp();
    }

    public static int getValue(JniPtr<NativeObjectToken> ptr) {
        return SampleJni.get().getValue(ptr);
    }

    public static int getDeletedCount() {
        return SampleJni.get().getDeletedCount();
    }

    public static void doSingleBasicCall() {
        Log.i(TAG, "Basic call");
        SampleJni.get().doSomething();
    }

    public static void doParameterCalls() {
        Log.i(TAG, "Parameter call");
        if (!SampleJni.get().testMultipleParams(1, 2, "3", new Sample())) {
            throw new RuntimeException("Call should have returned true");
        }
    }

    public static void doTwoWayCalls() {
        Log.i(TAG, "Two wall calls");
        sDidStaticCallWork = false;
        SampleJni.get().callBackIntoJava();
        if (!sDidStaticCallWork) {
            throw new RuntimeException("Static call did not set flag");
        }
        Sample s = SampleJni.get().callBackIntoInstance(new Sample());
        if (!s.callbackWorked()) {
            throw new RuntimeException("Instance call did not set flag");
        }
    }

    public static void triggerCallbackWithSafePtr(int value) {
        SampleJni.get().triggerCallbackWithSafePtr(value);
    }

    @CalledByNative
    private void callback() {
        Log.i(TAG, "Instance callback worked!");
        mDidCallWork = true;
    }

    private boolean callbackWorked() {
        return mDidCallWork;
    }

    @CalledByNative
    private static void staticCallback() {
        Log.i(TAG, "Static callback worked!");
        sDidStaticCallWork = true;
    }

    @CalledByNative
    private static @JniType("std::vector<MyEnum>") int[] getArrayOfEnum() {
        return new int[] {1, 2, 3};
    }

    @CalledByNative
    private static void setArrayOfEnum(@JniType("std::vector<MyEnum>") int[] values) {
        if (!Arrays.equals(values, new int[] {1, 2, 3})) {
            throw new RuntimeException("got: " + Arrays.toString(values));
        }
        Log.i(TAG, "Array of enum worked.");
    }

    @NativeMethods
    interface Natives {
        void doSomething();

        boolean testMultipleParams(int a, int b, String c, Sample d);

        void callBackIntoJava();

        Sample callBackIntoInstance(Sample sample);

        void triggerCallbackWithSafePtr(int value);

        JniUniquePtr<NativeObjectToken> createNativeObject(int value);

        @Nullable
        JniUniquePtr<NativeObjectToken> createNullNativeObject();

        JniRawPtr<NativeObjectToken> borrowNativeObject(int value);

        boolean isNullPtr(@Nullable JniPtr<NativeObjectToken> ptr);

        int readHeldPtrFromCpp();

        int getValue(JniPtr<NativeObjectToken> self);

        int getDeletedCount();
    }
}
