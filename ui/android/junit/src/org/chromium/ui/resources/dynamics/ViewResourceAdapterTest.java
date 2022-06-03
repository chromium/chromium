// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.ref.WeakReference;

/**
 * Tests for {@link ViewResourceAdapter}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ViewResourceAdapterTest {
    private int mViewWidth;
    private int mViewHeight;
    @Mock
    private View mView;

    private ViewResourceAdapter mAdapter;

    @Before
    public void setup() {
        initMocks(this);

        mViewWidth = 200;
        mViewHeight = 100;

        when(mView.getWidth()).thenAnswer((invocation) -> mViewWidth);
        when(mView.getHeight()).thenAnswer((invocation) -> mViewHeight);

        mAdapter = new ViewResourceAdapter(mView) {
            /**
             * Mock this out to avoid calling {@link View#draw(Canvas)} on the mocked mView.
             * Otherwise the GC-related tests would fail.
             */
            @Override
            protected void capture(Canvas canvas) {}
        };

    }

    @Test
    public void testGetBitmap() {
        Bitmap bitmap = mAdapter.getBitmap();
        assertNotNull(bitmap);
        assertEquals(mViewWidth, bitmap.getWidth());
        assertEquals(mViewHeight, bitmap.getHeight());
    }

    @Test
    public void testGetBitmapSize() {
        Bitmap bitmap = mAdapter.getBitmap();
        Rect rect = mAdapter.getBitmapSize();
        assertEquals(bitmap.getWidth(), rect.width());
        assertEquals(bitmap.getHeight(), rect.height());
    }

    @Test
    public void testSetDownsamplingSize() {
        float scale = 0.5f;
        mAdapter.setDownsamplingScale(scale);
        Bitmap bitmap = mAdapter.getBitmap();
        assertEquals(mViewWidth * scale, bitmap.getWidth(), 1);
        assertEquals(mViewHeight * scale, bitmap.getHeight(), 1);

        Rect rect = mAdapter.getBitmapSize();
        assertEquals(mViewWidth, rect.width());
        assertEquals(mViewHeight, rect.height());
    }

    @Test
    public void testIsDirty() {
        assertTrue(mAdapter.isDirty());

        mAdapter.getBitmap();
        assertFalse(mAdapter.isDirty());
    }

    @Test
    public void testOnLayoutChange() {
        mAdapter.getBitmap();
        assertFalse(mAdapter.isDirty());

        mAdapter.onLayoutChange(mView, 0, 0, 1, 2, 0, 0, mViewWidth, mViewHeight);
        assertTrue(mAdapter.isDirty());

        Rect dirtyRect = mAdapter.getDirtyRect();
        assertEquals(1, dirtyRect.width());
        assertEquals(2, dirtyRect.height());
    }

    @Test
    public void testOnLayoutChangeDownsampled() {
        mAdapter.setDownsamplingScale(0.5f);

        mAdapter.getBitmap();
        assertFalse(mAdapter.isDirty());

        mAdapter.onLayoutChange(mView, 0, 0, 1, 2, 0, 0, mViewWidth, mViewHeight);
        assertTrue(mAdapter.isDirty());

        Rect dirtyRect = mAdapter.getDirtyRect();
        assertEquals(1, dirtyRect.width());
        assertEquals(2, dirtyRect.height());
    }

    @Test
    public void testInvalidate() {
        mAdapter.getBitmap();
        assertFalse(mAdapter.isDirty());

        mAdapter.invalidate(null);
        assertTrue(mAdapter.isDirty());

        Rect dirtyRect = mAdapter.getDirtyRect();
        assertEquals(mViewWidth, dirtyRect.width());
        assertEquals(mViewHeight, dirtyRect.height());
    }

    @Test
    public void testInvalidateRect() {
        mAdapter.getBitmap();
        assertFalse(mAdapter.isDirty());

        Rect dirtyRect = new Rect(1, 2, 3, 4);
        mAdapter.invalidate(dirtyRect);
        assertTrue(mAdapter.isDirty());
        assertEquals(dirtyRect.toString(), mAdapter.getDirtyRect().toString());
    }

    @Test
    public void testInvalidateRectDownsampled() {
        mAdapter.setDownsamplingScale(0.5f);

        mAdapter.getBitmap();
        assertFalse(mAdapter.isDirty());

        Rect dirtyRect = new Rect(1, 2, 3, 4);
        mAdapter.invalidate(dirtyRect);
        assertTrue(mAdapter.isDirty());
        assertEquals(dirtyRect.toString(), mAdapter.getDirtyRect().toString());
    }

    @Test
    public void testInvalidateRectUnion() {
        mAdapter.getBitmap();
        assertFalse(mAdapter.isDirty());

        mAdapter.invalidate(new Rect(1, 2, 3, 4));
        mAdapter.invalidate(new Rect(5, 6, 7, 8));
        assertTrue(mAdapter.isDirty());
        Rect expected = new Rect(1, 2, 7, 8);
        assertEquals(expected.toString(), mAdapter.getDirtyRect().toString());
    }

    @Test
    public void testGetBitmapResized() {
        Bitmap bitmap = mAdapter.getBitmap();
        assertNotNull(bitmap);
        assertEquals(mViewWidth, bitmap.getWidth());
        assertEquals(mViewHeight, bitmap.getHeight());

        mViewWidth = 10;
        mViewHeight = 20;
        mAdapter.invalidate(null);
        Bitmap bitmap2 = mAdapter.getBitmap();
        assertNotNull(bitmap2);
        assertEquals(mViewWidth, bitmap2.getWidth());
        assertEquals(mViewHeight, bitmap2.getHeight());
        assertNotEquals(bitmap, bitmap2);
    }

    @Test
    public void testBitmapReused() {
        Bitmap bitmap = mAdapter.getBitmap();
        assertNotNull(bitmap);

        mAdapter.invalidate(null);
        assertTrue(mAdapter.isDirty());
        assertEquals(bitmap, mAdapter.getBitmap());
    }

    @Test
    public void testDropCachedBitmap() {
        Bitmap bitmap = mAdapter.getBitmap();
        assertNotNull(bitmap);

        mAdapter.invalidate(null);
        assertTrue(mAdapter.isDirty());
        assertEquals(bitmap, mAdapter.getBitmap());

        mAdapter.dropCachedBitmap();
        mAdapter.invalidate(null);
        assertTrue(mAdapter.isDirty());
        assertNotEquals(bitmap, mAdapter.getBitmap());
    }

    @Test
    public void testDropCachedBitmapNotDirty() {
        mAdapter.getBitmap();
        mAdapter.dropCachedBitmap();
        assertFalse(mAdapter.isDirty());
    }

    @Test
    public void testDropCachedBitmapGCed() {
        WeakReference<Bitmap> bitmapWeakReference = new WeakReference<>(mAdapter.getBitmap());
        assertNotNull(bitmapWeakReference.get());
        assertFalse(canBeGarbageCollected(bitmapWeakReference));

        mAdapter.dropCachedBitmap();
        assertTrue(canBeGarbageCollected(bitmapWeakReference));
    }

    @Test
    public void testResizeGCed() {
        WeakReference<Bitmap> bitmapWeakReference = new WeakReference<>(mAdapter.getBitmap());
        assertNotNull(bitmapWeakReference.get());
        assertFalse(canBeGarbageCollected(bitmapWeakReference));

        mViewWidth += 10;
        mAdapter.invalidate(null);
        mAdapter.getBitmap();
        assertTrue(canBeGarbageCollected(bitmapWeakReference));
    }

    @Test
    public void testGetDirtyRect() {
        mAdapter.getBitmap();
        Rect rect = mAdapter.getDirtyRect();
        assertTrue(rect.isEmpty());

        mAdapter.invalidate(null);
        rect = mAdapter.getDirtyRect();
        assertEquals(mViewWidth, rect.width());
        assertEquals(mViewHeight, rect.height());
    }

    @Test
    public void testGetDirtyRectDownsampled() {
        mAdapter.setDownsamplingScale(0.5f);

        mAdapter.getBitmap();
        Rect rect = mAdapter.getDirtyRect();
        assertTrue(rect.isEmpty());

        mAdapter.invalidate(null);
        rect = mAdapter.getDirtyRect();
        assertEquals(mViewWidth, rect.width());
        assertEquals(mViewHeight, rect.height());
    }
}
