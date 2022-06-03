// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.auto.service.AutoService;
import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.matchers.Matcher;
import com.google.errorprone.matchers.Matchers;
import com.sun.source.tree.AnnotationTree;

/**
 * Assert androidx.annotation.Nullable is used instead of javax.annotation.Nullable.
 */
@AutoService(BugChecker.class)
@BugPattern(name = "AndroidNullableCheck",
        summary = "Use androidx.annotation.Nullable instead of javax.annotation.Nullable.",
        severity = BugPattern.SeverityLevel.ERROR, linkType = BugPattern.LinkType.CUSTOM,
        link = "http://crbug.com/771683")
public class AndroidNullableCheck extends BugChecker implements BugChecker.AnnotationTreeMatcher {
    static final Matcher<AnnotationTree> IS_JAVAX_NULLABLE =
            Matchers.anyOf(Matchers.isType("javax.annotation.Nullable"));

    /**
     * Match if nullable annotation is of type javax.annotation.Nullable.
     */
    @Override
    public Description matchAnnotation(AnnotationTree annotationTree, VisitorState visitorState) {
        if (IS_JAVAX_NULLABLE.matches(annotationTree, visitorState)) {
            return describeMatch(annotationTree);
        }
        return Description.NO_MATCH;
    }
}
