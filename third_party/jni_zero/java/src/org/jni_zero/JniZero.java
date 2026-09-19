// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import org.jni_zero.internal.Nullable;

import java.util.Collections;

/** Core APIs. */
@JNINamespace("jni_zero")
public class JniZero {
    private static ClassLoader sPendingJniClassLoader;
    private static boolean sInitialized;
    private static boolean sRawPtrHooksEnabled;
    private static @Nullable SafePointersTrackerFactory sTrackerFactory;

    static boolean isRawPtrHooksEnabled() {
        return sRawPtrHooksEnabled;
    }

    /** Seam for //base's LifetimeAssert to attach leak tracking without a //base dep. */
    public interface SafePointersTrackerFactory {
        SafePointersTracker create(Object target);
    }

    public interface SafePointersTracker {
        void destroy();
    }

    public static void setSafePointersTrackerFactory(SafePointersTrackerFactory factory) {
        sTrackerFactory = factory;
    }

    static @Nullable SafePointersTracker createSafePointersTracker(Object target) {
        return sTrackerFactory == null ? null : sTrackerFactory.create(target);
    }

    /** Sets the ClassLoader used to resolve classes by JNI Zero. */
    public static void setJniClassLoader(ClassLoader classLoader) {
        if (sInitialized) {
            JniZeroJni.get().setJniClassLoader(classLoader);
        } else {
            sPendingJniClassLoader = classLoader;
        }
    }

    @CalledByNative
    private static long getNativePtr(JniPtrInner<?> ptr) {
        return JniZeroInternal.getNativePtr(ptr);
    }

    @CalledByNative
    private static Object[] init(boolean rawPtrHooksEnabled) {
        sInitialized = true;
        sRawPtrHooksEnabled = rawPtrHooksEnabled;
        // For JVM (works fine on ART), cannot call from Java -> Native during InitVM because the
        // System.loadLibrary() call has not yet completed. Could work around this by using
        // RegisterNatives(), but simpler to return an array than to make Java->Native work.
        ClassLoader jniClassLoader = sPendingJniClassLoader;
        if (jniClassLoader != null) {
            sPendingJniClassLoader = null;
        } else {
            jniClassLoader = JniZero.class.getClassLoader();
        }
        return new Object[] {Collections.EMPTY_LIST, Collections.EMPTY_MAP, jniClassLoader};
    }

    @CalledByNative
    private static void crashIfMultiplexingMisaligned(long wholeHash, long priorityHash) {
        try {
            // Reflection is required because we cannot reference the J/N class at compile time -
            // it gets inserted at the very end of the build process as a srcjar_dep.
            long javaWholeHash = Class.forName("J.N").getField("WHOLE_HASH").getLong(null);
            long javaPriorityHash = Class.forName("J.N").getField("PRIORITY_HASH").getLong(null);
            // We only compare the "priority" to the "whole" - we need the entirety of at least one
            // to always be compared.
            if (javaWholeHash != wholeHash
                    && javaWholeHash != priorityHash
                    && javaPriorityHash != wholeHash) {
                throw new RuntimeException(
                        "JNI Zero multiplexing hashes do not align. Native: "
                                + wholeHash
                                + " or "
                                + priorityHash
                                + " Java: "
                                + javaWholeHash
                                + " or "
                                + javaPriorityHash);
            }
        } catch (ReflectiveOperationException e) {
            // This check is just a backup. If we fail to actually do the check, we assert so that
            // we get some notice on debug/assert enabled builds, but don't want to crash everyone
            // since it's likely fine.
            assert false : "JNI multiplexing hash lookup failed with " + e.getMessage();
        }
    }

    @NativeMethods
    interface Natives {
        void setJniClassLoader(ClassLoader classLoader);
    }
}
