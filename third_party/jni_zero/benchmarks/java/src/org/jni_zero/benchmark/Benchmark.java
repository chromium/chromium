// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.jni_zero.benchmark;

import android.os.Environment;

import org.jni_zero.AccessedByNative;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@JNINamespace("jni_zero::benchmark")
public class Benchmark {
    private static final String TAG = "JNI_PERF";
    private static final long MS_TO_NANO = 1000000;
    private static final long US_TO_NANO = 1000;

    // So it is not optimized away.
    @AccessedByNative private static long count = 0;

    public static void runBenchmark() {
        Log.i(TAG, "===============Start of Benchmarks===============");
        // Warmup the native lib.
        BenchmarkJni.get().callMe();
        StringBuilder sb = new StringBuilder();
        sb.append("# Trivial Calls Benchmark\n");
        sb.append(runJavaToNativeCallBenchmark()).append('\n');
        sb.append(BenchmarkJni.get().runLookupBenchmark()).append('\n');
        sb.append("# Integer boxing/unboxing Benchmark\n");
        sb.append(BenchmarkJni.get().runIntegerBoxingBenchmark()).append('\n');
        sb.append("# Parameter Sizes Benchmark\n");
        sb.append(runJavaToNativeParamSizesBenchmark()).append('\n');
        sb.append(BenchmarkJni.get().runNativeToJavaParamSizesBenchmark()).append('\n');
        sb.append("# Multiple Params Benchmark\n");
        sb.append(runJavaToNativeMultipleParamsBenchmark()).append('\n');
        sb.append(BenchmarkJni.get().runNativeToJavaMultipleParamsBenchmark()).append('\n');
        sb.append("# AttachCurrentThread Benchmark\n");
        sb.append(BenchmarkJni.get().runAttachCurrentThreadBenchmark()).append('\n');
        sb.append("# List Iteration Benchmark\n");
        sb.append(runListIterationBenchmark()).append('\n');
        sb.append("# Strings Benchmark\n");
        sb.append(runJavaToNativeStringBenchmark()).append('\n');
        sb.append(BenchmarkJni.get().runNativeToJavaStringsBenchmark()).append('\n');

        File basedir =
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        File outputFile = new File(basedir, "benchmark_log.txt");
        try (FileOutputStream fos = new FileOutputStream(outputFile)) {
            fos.write(sb.toString().getBytes(StandardCharsets.UTF_8));
            Log.i(TAG, "Benchmark results written to " + outputFile.getCanonicalPath());
        } catch (Exception e) {
            Log.e(TAG, "Failed to write benchmark log to disk:", e);
        }
        Log.i(TAG, "===============End of Benchmarks===============");
    }

    @CalledByNative
    static void callMe() {}

    @CalledByNative
    static void receiveU8String(@JniType("std::string") String s) {}

    @CalledByNative
    static void receiveU16String(@JniType("std::u16string") String s) {}

    @CalledByNative
    static void receiveLargeIntArray(@JniType("std::vector<int32_t>") int[] array) {
        for (int i = 0; i < array.length; i++) {
            count += array[i];
        }
    }

    @CalledByNative
    static void receiveSingleInt(int param) {
        count += param;
    }

    @CalledByNative
    static void receiveSingleInteger(@JniType("int32_t") Integer param) {
        count += param;
    }

    @CalledByNative
    static void receive10Ints(
            int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
        count += a + b + c + d + e + f + g + h + i + j;
    }

    @CalledByNative
    static void receive10IntegersConverted(
            @JniType("int") Integer a,
            @JniType("int") Integer b,
            @JniType("int") Integer c,
            @JniType("int") Integer d,
            @JniType("int") Integer e,
            @JniType("int") Integer f,
            @JniType("int") Integer g,
            @JniType("int") Integer h,
            @JniType("int") Integer i,
            @JniType("int") Integer j) {
        count += a + b + c + d + e + f + g + h + i + j;
    }

    static String runJavaToNativeCallBenchmark() {
        StringBuilder sb = new StringBuilder();
        int NUM_TRIES = 10000;
        long startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int i = 0; i < NUM_TRIES; i++) {
            BenchmarkJni.get().callMe();
        }
        long elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Calling native method [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        double averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average per call = " + averageUs * US_TO_NANO + " ns\n");
        return sb.toString();
    }

