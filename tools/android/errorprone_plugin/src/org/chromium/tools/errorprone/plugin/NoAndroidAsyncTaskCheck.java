// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.MemberSelectTree;
import com.sun.tools.javac.code.Symbol;

import org.chromium.build.annotations.ServiceImpl;

/** Triggers an error for any occurrence of android.os.AsyncTask. */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoAndroidAsyncTaskCheck",
        summary = "Do not use android.os.AsyncTask - use org.chromium.base.task.AsyncTask instead",
        severity = BugPattern.SeverityLevel.ERROR,
        linkType = BugPattern.LinkType.CUSTOM,
        link = "https://bugs.chromium.org/p/chromium/issues/detail?id=843745")
public class NoAndroidAsyncTaskCheck extends BugChecker
        implements BugChecker.MemberSelectTreeMatcher {
    @Override
    public Description matchMemberSelect(MemberSelectTree tree, VisitorState state) {
        if (tree.getIdentifier().contentEquals("AsyncTask")) {
            Symbol symbol = ASTHelpers.getSymbol(tree.getExpression());
            if (symbol.getQualifiedName().contentEquals("android.os")) {
                return buildDescription(tree)
                        .setMessage(
                                "Do not use android.os.AsyncTask - "
                                        + "use org.chromium.base.task.AsyncTask instead")
                        .build();
            }
        }
        return Description.NO_MATCH;
    }
}
