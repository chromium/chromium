// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import java.util.Collections;

/** Used by jni_zero.cc. */
@JNINamespace("jni_zero")
public class JniInit {
    @CalledByNative
    private static Object[] init() {
        // For JVM (works fine on ART), cannot call from Java -> Native during InitVM because the
        // System.loadLibrary() call has not yet completed. Could work around this by using
        // RegisterNatives(), but simpler to return an array than to make Java->Native work.
        return new Object[] {Collections.EMPTY_LIST, Collections.EMPTY_MAP};
    }

    @CalledByNative
    private static void crashIfMultiplexingMisaligned(long wholeHash, long priorityHash) {
        try {
            long javaHash = Class.forName("J.N").getField("MUXING_HASH").getLong(null);
            // We compare what we have in our Java to what is all in native's JNI, or the "priority"
            // elements (to cover the case of Webview) in native.
            if (javaHash != wholeHash && javaHash != priorityHash) {
                throw new RuntimeException(
                        "JNI Zero multiplexing hashes do not align. Native: "
                                + Long.toString(wholeHash)
                                + " or "
                                + Long.toString(priorityHash)
                                + " Java: "
                                + Long.toString(javaHash));
            }
        } catch (ReflectiveOperationException e) {
            // This check is just a backup. If we fail to actually do the check, we assert so that
            // we get some notice on debug/assert enabled builds, but don't want to crash everyone
            // since it's likely fine.
            assert false : "JNI multiplexing hash lookup failed with " + e.getMessage();
        }
    }
}
