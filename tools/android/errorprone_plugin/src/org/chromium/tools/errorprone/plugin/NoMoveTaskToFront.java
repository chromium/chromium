// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import static com.google.errorprone.matchers.Matchers.instanceMethod;

import com.google.errorprone.BugPattern;
import com.google.errorprone.BugPattern.LinkType;
import com.google.errorprone.BugPattern.SeverityLevel;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.bugpatterns.BugChecker.MethodInvocationTreeMatcher;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.matchers.Matcher;
import com.google.errorprone.suppliers.Suppliers;
import com.sun.source.tree.ExpressionTree;
import com.sun.source.tree.MethodInvocationTree;

import org.chromium.build.annotations.ServiceImpl;

/**
 * Assert {@link android.app.ActivityManager#moveTaskToFront(int, int)} and {@link
 * android.app.ActivityManager#moveTaskToFront(int, int, android.os.Bundle)} are not used.
 *
 * <p>The Android API {@link android.app.ActivityManager#moveTaskToFront(int, int)} may throw a
 * NullPointerException when the request is blocked by the Background Activity Launch policy. This
 * is a known bug that should be fixed with subsequent releases of Android (starting with Android
 * 26Q2). See crbug.com/471434499 for more context.
 */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoMoveTaskToFront",
        summary = "Do not use android.app.ActivityManager#moveTaskToFront(int, int).",
        severity = SeverityLevel.ERROR,
        linkType = LinkType.CUSTOM,
        link = "https://crbug.com/471434499")
public class NoMoveTaskToFront extends BugChecker implements MethodInvocationTreeMatcher {
    private static final String CLASS_NAME = "android.app.ActivityManager";
    private static final String METHOD_NAME = "moveTaskToFront";

    private static final Matcher<ExpressionTree> INVOCATION_MATCHER =
            instanceMethod()
                    .onDescendantOf(Suppliers.typeFromString(CLASS_NAME))
                    .named(METHOD_NAME);

    @Override
    public Description matchMethodInvocation(MethodInvocationTree tree, VisitorState visitorState) {
        if (INVOCATION_MATCHER.matches(tree, visitorState) && !isTest(visitorState)) {
            return buildDescription(tree)
                    .setMessage(
                            "ActivityManager#moveTaskToFront may throw unexpected NPE due to a"
                                    + " platform bug. Consider using the safe version,"
                                    + " ApiCompatibilityUtils#moveTaskToFront, instead. See"
                                    + " crbug.com/471434499 for more context.")
                    .build();
        } else {
            return Description.NO_MATCH;
        }
    }

    private boolean isTest(VisitorState visitorState) {
        String filePath = visitorState.getPath().getCompilationUnit().getSourceFile().getName();
        return filePath.endsWith("Test.java");
    }
}