    static String runListIterationBenchmark() {
        StringBuilder sb = new StringBuilder();
        int NUM_TRIES = 10000;
        int ARRAY_SIZE = 1000;
        List<String> list = new ArrayList(ARRAY_SIZE);
        for (int i = 0; i < ARRAY_SIZE; i++) {
            list.add("a");
        }
        long startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int i = 0; i < NUM_TRIES; i++) {
            BenchmarkJni.get().sendListObject(list);
        }
        long elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Iterating over ("
                        + ARRAY_SIZE
                        + ") Java pointers list no conversion ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        double averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append(
                "Average per " + ARRAY_SIZE + " sized list = " + averageUs * US_TO_NANO + " ns\n");

        elapsedTimeUs = 0;
        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int i = 0; i < NUM_TRIES; i++) {
            BenchmarkJni.get().sendListConverted(list);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Iterating over ("
                        + ARRAY_SIZE
                        + ") Java pointers list converted to vector with @JniType ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append(
                "Average per " + ARRAY_SIZE + " sized list = " + averageUs * US_TO_NANO + " ns\n");
        return sb.toString();
    }

    public static String runJavaToNativeMultipleParamsBenchmark() {
        StringBuilder sb = new StringBuilder();
        int NUM_TRIES = 10000;
        long startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int i = 0; i < NUM_TRIES; i++) {
            BenchmarkJni.get().send10Ints(i, i, i, i, i, i, i, i, i, i);
        }
        long elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending 10 ints [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        double averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average 10 int call = " + averageUs * US_TO_NANO + " ns\n");

        Integer a = 1;
        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int k = 0; k < NUM_TRIES; k++) {
            BenchmarkJni.get().send10Integers(a, a, a, a, a, a, a, a, a, a);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending 10 Integers [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average 10 Integer call = " + averageUs * US_TO_NANO + " ns\n");
        return sb.toString();
    }

    public static String runJavaToNativeParamSizesBenchmark() {
        StringBuilder sb = new StringBuilder();
        int ARRAY_SIZE = 10000;
        int[] intArray = new int[ARRAY_SIZE];
        int NUM_TRIES = 1000;
        for (int i = 0; i < intArray.length; i++) {
            intArray[i] = i;
        }
        Integer[] integerArray = new Integer[ARRAY_SIZE];
        for (int i = 0; i < ARRAY_SIZE; i++) {
            integerArray[i] = i;
        }
        List<Integer> integerList = Arrays.asList(integerArray);
        byte[] byteArray = new byte[ARRAY_SIZE];
        for (int i = 0; i < ARRAY_SIZE; i++) {
            byteArray[i] = (byte) (i % 128);
        }

        long startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            BenchmarkJni.get().sendLargeIntArray(intArray);
        }
        long elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + ARRAY_SIZE
                        + " int array (direct access no conversion) [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        double averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append(
                "Average per " + ARRAY_SIZE + " array call = " + averageUs * US_TO_NANO + " ns\n");

        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            BenchmarkJni.get().sendLargeIntArrayConverted(intArray);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + ARRAY_SIZE
                        + " int array converted to vector (@JniType) [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append(
                "Average per " + ARRAY_SIZE + " array call = " + averageUs * US_TO_NANO + " ns\n");

        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            BenchmarkJni.get().sendLargeObjectArray(integerArray);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + ARRAY_SIZE
                        + " Integer array (no conversion) [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append(
                "Average per " + ARRAY_SIZE + " array call = " + averageUs * US_TO_NANO + " ns\n");

        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            BenchmarkJni.get().sendLargeObjectList(integerList);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + ARRAY_SIZE
                        + " Integer list (no conversion) [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average per " + ARRAY_SIZE + " list call = " + averageUs * US_TO_NANO + " ns\n");

        elapsedTimeUs = 0;
        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            int[] streamedIntArray =
                    integerList.stream().mapToInt((integer) -> integer.intValue()).toArray();
            BenchmarkJni.get().sendLargeIntArrayConverted(streamedIntArray);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + ARRAY_SIZE
                        + " Integer list as int array (java stream conversion + primitive vector"
                        + " conversion) [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average per " + ARRAY_SIZE + " list call = " + averageUs * US_TO_NANO + " ns\n");

        elapsedTimeUs = 0;
        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            for (int i = 0; i < ARRAY_SIZE; i++) {
                BenchmarkJni.get().sendSingleInt(i);
            }
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + ARRAY_SIZE
                        + " ints one at a time [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average per " + ARRAY_SIZE + " ints = " + averageUs * US_TO_NANO + " ns\n");

        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            for (int i = 0; i < ARRAY_SIZE; i++) {
                BenchmarkJni.get().sendSingleInteger(i);
            }
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + ARRAY_SIZE
                        + " Integers one at a time [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average per " + ARRAY_SIZE + " Integers = " + averageUs * US_TO_NANO + " ns\n");

        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            BenchmarkJni.get().sendByteArrayUseView(byteArray);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + ARRAY_SIZE
                        + " Bytes and read using ByteArrayView [Java -> Native] ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append(
                "Average per " + ARRAY_SIZE + " byte[] call = " + averageUs * US_TO_NANO + " ns\n");
        return sb.toString();
    }

    public static String runJavaToNativeStringBenchmark() {
        StringBuilder sb = new StringBuilder();
        int NUM_TRIES = 10000;
        int STRING_SIZE = 1000;
        StringBuilder asciiBuilder = new StringBuilder();
        for (int i = 0; i < STRING_SIZE; i++) {
            asciiBuilder.append('a');
        }
        String asciiString = asciiBuilder.toString();
        StringBuilder nonAsciiBuilder = new StringBuilder();
        for (int i = 0; i < STRING_SIZE; i++) {
            nonAsciiBuilder.append('ق');
        }
        String nonAsciiString = nonAsciiBuilder.toString();
        long startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            BenchmarkJni.get().sendAsciiStringConvertedToU8(asciiString);
        }
        long elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + STRING_SIZE
                        + " chars ASCII string [Java -> Native] with conversion to string ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        double averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average per " + STRING_SIZE + " string = " + averageUs * US_TO_NANO + " ns\n");

        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            BenchmarkJni.get().sendAsciiStringConvertedToU16(asciiString);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + STRING_SIZE
                        + " chars ASCII string [Java -> Native] with conversion to u16string ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average per " + STRING_SIZE + " string = " + averageUs * US_TO_NANO + " ns\n");

        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            BenchmarkJni.get().sendNonAsciiStringConvertedToU8(nonAsciiString);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + STRING_SIZE
                        + " chars non-ASCII string [Java -> Native] with conversion to string ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average per " + STRING_SIZE + " string = " + averageUs * US_TO_NANO + " ns\n");

        startTimeUs = System.nanoTime() / US_TO_NANO;
        for (int tries = 0; tries < NUM_TRIES; tries++) {
            BenchmarkJni.get().sendNonAsciiStringConvertedToU16(nonAsciiString);
        }
        elapsedTimeUs = System.nanoTime() / US_TO_NANO - startTimeUs;
        sb.append(
                "Sending "
                        + STRING_SIZE
                        + " chars non-ASCII string [Java -> Native] with conversion to u16string ("
                        + NUM_TRIES
                        + " times). Elapsed Time = "
                        + elapsedTimeUs
                        + " us\n");
        averageUs = (elapsedTimeUs * 1.0) / NUM_TRIES;
        sb.append("Average per " + STRING_SIZE + " string = " + averageUs * US_TO_NANO + " ns\n");
        return sb.toString();
    }

    @NativeMethods
    interface Natives {
        @JniType("std::string")
        String runLookupBenchmark();

        @JniType("std::string")
        String runGeneratedClassesBenchmark();

        @JniType("std::string")
        String runNativeToJavaParamSizesBenchmark();

        @JniType("std::string")
        String runIntegerBoxingBenchmark();

        @JniType("std::string")
        String runAttachCurrentThreadBenchmark();

        @JniType("std::string")
        String runNativeToJavaMultipleParamsBenchmark();

        @JniType("std::string")
        String runNativeToJavaStringsBenchmark();

        void sendLargeIntArray(int[] array);

        void sendLargeIntArrayConverted(@JniType("std::vector<int32_t>") int[] array);

        void sendByteArrayUseView(@JniType("jni_zero::ByteArrayView") byte[] bytearray);

        // void sendByteArrayConverted(@JniType("std::vector<int8_t>") byte[] bytearray);
        void sendLargeObjectArray(Integer[] array);

        void sendLargeObjectList(List<Integer> array);

        void send10Ints(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j);

        void send10Integers(
                Integer a,
                Integer b,
                Integer c,
                Integer d,
                Integer e,
                Integer f,
                Integer g,
                Integer h,
                Integer i,
                Integer j);

        void sendSingleInt(int i);

        void sendSingleInteger(Integer i);

        void sendAsciiStringConvertedToU8(@JniType("std::string") String s);

        void sendAsciiStringConvertedToU16(@JniType("std::u16string") String s);

        void sendNonAsciiStringConvertedToU8(@JniType("std::string") String s);

        void sendNonAsciiStringConvertedToU16(@JniType("std::u16string") String s);

        void sendListObject(List l);

        void sendListConverted(@JniType("std::vector") List l);

        void callMe();
    }
}
