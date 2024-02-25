// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the {@link org.chromium.ui.widget.ViewLookupCachingFrameLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ViewLookupCachingFrameLayoutTest {
    private static final int VIEW1_ID = 10;
    private static final int VIEW2_ID = 20;

    private ViewLookupCachingFrameLayout mCachingLayout;
    private View mView1;
    private View mViewWithSameIdAs1;
    private View mView2;
    private ViewGroup mGroup;

    @Before
    public void setUp() {
        Context context = RuntimeEnvironment.systemContext;
        mCachingLayout = new ViewLookupCachingFrameLayout(context, null);

        mView1 = new View(context);
        mView1.setId(VIEW1_ID);

        mViewWithSameIdAs1 = new View(context);
        mViewWithSameIdAs1.setId(VIEW1_ID);

        mView2 = new View(context);
        mView2.setId(VIEW2_ID);

        mGroup = new FrameLayout(context);

        assertEquals("Cache should be empty.", 0, mCachingLayout.getCache().size());
    }

    @Test
    public void testAddViewAndLookup() {
        mCachingLayout.addView(mView1);

        assertEquals(
                "Cache should be empty; no lookups have occurred.",
                0,
                mCachingLayout.getCache().size());

        assertEquals(
                "Lookup found the wrong view.", mView1, mCachingLayout.fastFindViewById(VIEW1_ID));

        assertEquals(
                "The cache should contain the view.",
                mView1,
                mCachingLayout.getCache().get(VIEW1_ID).get());
    }

    @Test
    public void testAddNestedViewAddSameId() {
        mCachingLayout.addView(mGroup);
        mGroup.addView(mView1);

        assertEquals(
                "View lookup methods should agree.",
                mCachingLayout.findViewById(VIEW1_ID),
                mCachingLayout.fastFindViewById(VIEW1_ID));
        assertEquals(
                "The cache should contain the first view.",
                mView1,
                mCachingLayout.getCache().get(VIEW1_ID).get());

        // Add the second view earlier in the hierarchy than the original.
        mGroup.addView(mViewWithSameIdAs1, 0);

        assertEquals("Cache should be empty.", 0, mCachingLayout.getCache().size());

        assertEquals(
                "View lookup methods should agree.",
                mCachingLayout.findViewById(VIEW1_ID),
                mCachingLayout.fastFindViewById(VIEW1_ID));
        assertEquals(
                "The cache should contain the view that was added second.",
                mViewWithSameIdAs1,
                mCachingLayout.getCache().get(VIEW1_ID).get());
    }

    @Test
    public void testAddNestedViewRemove() {
        mCachingLayout.addView(mGroup);
        mGroup.addView(mView1);

        assertEquals(
                "Lookup found the wrong view.", mView1, mCachingLayout.fastFindViewById(VIEW1_ID));

        assertEquals(
                "The cache should contain the view.",
                mView1,
                mCachingLayout.getCache().get(VIEW1_ID).get());

        mGroup.removeView(mView1);

        assertEquals("Cache should be empty.", 0, mCachingLayout.getCache().size());
        assertEquals(
                "The view should not longer be in the hierarchy.",
                null,
                mCachingLayout.fastFindViewById(VIEW1_ID));
    }

    @Test
    public void testAddItemWithSameId() {
        mCachingLayout.addView(mView1);

        assertEquals(
                "Lookup found the wrong view.", mView1, mCachingLayout.fastFindViewById(VIEW1_ID));

        assertEquals(
                "The cache should contain the view.",
                mView1,
                mCachingLayout.getCache().get(VIEW1_ID).get());

        mCachingLayout.addView(mViewWithSameIdAs1);

        assertEquals("Cache should be empty.", 0, mCachingLayout.getCache().size());
    }

    @Test
    public void testAddNestedItemWithSameId() {
        mCachingLayout.addView(mGroup);
        mGroup.addView(mView1);

        assertEquals(
                "Lookup found the wrong view.", mView1, mCachingLayout.fastFindViewById(VIEW1_ID));

        assertEquals(
                "The cache should contain the view.",
                mView1,
                mCachingLayout.getCache().get(VIEW1_ID).get());

        mGroup.addView(mViewWithSameIdAs1);

        assertEquals("Cache should be empty.", 0, mCachingLayout.getCache().size());
    }

    @Test
    public void testAddItemWithDifferentId() {
        mCachingLayout.addView(mView1);
        mCachingLayout.addView(mView2);

        assertEquals(
                "Lookup found the wrong view.", mView1, mCachingLayout.fastFindViewById(VIEW1_ID));
        assertEquals(
                "Lookup found the wrong view.", mView2, mCachingLayout.fastFindViewById(VIEW2_ID));

        assertEquals(
                "The cache should contain the first view.",
                mView1,
                mCachingLayout.getCache().get(VIEW1_ID).get());
        assertEquals(
                "The cache should contain the second view.",
                mView2,
                mCachingLayout.getCache().get(VIEW2_ID).get());
    }

    @Test
    public void testRemoveItem() {
        mCachingLayout.addView(mView1);

        assertEquals(
                "Lookup found the wrong view.", mView1, mCachingLayout.fastFindViewById(VIEW1_ID));

        assertEquals(
                "The cache should contain the first view.",
                mView1,
                mCachingLayout.getCache().get(VIEW1_ID).get());

        mCachingLayout.removeView(mView1);

        assertEquals("Cache should be empty.", 0, mCachingLayout.getCache().size());
        assertEquals(
                "The view should not longer be in the hierarchy.",
                null,
                mCachingLayout.fastFindViewById(VIEW1_ID));
    }
}
