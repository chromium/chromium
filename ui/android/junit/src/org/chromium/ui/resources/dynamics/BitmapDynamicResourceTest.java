// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;

import android.graphics.Bitmap;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.ref.WeakReference;

/**
 * Tests for {@link BitmapDynamicResource}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BitmapDynamicResourceTest {
    private BitmapDynamicResource mResource;

    @Before
    public void setup() {
        mResource = new BitmapDynamicResource(1);
    }

    @Test
    public void testGetBitmap() {
        Bitmap bitmap = Bitmap.createBitmap(1, 2, Bitmap.Config.ARGB_8888);
        mResource.setBitmap(bitmap);
        assertEquals(bitmap, mResource.getBitmap());
    }

    @Test
    public void testSetBitmapGCed() {
        Bitmap bitmap = Bitmap.createBitmap(1, 2, Bitmap.Config.ARGB_8888);
        WeakReference<Bitmap> bitmapWeakReference = new WeakReference<>(bitmap);
        mResource.setBitmap(bitmap);
        bitmap = null;
        assertFalse(canBeGarbageCollected(bitmapWeakReference));

        Bitmap bitmap2 = Bitmap.createBitmap(3, 4, Bitmap.Config.ARGB_8888);
        mResource.setBitmap(bitmap2);
        assertTrue(canBeGarbageCollected(bitmapWeakReference));
    }

    @Test
    public void testGetBitmapGCed() {
        Bitmap bitmap = Bitmap.createBitmap(1, 2, Bitmap.Config.ARGB_8888);
        WeakReference<Bitmap> bitmapWeakReference = new WeakReference<>(bitmap);
        mResource.setBitmap(bitmap);
        bitmap = null;
        assertFalse(canBeGarbageCollected(bitmapWeakReference));

        mResource.getBitmap();
        assertTrue(canBeGarbageCollected(bitmapWeakReference));
    }
}
