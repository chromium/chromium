// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import static com.google.errorprone.matchers.Matchers.instanceMethod;

import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.matchers.Matcher;
import com.google.errorprone.suppliers.Supplier;
import com.google.errorprone.suppliers.Suppliers;
import com.sun.source.tree.ExpressionTree;
import com.sun.source.tree.MethodInvocationTree;
import com.sun.tools.javac.code.Type;

import org.chromium.build.annotations.ServiceImpl;

/**
 * Checks for calls to getApplicationContext from {@link android.content.Context}. These calls
 * should be replaced with the static getApplicationContext method in {@link
 * org.chromium.base.ContextUtils}.
 */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoContextGetApplicationContext",
        summary = "Do not use Context#getApplicationContext",
        severity = BugPattern.SeverityLevel.ERROR,
        linkType = BugPattern.LinkType.CUSTOM,
        link = "https://bugs.chromium.org/p/chromium/issues/detail?id=560466")
public class NoContextGetApplicationContext extends BugChecker
        implements BugChecker.MethodInvocationTreeMatcher {
    private static final String CONTEXT_CLASS_NAME = "android.content.Context";
    private static final String CONTEXT_UTILS_CLASS_NAME = "org.chromium.base.ContextUtils";
    private static final String METHOD_NAME = "getApplicationContext";

    private static final Supplier<Type> CONTEXT_UTILS_SUPPLIER =
            Suppliers.typeFromString(CONTEXT_UTILS_CLASS_NAME);
    private static final Matcher<ExpressionTree> CONTEXT_MATCHER =
            instanceMethod()
                    .onDescendantOf(Suppliers.typeFromString(CONTEXT_CLASS_NAME))
                    .named(METHOD_NAME);

    @Override
    public Description matchMethodInvocation(MethodInvocationTree tree, VisitorState visitorState) {
        if (!CONTEXT_MATCHER.matches(tree, visitorState)) {
            return Description.NO_MATCH;
        }

        // If ContextUtils can't be loaded, we are probably inside a third_party lib and shouldn't
        // check for errors.
        boolean canLoadContextUtils = CONTEXT_UTILS_SUPPLIER.get(visitorState) != null;
        if (!canLoadContextUtils) {
            return Description.NO_MATCH;
        }

        return buildDescription(tree)
                .setMessage(
                        "Don't use Context#getApplicationContext - "
                                + "call ContextUtils.getApplicationContext instead")
                .build();
    }
}
