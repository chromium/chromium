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
     * Tells the the code generator to convert an arg's default cpp param type to the type listed in
     * |value| before passing it on to the user defined implementaton of the native method. Must
     * have a template function instatiation defined with the signature (where in_type is default
     * cpp param type for this java param, out_type is |value|): out_type
     * jni_zero::ConvertType<out_type>(JniEnv*env, jni_zero::JavaRef<in_type> param)
     */
    public String value();
}
