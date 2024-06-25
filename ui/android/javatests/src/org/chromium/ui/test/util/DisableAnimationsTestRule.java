// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Use @TestAnimations.EnableAnimations instead. */
public class DisableAnimationsTestRule implements TestRule {
    /** Allows methods to ensure animations are on while disabled rule is applied class-wide. */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface EnsureAnimationsOn {}

    @Override
    public Statement apply(final Statement statement, Description description) {
        return statement;
    }
}
