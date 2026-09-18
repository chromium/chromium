// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.errorprone.BugPattern;
import com.google.errorprone.BugPattern.LinkType;
import com.google.errorprone.BugPattern.SeverityLevel;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.bugpatterns.BugChecker.NewClassTreeMatcher;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.matchers.Matcher;
import com.google.errorprone.matchers.Matchers;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.MethodTree;
import com.sun.source.tree.NewClassTree;
import com.sun.source.tree.Tree;
import com.sun.source.util.TreeScanner;

import org.chromium.build.annotations.ServiceImpl;

/**
 * Catches text that gets raised and shrunk without the line being told to make room for it.
 *
 * <p>SuperscriptSpan lifts text above the baseline but leaves the line metrics alone, so anything
 * that reaches high above the cap height gets its top sliced off. Thai is the usual victim, where
 * the vowel signs that sit above the line lose their heads.
 *
 * <p>Most of the time this turns out to be a "New" badge, which is what NewLabelUtils is for. It
 * can also be an exponent, a footnote marker or a currency superscript, and those have the same
 * clipping problem without the same fix available, so the message offers a way out.
 *
 * <p>A SuperscriptSpan on its own is fine and stays fine - that's how the accessibility code maps
 * the web's &lt;sup&gt; onto Android text. Shrinking the text at the same time is what says someone
 * is styling a small raised run rather than marking up real superscript content.
 */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoClippedSuperscript",
        summary = "Raising and shrinking text without reserving room for it clips tall glyphs.",
        severity = SeverityLevel.ERROR,
        linkType = LinkType.CUSTOM,
        link = "https://crbug.com/512323229")
public class NoClippedSuperscript extends BugChecker implements NewClassTreeMatcher {
    private static final Matcher<Tree> SUPERSCRIPT_SPAN =
            Matchers.isSubtypeOf("android.text.style.SuperscriptSpan");
    private static final Matcher<Tree> RELATIVE_SIZE_SPAN =
            Matchers.isSubtypeOf("android.text.style.RelativeSizeSpan");

    @Override
    public Description matchNewClass(NewClassTree tree, VisitorState state) {
        // Start from the SuperscriptSpan because hardly anything constructs one, which keeps the
        // walk below off the hot path for the rest of the codebase.
        if (!SUPERSCRIPT_SPAN.matches(tree, state)) {
            return Description.NO_MATCH;
        }

        MethodTree method = ASTHelpers.findEnclosingNode(state.getPath(), MethodTree.class);
        if (method == null || !hasRelativeSizeSpan(method, state)) {
            return Description.NO_MATCH;
        }

        return buildDescription(tree)
                .setMessage(
                        "SuperscriptSpan raises this text without asking the line for more room, so"
                            + " the top of it gets clipped in languages with tall glyphs such as"
                            + " Thai. If this is a \"New\" badge, use NewLabelUtils.withBadge()"
                            + " from //components/browser_ui/styles, which reserves the space. If"
                            + " it is something else, like an exponent or a footnote marker, the"
                            + " same clipping applies but there is no drop-in fix yet - add"
                            + " @SuppressWarnings(\"NoClippedSuperscript\") with a note explaining"
                            + " why it is safe here, and please comment on the linked bug so we"
                            + " know a second caller exists.")
                .build();
    }

    /**
     * Returns whether the method builds a RelativeSizeSpan anywhere. The whole method is fair game
     * rather than just the surrounding statement, because the spans often get assigned to locals
     * before they're applied.
     */
    private static boolean hasRelativeSizeSpan(MethodTree method, VisitorState state) {
        Boolean found =
                new TreeScanner<Boolean, Void>() {
                    @Override
                    public Boolean visitNewClass(NewClassTree node, Void unused) {
                        if (RELATIVE_SIZE_SPAN.matches(node, state)) {
                            return true;
                        }
                        return super.visitNewClass(node, unused);
                    }

                    @Override
                    public Boolean reduce(Boolean left, Boolean right) {
                        return Boolean.TRUE.equals(left) || Boolean.TRUE.equals(right);
                    }
                }.scan(method.getBody(), null);
        return Boolean.TRUE.equals(found);
    }
}
