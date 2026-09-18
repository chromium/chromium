// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import android.graphics.Color;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SubscriptSpan;
import android.text.style.SuperscriptSpan;

/** Test for NoClippedSuperscript that verifies valid code compiles without warnings. */
public class NoClippedSuperscriptTestCompile {
    public void superscriptOnTextThatIsReallySuperscript(SpannableString text) {
        // No trigger: raising text without shrinking it is how the accessibility code maps the
        // web's <sup> onto Android text, and it stays allowed.
        text.setSpan(new SuperscriptSpan(), 0, 3, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
    }

    public void superscriptAlongsideOtherStyleSpans(SpannableString text) {
        // No trigger: the accessibility code walks a list of styles and hands back whichever span
        // matches, so a superscript sits next to unrelated spans in the same method.
        text.setSpan(new SubscriptSpan(), 0, 1, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        text.setSpan(new SuperscriptSpan(), 1, 2, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        text.setSpan(new ForegroundColorSpan(Color.RED), 2, 3, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
    }

    public void shrinkingTextOnItsOwn(SpannableString text) {
        // No trigger: plenty of text is smaller than its neighbours without being raised.
        text.setSpan(new RelativeSizeSpan(0.75f), 0, 3, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
    }

    @SuppressWarnings("NoClippedSuperscript") // Pretend this is an exponent we've measured.
    public void suppressedOnPurpose(SpannableString text) {
        // No trigger: the message tells people to opt out this way, so make sure it works.
        text.setSpan(new SuperscriptSpan(), 0, 3, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        text.setSpan(new RelativeSizeSpan(0.6f), 0, 3, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
    }
}
