// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import static com.google.errorprone.matchers.Matchers.instanceMethod;

import com.google.auto.service.AutoService;
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

/** Assert {@link android.content.res.Resources#getColor(int)} is not used. */
@AutoService(BugChecker.class)
@BugPattern(
        name = "NoResourcesGetColor",
        summary =
                "Use android.content.Context#getColor instead of"
                        + " android.content.res.Resources#getColor.",
        severity = SeverityLevel.SUGGESTION,
        linkType = LinkType.CUSTOM,
        link = "http://crbug.com/1302803")
public class NoResourcesGetColor extends BugChecker implements MethodInvocationTreeMatcher {
    private static final String CLASS_NAME = "android.content.res.Resources";
    private static final String METHOD_NAME = "getColor";

    // Allow Resources#getColor(int, Theme) on purpose as an easy escape hatch for this check, and
    // it does correctly handle themes/dynamic colors.
    private static final Matcher<ExpressionTree> CONTEXT_MATCHER =
            instanceMethod()
                    .onDescendantOf(Suppliers.typeFromString(CLASS_NAME))
                    .named(METHOD_NAME)
                    .withParameters(int.class.getName());

    @Override
    public Description matchMethodInvocation(MethodInvocationTree tree, VisitorState visitorState) {
        if (CONTEXT_MATCHER.matches(tree, visitorState)) {
            return buildDescription(tree)
                    .setMessage("Don't use Resources#getColor - call Context#getColor instead")
                    .build();
        } else {
            return Description.NO_MATCH;
        }
    }
}
