// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.errorprone.BugPattern;
import com.google.errorprone.BugPattern.LinkType;
import com.google.errorprone.BugPattern.SeverityLevel;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.bugpatterns.BugChecker.MemberSelectTreeMatcher;
import com.google.errorprone.bugpatterns.BugChecker.MethodInvocationTreeMatcher;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.ImportTree;
import com.sun.source.tree.MemberSelectTree;
import com.sun.source.tree.MethodInvocationTree;
import com.sun.tools.javac.code.Symbol;

import org.chromium.build.annotations.ServiceImpl;

/**
 * Checks that android.util.Log is not used directly.
 *
 * <p>This check catches both: 1: Method invocations: {@code Log.d(TAG, "msg")} (imported or fully
 * qualified) 2: Fully qualified references: {@code android.util.Log} (e.g. used as a type)
 *
 * <p>Warnings are reported on usage sites (not imports), so
 * {@code @SuppressWarnings("NoAndroidLog")} can be used to suppress them at the method or class
 * level.
 */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoAndroidLog",
        summary = "Use org.chromium.base.Log instead of android.util.Log.",
        severity = SeverityLevel.WARNING,
        linkType = LinkType.CUSTOM,
        link = "https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_logging.md")
public class NoAndroidLog extends BugChecker
        implements MethodInvocationTreeMatcher, MemberSelectTreeMatcher {
    private static final String ANDROID_LOG_CLASS = "android.util.Log";

    private static final String ERROR_MESSAGE =
            "Do not use android.util.Log directly. Use org.chromium.base.Log instead.";

    @Override
    public Description matchMethodInvocation(MethodInvocationTree tree, VisitorState state) {
        Symbol.MethodSymbol method = ASTHelpers.getSymbol(tree);
        if (method == null) {
            return Description.NO_MATCH;
        }

        // Check if the method belongs to android.util.Log.
        String className = method.enclClass().fullname.toString();
        if (!ANDROID_LOG_CLASS.equals(className)) {
            return Description.NO_MATCH;
        }

        return buildDescription(tree).setMessage(ERROR_MESSAGE).build();
    }

    @Override
    public Description matchMemberSelect(MemberSelectTree tree, VisitorState state) {
        // Skip if inside an import statement.
        if (ASTHelpers.findEnclosingNode(state.getPath(), ImportTree.class) != null) {
            return Description.NO_MATCH;
        }

        // Skip if inside a method invocation.
        if (ASTHelpers.findEnclosingNode(state.getPath(), MethodInvocationTree.class) != null) {
            return Description.NO_MATCH;
        }

        // Check for fully qualified usage like android.util.Log as a type reference.
        if (!tree.getIdentifier().contentEquals("Log")) {
            return Description.NO_MATCH;
        }

        Symbol symbol = ASTHelpers.getSymbol(tree.getExpression());
        if (symbol == null || !symbol.getQualifiedName().contentEquals("android.util")) {
            return Description.NO_MATCH;
        }

        return buildDescription(tree).setMessage(ERROR_MESSAGE).build();
    }
}
