package org.jni_zero.sample;

import android.util.Log;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import java.util.Arrays;

@JNINamespace("jni_zero::sample")
public class Sample {
    private static String TAG = "jni_zero";
    private static boolean sDidStaticCallWork;
    private boolean mDidCallWork;

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
    }
}
