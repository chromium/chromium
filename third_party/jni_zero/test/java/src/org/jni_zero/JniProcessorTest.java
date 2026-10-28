// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import java.util.List;

@JNINamespace("jni_zero")
public class JniProcessorTest {

    public static class WithJni implements OuterClass {
        // Annotation Processor should fail if changed to just "Nested".
        @AccessedByNative public OuterClass.Nested mNestedField;

        // Annotation Processor should fail if changed to just "Nested".
        @CalledByNative
        public List<OuterClass.Nested> getNestedList() {
            return null;
        }

        // Annotation Processor should fail if changed to just "Nested".
        @CalledByNative
        public void setNestedArray(OuterClass.Nested[] array) {}

        @NativeMethods
        interface Natives {
            // Annotation Processor should fail if changed to just "Nested".
            OuterClass.Nested getNested();
        }
    }
}
