// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.errorprone.BugPattern;
import com.google.errorprone.BugPattern.LinkType;
import com.google.errorprone.BugPattern.SeverityLevel;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.bugpatterns.BugChecker.ImportTreeMatcher;
import com.google.errorprone.bugpatterns.BugChecker.MemberSelectTreeMatcher;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.ClassTree;
import com.sun.source.tree.ImportTree;
import com.sun.source.tree.MemberSelectTree;
import com.sun.tools.javac.code.Symbol;

import org.chromium.build.annotations.ServiceImpl;

/**
 * Checks that android.util.Log is not used directly.
 *
 * <p>This check catches both: 1: Import statements: {@code import android.util.Log} 2: Fully
 * qualified usage: {@code android.util.Log.d(...)}
 */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoAndroidLog",
        summary = "Use org.chromium.base.Log instead of android.util.Log.",
        severity = SeverityLevel.WARNING,
        linkType = LinkType.CUSTOM,
        link = "https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_logging.md")
public class NoAndroidLog extends BugChecker implements ImportTreeMatcher, MemberSelectTreeMatcher {
    private static final String ANDROID_LOG_CLASS = "android.util.Log";
    private static final String CHROMIUM_LOG_CLASS = "org.chromium.base.Log";

    private static final String ERROR_MESSAGE =
            "Do not use android.util.Log directly. Use org.chromium.base.Log instead.";

    @Override
    public Description matchImport(ImportTree tree, VisitorState state) {
        String importName = tree.getQualifiedIdentifier().toString();
        if (importName.equals(ANDROID_LOG_CLASS)
                || importName.startsWith(ANDROID_LOG_CLASS + ".")) {
            return buildDescription(tree).setMessage(ERROR_MESSAGE).build();
        }
        return Description.NO_MATCH;
    }

    @Override
    public Description matchMemberSelect(MemberSelectTree tree, VisitorState state) {
        // Check for fully qualified usage like android.util.Log.d()
        if (!tree.getIdentifier().contentEquals("Log")) {
            return Description.NO_MATCH;
        }

        Symbol symbol = ASTHelpers.getSymbol(tree.getExpression());
        if (symbol == null || !symbol.getQualifiedName().contentEquals("android.util")) {
            return Description.NO_MATCH;
        }

        // Allow usage in org.chromium.base.Log itself.
        ClassTree enclosingClassTree =
                ASTHelpers.findEnclosingNode(state.getPath(), ClassTree.class);
        if (enclosingClassTree != null) {
            Symbol.ClassSymbol enclosingClass = ASTHelpers.getSymbol(enclosingClassTree);
            if (enclosingClass != null
                    && enclosingClass.getQualifiedName().contentEquals(CHROMIUM_LOG_CLASS)) {
                return Description.NO_MATCH;
            }
        }

        return buildDescription(tree).setMessage(ERROR_MESSAGE).build();
    }
}
