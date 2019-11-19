// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.drawable;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.graphics.drawable.AnimatedStateListDrawable;
import android.graphics.drawable.StateListDrawable;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowStateListDrawable;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.shadows.ShadowAnimatedStateListDrawable;
import org.chromium.ui.shadows.ShadowAppCompatResources;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class, ShadowAnimatedStateListDrawable.class})
public class StateListDrawableBuilderTest {
    private static final int[] CHECKED_STATE = new int[] {android.R.attr.state_checked};
    private static final int[] WILDCARD_STATE = new int[0];
    private static final int CHECKED_DRAWABLE = 34567;
    private static final int DEFAULT_DRAWABLE = 45678;

    @Test
    @Config(sdk = 19)
    public void testPreL() {
        StateListDrawableBuilder b = new StateListDrawableBuilder(RuntimeEnvironment.application);
        b.addState(CHECKED_DRAWABLE, android.R.attr.state_checked);
        b.addState(DEFAULT_DRAWABLE, WILDCARD_STATE);
        StateListDrawable result = b.build();
        assertEquals(result.getClass(), StateListDrawable.class);
        ShadowStateListDrawable drawable = shadowOf(result);
        assertEquals(CHECKED_DRAWABLE,
                shadowOf(drawable.getDrawableForState(CHECKED_STATE)).getCreatedFromResId());
        assertEquals(DEFAULT_DRAWABLE,
                shadowOf(drawable.getDrawableForState(WILDCARD_STATE)).getCreatedFromResId());
    }

    @Test
    @Config(sdk = 21)
    public void testPostL() {
        StateListDrawableBuilder b = new StateListDrawableBuilder(RuntimeEnvironment.application);
        b.addState(CHECKED_DRAWABLE, android.R.attr.state_checked);
        b.addState(DEFAULT_DRAWABLE, WILDCARD_STATE);
        StateListDrawable result = b.build();
        assertEquals(result.getClass(), AnimatedStateListDrawable.class);
        ShadowAnimatedStateListDrawable drawable = Shadow.extract(result);
        assertEquals(CHECKED_DRAWABLE,
                shadowOf(drawable.getDrawableForState(CHECKED_STATE)).getCreatedFromResId());
        assertEquals(DEFAULT_DRAWABLE,
                shadowOf(drawable.getDrawableForState(WILDCARD_STATE)).getCreatedFromResId());
    }
}
