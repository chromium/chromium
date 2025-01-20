// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import static com.google.errorprone.matchers.Matchers.instanceMethod;

import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.matchers.Matcher;
import com.google.errorprone.predicates.TypePredicates;
import com.sun.source.tree.ExpressionTree;
import com.sun.source.tree.MethodInvocationTree;

import org.chromium.build.annotations.ServiceImpl;

/** Checks for calls to .stream(). See //styleguide/java/java.md for rationale. */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoStreams",
        summary = "Prefer loops over stream().",
        severity = BugPattern.SeverityLevel.WARNING,
        linkType = BugPattern.LinkType.CUSTOM,
        link =
                "https://chromium.googlesource.com/chromium/src/+/main/styleguide/java/java.md#Streams")
public class NoStreams extends BugChecker implements BugChecker.MethodInvocationTreeMatcher {
    private static final Matcher<ExpressionTree> MATCHER =
            instanceMethod().onClass(TypePredicates.isDescendantOf("java.util.stream.Stream"));

    @Override
    public Description matchMethodInvocation(MethodInvocationTree tree, VisitorState visitorState) {
        if (!MATCHER.matches(tree, visitorState)) {
            return Description.NO_MATCH;
        }

        return buildDescription(tree)
                .setMessage("Using Java stream APIs is strongly discouraged. Use loops instead.")
                .build();
    }
}
