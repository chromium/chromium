// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;

/** Test for the NoClippedSuperscript checker. */
public class NoClippedSuperscriptNoCompileSources {
    public void raisedAndShrunkInline(SpannableString text) {
        text.setSpan(new SuperscriptSpan(), 0, 3, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        text.setSpan(new RelativeSizeSpan(0.75f), 0, 3, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
    }

    public void raisedAndShrunkViaLocals(SpannableString text) {
        SuperscriptSpan raised = new SuperscriptSpan();
        RelativeSizeSpan smaller = new RelativeSizeSpan(0.6f);
        text.setSpan(raised, 0, 3, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        text.setSpan(smaller, 0, 3, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
    }
}
