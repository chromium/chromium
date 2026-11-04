// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.content.Context;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;

/**
 * A {@link FrameLayout} that records every haptic feedback constant performed on it, so tests can
 * count haptics.
 *
 * <p>Robolectric's {@code ShadowView#lastHapticFeedbackPerformed()} only exposes the last constant,
 * and Chromium's DoNotMock Error Prone check disallows mocking or spying on Views.
 */
@NullMarked
public class TestHapticFeedbackView extends FrameLayout {
    private final List<Integer> mHapticFeedbackConstants = new ArrayList<>();

    public TestHapticFeedbackView(Context context) {
        super(context);
    }

    @Override
    public boolean performHapticFeedback(int feedbackConstant) {
        mHapticFeedbackConstants.add(feedbackConstant);
        return true;
    }

    /** Returns the haptic feedback constants performed on this view, in order. */
    public List<Integer> getHapticFeedbackConstants() {
        return mHapticFeedbackConstants;
    }
}
