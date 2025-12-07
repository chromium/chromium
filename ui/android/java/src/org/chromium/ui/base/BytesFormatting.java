// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Java mirror of ui/base/text/bytes_formatting.h. */
@JNINamespace("ui")
@NullMarked
public class BytesFormatting {

    /**
     * Simple call to return a speed as a string in human-readable format. Ex: FormatSpeed(512) =>
     * "512 B/s" Ex: FormatSpeed(101479) => "99.1 kB/s"
     */
    public static String formatSpeed(long bytes) {
        return BytesFormattingJni.get().formatSpeed(bytes);
    }

    @NativeMethods
    interface Natives {
        String formatSpeed(long bytes);
    }
}
