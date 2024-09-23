// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.jni_zero;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.CLASS)
public @interface JniType {
    /**
     * Tells the the code generator to handle the conversion from the default c++ jni type (e.g.
     * jstring, jobject, jobjectArray, etc.) to more standard cpp types (e.g. std::string,
     * std::vector, etc.). Must have a template function instatiation defined with the signature
     * (where in_type is default cpp param type for this java param, cpp_type is |value| and
     * java_type is the default JNI spec c++ type):
     *
     * <pre>
     * cpp_type jni_zero::FromJniType<cpp_type>(JniEnv*, const JavaRef<java_type>&);
     *
     * OR
     *
     * ScopedJavaLocalRef<java_type> jni_zero::ToJniType<cpp_type>(JniEnv*, const cpp_type&);
     * </pre>
     */
    public String value();
}
