// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import static org.junit.Assert.assertEquals;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.res.ColorStateList;
import android.graphics.Color;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Tests for {@link PropertyModelAnimatorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PropertyModelAnimatorFactoryTest {
    private static final WritableFloatPropertyKey FLOAT_KEY = new WritableFloatPropertyKey();
    private static final WritableObjectPropertyKey<ColorStateList> COLOR_STATE_LIST_KEY =
            new WritableObjectPropertyKey<>();

    @Test
    public void testOfFloat_targetValue() {
        PropertyModel model = new PropertyModel.Builder(FLOAT_KEY).with(FLOAT_KEY, 0.0f).build();
        ObjectAnimator animator = PropertyModelAnimatorFactory.ofFloat(model, FLOAT_KEY, 1.0f);
        runToEnd(animator);
        assertEquals(1.0f, model.get(FLOAT_KEY), 0.001f);
    }

    @Test
    public void testOfArgb_colorStateListProperty_targetValue() {
        PropertyModel model =
                new PropertyModel.Builder(COLOR_STATE_LIST_KEY)
                        .with(COLOR_STATE_LIST_KEY, ColorStateList.valueOf(Color.RED))
                        .build();
        ObjectAnimator animator =
                PropertyModelAnimatorFactory.ofArgb(model, COLOR_STATE_LIST_KEY, Color.BLUE);
        runToEnd(animator);
        assertEquals(Color.BLUE, model.get(COLOR_STATE_LIST_KEY).getDefaultColor());
    }

    @Test
    public void testOfArgb_colorStateListProperty_multipleValues() {
        PropertyModel model =
                new PropertyModel.Builder(COLOR_STATE_LIST_KEY)
                        .with(COLOR_STATE_LIST_KEY, ColorStateList.valueOf(Color.GREEN))
                        .build();
        ObjectAnimator animator =
                PropertyModelAnimatorFactory.ofArgb(
                        model, COLOR_STATE_LIST_KEY, Color.RED, Color.BLUE);
        runToEnd(animator);
        assertEquals(Color.BLUE, model.get(COLOR_STATE_LIST_KEY).getDefaultColor());
    }

    @Test
    public void testOfArgb_colorStateListProperty_nullInitialValue() {
        PropertyModel model = new PropertyModel.Builder(COLOR_STATE_LIST_KEY).build();
        ObjectAnimator animator =
                PropertyModelAnimatorFactory.ofArgb(model, COLOR_STATE_LIST_KEY, Color.BLUE);
        runToEnd(animator);
        assertEquals(Color.BLUE, model.get(COLOR_STATE_LIST_KEY).getDefaultColor());
    }

    private static void runToEnd(Animator animator) {
        animator.setDuration(100);
        animator.start();
        animator.end();
    }
}
