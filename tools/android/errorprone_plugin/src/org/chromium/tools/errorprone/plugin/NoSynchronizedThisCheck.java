// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.SynchronizedTree;
import com.sun.tools.javac.code.Symbol;
import com.sun.tools.javac.tree.JCTree;
import com.sun.tools.javac.tree.TreeInfo;

import org.chromium.build.annotations.ServiceImpl;

import javax.lang.model.element.Modifier;

/** This class detects the synchronized method. */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoSynchronizedThisCheck",
        summary = "Do not synchronized on 'this' in public classes",
        severity = BugPattern.SeverityLevel.ERROR,
        linkType = BugPattern.LinkType.CUSTOM,
        link = "https://stackoverflow.com/questions/442564/avoid-synchronizedthis-in-java")
public class NoSynchronizedThisCheck extends BugChecker
        implements BugChecker.SynchronizedTreeMatcher {
    @Override
    public Description matchSynchronized(SynchronizedTree tree, VisitorState visitorState) {
        Symbol lock = ASTHelpers.getSymbol(TreeInfo.skipParens((JCTree) tree.getExpression()));
        // Skip locks that are not 'this'
        if (!lock.getSimpleName().contentEquals("this")) {
            return Description.NO_MATCH;
        }
        // Skip non-public classes
        Symbol.ClassSymbol enclosingClass = ASTHelpers.enclosingClass(lock);
        if (!enclosingClass.getModifiers().contains(Modifier.PUBLIC)) {
            return Description.NO_MATCH;
        }
        return buildDescription(tree)
                .setMessage("Used instance variable as synchronization lock")
                .build();
    }
}
