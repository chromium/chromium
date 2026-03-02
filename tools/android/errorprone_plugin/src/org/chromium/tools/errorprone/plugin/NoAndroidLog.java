// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.errorprone.BugPattern;
import com.google.errorprone.BugPattern.LinkType;
import com.google.errorprone.BugPattern.SeverityLevel;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.bugpatterns.BugChecker.IdentifierTreeMatcher;
import com.google.errorprone.bugpatterns.BugChecker.MemberSelectTreeMatcher;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.IdentifierTree;
import com.sun.source.tree.ImportTree;
import com.sun.source.tree.MemberSelectTree;
import com.sun.source.tree.Tree;
import com.sun.tools.javac.code.Symbol;

import org.chromium.build.annotations.ServiceImpl;

/**
 * Checks that android.util.Log is not used directly.
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
        implements IdentifierTreeMatcher, MemberSelectTreeMatcher {
    private static final String ANDROID_LOG_CLASS = "android.util.Log";

    private static final String ERROR_MESSAGE =
            "Do not use android.util.Log directly. Use org.chromium.base.Log instead.";

    @Override
    public Description matchIdentifier(IdentifierTree tree, VisitorState state) {
        if (isImport(state)) return Description.NO_MATCH;
        if (isAndroidLog(ASTHelpers.getSymbol(tree)) && !isParentAMatchingMemberSelect(state)) {
            return buildDescription(tree).setMessage(ERROR_MESSAGE).build();
        }
        return Description.NO_MATCH;
    }

    @Override
    public Description matchMemberSelect(MemberSelectTree tree, VisitorState state) {
        if (isImport(state)) return Description.NO_MATCH;
        if (isAndroidLog(ASTHelpers.getSymbol(tree)) && !isParentAMatchingMemberSelect(state)) {
            return buildDescription(tree).setMessage(ERROR_MESSAGE).build();
        }
        return Description.NO_MATCH;
    }

    private boolean isAndroidLog(Symbol symbol) {
        if (symbol == null) return false;
        if (symbol instanceof Symbol.ClassSymbol) {
            return symbol.getQualifiedName().contentEquals(ANDROID_LOG_CLASS);
        }
        return symbol.owner != null
                && symbol.owner.getQualifiedName().contentEquals(ANDROID_LOG_CLASS);
    }

    private boolean isImport(VisitorState state) {
        return ASTHelpers.findEnclosingNode(state.getPath(), ImportTree.class) != null;
    }

    private boolean isParentAMatchingMemberSelect(VisitorState state) {
        Tree parent = state.getPath().getParentPath().getLeaf();
        if (parent instanceof MemberSelectTree) {
            return isAndroidLog(ASTHelpers.getSymbol(parent));
        }
        return false;
    }
}
