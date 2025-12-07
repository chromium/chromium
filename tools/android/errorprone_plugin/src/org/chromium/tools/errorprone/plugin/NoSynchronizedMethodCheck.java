// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.fixes.SuggestedFixes;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.MethodTree;
import com.sun.tools.javac.code.Symbol;

import org.chromium.build.annotations.ServiceImpl;

import javax.lang.model.element.Modifier;

/** Triggers an error for public methods that use "synchronized" in their signature. */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoSynchronizedMethodCheck",
        summary = "Use of synchronized in public method signature disallowed.",
        severity = BugPattern.SeverityLevel.ERROR,
        linkType = BugPattern.LinkType.CUSTOM,
        link =
                "https://stackoverflow.com/questions/20906548/why-is-synchronized-block-better-than-synchronized-method")
public class NoSynchronizedMethodCheck extends BugChecker implements BugChecker.MethodTreeMatcher {
    @Override
    public Description matchMethod(MethodTree methodTree, VisitorState visitorState) {
        Symbol.MethodSymbol method = ASTHelpers.getSymbol(methodTree);
        // Skip methods that aren't synchronized and non-public methods
        if (!method.getModifiers().contains(Modifier.SYNCHRONIZED)
                || !method.getModifiers().contains(Modifier.PUBLIC)) {
            return Description.NO_MATCH;
        }
        // Skip methods that are only public due to VisibleForTesting
        if (ASTHelpers.hasDirectAnnotationWithSimpleName(method, "VisibleForTesting")) {
            return Description.NO_MATCH;
        }
        // A Synchronized @Override methods is unavoidable if the method being overridden is
        // an Android API method (Example: Exception#fillInStackTrace()).
        if (ASTHelpers.hasDirectAnnotationWithSimpleName(method, "Override")) {
            return Description.NO_MATCH;
        }
        // Skip non-public classes
        Symbol.ClassSymbol enclosingClass = ASTHelpers.enclosingClass(method);
        if (!enclosingClass.getModifiers().contains(Modifier.PUBLIC)) {
            return Description.NO_MATCH;
        }
        return buildDescription(methodTree)
                .addFix(
                        SuggestedFixes.removeModifiers(
                                methodTree, visitorState, Modifier.SYNCHRONIZED))
                .setMessage(
                        String.format(
                                "Used synchronized modifier in public method %s",
                                method.getSimpleName()))
                .build();
    }
}
