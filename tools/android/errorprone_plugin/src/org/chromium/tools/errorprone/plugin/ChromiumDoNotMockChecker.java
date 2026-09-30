// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.common.collect.ImmutableMap;
import com.google.common.collect.Lists;
import com.google.errorprone.BugPattern;
import com.google.errorprone.bugpatterns.AbstractMockChecker;
import com.google.errorprone.bugpatterns.BugChecker;
import com.sun.tools.javac.code.Type;

import org.chromium.build.annotations.ServiceImpl;

import java.util.Optional;

/**
 * Extends Error Prone's built-in DoNotMock check to types that we cannot annotate with @DoNotMock
 * (e.g. Android framework classes).
 *
 * <p>Error Prone does not allow plugins to replace built-in checks (it throws when two checks share
 * a canonical name), so this check instead uses "DoNotMock" as an alternate name. This means that
 * {@code @SuppressWarnings("DoNotMock")} and {@code -Xep:DoNotMock:...} apply to both checks. The
 * built-in check continues to handle @DoNotMock annotations.
 */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "ChromiumDoNotMock",
        altNames = "DoNotMock",
        severity = BugPattern.SeverityLevel.ERROR,
        summary = "Identifies undesirable mocks.",
        linkType = BugPattern.LinkType.NONE,
        documentSuppression = false)
public class ChromiumDoNotMockChecker
        extends AbstractMockChecker<ChromiumDoNotMockChecker.UnusedAnnotation> {
    /**
     * Never applied to anything. AbstractMockChecker requires an annotation, but the built-in
     * DoNotMock check already handles @DoNotMock, so this check only uses {@link #forbidder()}.
     */
    @interface UnusedAnnotation {}

    // Map of fully-qualified class name -> reason it should not be mocked.
    private static final ImmutableMap<String, String> DISALLOWED_TYPES =
            ImmutableMap.of(
                    "android.app.Activity",
                    "Use Robolectric.buildActivity() or a real Activity instead",
                    "android.view.View",
                    "Use a real View instead");

    public ChromiumDoNotMockChecker() {
        super(MOCKED_VAR, MOCKING_METHOD, UnusedAnnotation.class, unused -> "");
    }

    @Override
    protected MockForbidder forbidder() {
        // Walks the type closure in the same order as AbstractMockChecker does for annotations.
        return (mockedType, state) -> {
            for (Type currentType : Lists.reverse(state.getTypes().closure(mockedType))) {
                String reason =
                        DISALLOWED_TYPES.get(currentType.tsym.getQualifiedName().toString());
                if (reason != null) {
                    return Optional.of(Reason.of(currentType, reason));
                }
            }
            return Optional.empty();
        };
    }
}
