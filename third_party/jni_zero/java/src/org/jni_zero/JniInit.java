// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/** Used by jni_zero.cc. */
@JNINamespace("jni_zero")
public class JniInit {
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
}
