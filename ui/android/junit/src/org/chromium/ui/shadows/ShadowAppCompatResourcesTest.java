// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.shadows;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests logic in the ShadowAppCompatResources class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class ShadowAppCompatResourcesTest {
    private static final int DRAWABLE_RES_ID = 34567;

    @Test
    public void testShadowAppCompatResources() {
        Drawable drawable =
                AppCompatResources.getDrawable(RuntimeEnvironment.application, DRAWABLE_RES_ID);
        assertEquals(DRAWABLE_RES_ID, shadowOf(drawable).getCreatedFromResId());
    }
}
