// Copyright 2019 The Chromium Authors
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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.ResourceFactory;
import org.chromium.ui.resources.ResourceFactoryJni;

import java.lang.ref.WeakReference;

/** Tests for {@link ViewResourceAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ViewResourceAdapterTest.ShadowCaptureUtils.class})
public class ViewResourceAdapterTest {
    /**
     * Mock this out to avoid calling {@link View#draw(Canvas)} on the mocked mView.
     * Otherwise the GC-related tests would fail because Mockito holds onto a references to the
     * bitmap forever.
     */
    @Implements(CaptureUtils.class)
    static class ShadowCaptureUtils {
        @Implementation
        public static boolean captureCommon(
                Canvas canvas,
                View view,
                Rect dirtyRect,
                float scale,
                boolean drawWhileDetached,
                CaptureObserver observer) {
            return true;
        }
    }

    private int mViewWidth;
    private int mViewHeight;

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private ResourceFactory.Natives mResourceFactoryJni;
    @Mock private View mView;

    private ViewResourceAdapter mAdapter;

    @Before
    public void setup() {
        initMocks(this);
        mJniMocker.mock(ResourceFactoryJni.TEST_HOOKS, mResourceFactoryJni);

        mViewWidth = 200;
        mViewHeight = 100;

        when(mView.getWidth()).thenAnswer((invocation) -> mViewWidth);
        when(mView.getHeight()).thenAnswer((invocation) -> mViewHeight);

        mAdapter = new ViewResourceAdapter(mView);
    }

    private Rect getBitmapSize() {
        // Need to mark dirty before requesting, otherwise it will no-op.
        mAdapter.invalidate(null);
        return DynamicResourceTestUtils.getBitmapSizeSync(mAdapter);
    }

    private Bitmap getBitmap() {
        // Need to mark dirty before requesting, otherwise it will no-op.
        mAdapter.invalidate(null);
        return DynamicResourceTestUtils.getBitmapSync(mAdapter);
    }

    @Test
    public void testGetBitmap() {
        Bitmap bitmap = getBitmap();
        assertNotNull(bitmap);
        assertEquals(mViewWidth, bitmap.getWidth());
        assertEquals(mViewHeight, bitmap.getHeight());
    }

    @Test
    public void testGetBitmapSize() {
        Bitmap bitmap = getBitmap();
        Rect rect = getBitmapSize();

        assertEquals(bitmap.getWidth(), rect.width());
        assertEquals(bitmap.getHeight(), rect.height());
    }

    @Test
    public void testSetDownsamplingSize() {
        float scale = 0.5f;
        mAdapter.setDownsamplingScale(scale);
        Bitmap bitmap = getBitmap();
        assertEquals(mViewWidth * scale, bitmap.getWidth(), 1);
        assertEquals(mViewHeight * scale, bitmap.getHeight(), 1);

        Rect rect = getBitmapSize();
        assertEquals(mViewWidth, rect.width());
        assertEquals(mViewHeight, rect.height());
    }

    @Test
    public void testIsDirty() {
        assertTrue(mAdapter.isDirty());

        getBitmap();
        assertFalse(mAdapter.isDirty());
    }

    @Test
    public void testOnLayoutChange() {
        getBitmap();
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

        getBitmap();
        assertFalse(mAdapter.isDirty());

        mAdapter.onLayoutChange(mView, 0, 0, 1, 2, 0, 0, mViewWidth, mViewHeight);
        assertTrue(mAdapter.isDirty());

        Rect dirtyRect = mAdapter.getDirtyRect();
        assertEquals(1, dirtyRect.width());
        assertEquals(2, dirtyRect.height());
    }

    @Test
    public void testInvalidate() {
        getBitmap();
        assertFalse(mAdapter.isDirty());

        mAdapter.invalidate(null);
        assertTrue(mAdapter.isDirty());

        Rect dirtyRect = mAdapter.getDirtyRect();
        assertEquals(mViewWidth, dirtyRect.width());
        assertEquals(mViewHeight, dirtyRect.height());
    }

    @Test
    public void testInvalidateRect() {
        getBitmap();
        assertFalse(mAdapter.isDirty());

        Rect dirtyRect = new Rect(1, 2, 3, 4);
        mAdapter.invalidate(dirtyRect);
        assertTrue(mAdapter.isDirty());
        assertEquals(dirtyRect.toString(), mAdapter.getDirtyRect().toString());
    }

    @Test
    public void testInvalidateRectDownsampled() {
        mAdapter.setDownsamplingScale(0.5f);

        getBitmap();
        assertFalse(mAdapter.isDirty());

        Rect dirtyRect = new Rect(1, 2, 3, 4);
        mAdapter.invalidate(dirtyRect);
        assertTrue(mAdapter.isDirty());
        assertEquals(dirtyRect.toString(), mAdapter.getDirtyRect().toString());
    }

    @Test
    public void testInvalidateRectUnion() {
        getBitmap();
        assertFalse(mAdapter.isDirty());

        mAdapter.invalidate(new Rect(1, 2, 3, 4));
        mAdapter.invalidate(new Rect(5, 6, 7, 8));
        assertTrue(mAdapter.isDirty());
        Rect expected = new Rect(1, 2, 7, 8);
        assertEquals(expected.toString(), mAdapter.getDirtyRect().toString());
    }

    @Test
    public void testGetBitmapResized() {
        Bitmap bitmap = getBitmap();
        assertNotNull(bitmap);
        assertEquals(mViewWidth, bitmap.getWidth());
        assertEquals(mViewHeight, bitmap.getHeight());

        mViewWidth = 10;
        mViewHeight = 20;
        mAdapter.invalidate(null);
        Bitmap bitmap2 = getBitmap();
        assertNotNull(bitmap2);
        assertEquals(mViewWidth, bitmap2.getWidth());
        assertEquals(mViewHeight, bitmap2.getHeight());
        assertNotEquals(bitmap, bitmap2);
    }

    @Test
    public void testBitmapReused() {
        Bitmap bitmap = getBitmap();
        assertNotNull(bitmap);

        mAdapter.invalidate(null);
        assertTrue(mAdapter.isDirty());
        assertEquals(bitmap, getBitmap());
    }

    @Test
    public void testDropCachedBitmap() {
        Bitmap bitmap = getBitmap();
        assertNotNull(bitmap);

        mAdapter.invalidate(null);
        assertTrue(mAdapter.isDirty());
        assertEquals(bitmap, getBitmap());

        mAdapter.dropCachedBitmap();
        mAdapter.invalidate(null);
        assertTrue(mAdapter.isDirty());
        assertNotEquals(bitmap, getBitmap());
    }

    @Test
    public void testDropCachedBitmapNotDirty() {
        getBitmap();
        mAdapter.dropCachedBitmap();
        assertFalse(mAdapter.isDirty());
    }

    @Test
    public void testDropCachedBitmapGCed() {
        WeakReference<Bitmap> bitmapWeakReference = new WeakReference<>(getBitmap());
        assertNotNull(bitmapWeakReference.get());
        assertFalse(canBeGarbageCollected(bitmapWeakReference));

        mAdapter.dropCachedBitmap();
        assertTrue(canBeGarbageCollected(bitmapWeakReference));
    }

    @Test
    public void testResizeGCed() {
        WeakReference<Bitmap> bitmapWeakReference = new WeakReference<>(getBitmap());
        assertNotNull(bitmapWeakReference.get());
        assertFalse(canBeGarbageCollected(bitmapWeakReference));

        mViewWidth += 10;
        mAdapter.invalidate(null);
        getBitmap();
        assertTrue(canBeGarbageCollected(bitmapWeakReference));
    }

    @Test
    public void testGetDirtyRect() {
        getBitmap();
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

        getBitmap();
        Rect rect = mAdapter.getDirtyRect();
        assertTrue(rect.isEmpty());

        mAdapter.invalidate(null);
        rect = mAdapter.getDirtyRect();
        assertEquals(mViewWidth, rect.width());
        assertEquals(mViewHeight, rect.height());
    }

    @Test
    public void testManuallyTriggerCapture() throws Exception {
        Bitmap bitmap = getBitmap();

        Bitmap[] bitmapHolder = new Bitmap[1];
        Callback<Resource> callback =
                (resource) -> {
                    bitmapHolder[0] = resource.getBitmap();
                };
        mAdapter.addOnResourceReadyCallback(callback);

        CallbackHelper helper = new CallbackHelper();
        DynamicResourceReadyOnceCallback.onNext(mAdapter, (r) -> helper.notifyCalled());

        mAdapter.triggerBitmapCapture();

        helper.waitForOnly("Capture never completed.");
        // Bitmap is re-used.
        assertEquals(bitmap, bitmapHolder[0]);

        mAdapter.triggerBitmapCapture();
        // Assert that no further captures occur.
        assertEquals(1, helper.getCallCount());

        // Clear this out in case another case depends on it being unset.
        mAdapter.removeOnResourceReadyCallback(callback);
    }
}
