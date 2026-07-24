// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.recyclerview.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

/** Unit tests for {@link ItemTouchHelper2}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class ItemTouchHelper2UnitTest {

    private Context mContext;
    private ItemTouchHelper2 mItemTouchHelper;
    private ItemTouchHelper2.Callback mCallback;
    private RecyclerView mRecyclerView;
    private TestViewHolder mViewHolder;
    private View mItemView;

    private static class TestViewHolder extends RecyclerView.ViewHolder {
        public TestViewHolder(View itemView) {
            super(itemView);
        }
    }

    private static class TestCallback extends ItemTouchHelper2.Callback {
        @Override
        public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
            return makeMovementFlags(0, 0);
        }

        @Override
        public boolean onMove(
                RecyclerView recyclerView,
                RecyclerView.ViewHolder viewHolder,
                RecyclerView.ViewHolder target) {
            return false;
        }

        @Override
        public void onSwiped(RecyclerView.ViewHolder viewHolder, int direction) {}
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mCallback = new TestCallback();
        mItemTouchHelper = new ItemTouchHelper2(mCallback);
        mRecyclerView = new RecyclerView(mContext);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mContext));
        mItemTouchHelper.attachToRecyclerView(mRecyclerView);

        mItemView = new View(mContext);
        mItemView.setLayoutParams(
                new RecyclerView.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mViewHolder = new TestViewHolder(mItemView);
    }

    @Test
    public void testExternalDrag_Lifecycle() {
        // Set the external drag item.
        mItemTouchHelper.setExternalDragItem(mViewHolder);
        assertFalse("Item should not be recyclable while dragging.", mViewHolder.isRecyclable());

        // Start external drag.
        mItemTouchHelper.onExternalDragStart(10.0f, 20.0f, /* hideItemWhileDragging= */ false);

        // Stop external drag.
        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ true);

        assertNull(mItemTouchHelper.mSelected);
        assertTrue("Item should be recyclable after drag stops.", mViewHolder.isRecyclable());
    }

    @Test
    public void testExternalDrag_VisibilityModifications() {
        // Setup original layout params with specific height.
        RecyclerView.LayoutParams layoutParams = new RecyclerView.LayoutParams(100, 200);
        layoutParams.topMargin = 10;
        layoutParams.bottomMargin = 20;
        mItemView.setLayoutParams(layoutParams);

        // Set the external drag item.
        mItemTouchHelper.setExternalDragItem(mViewHolder);

        // Clear visibility.
        mItemTouchHelper.clearExternalDragItemVisibility();

        assertEquals("Item should be hidden.", View.GONE, mItemView.getVisibility());
        RecyclerView.LayoutParams newParams =
                (RecyclerView.LayoutParams) mItemView.getLayoutParams();
        assertEquals(0, newParams.width);
        assertEquals(0, newParams.height);
        assertEquals(0, newParams.topMargin);
        assertEquals(0, newParams.bottomMargin);

        // Restore visibility.
        mItemTouchHelper.restoreExternalDragItemVisibility(/* isOSNewWindowDrop= */ false);

        assertEquals("Item should be visible.", View.VISIBLE, mItemView.getVisibility());
        RecyclerView.LayoutParams restoredParams =
                (RecyclerView.LayoutParams) mItemView.getLayoutParams();
        assertEquals(100, restoredParams.width);
        assertEquals(200, restoredParams.height);
        assertEquals(10, restoredParams.topMargin);
        assertEquals(20, restoredParams.bottomMargin);
    }

    @Test
    public void testExternalDrag_HideItemWhileDragging() {
        mItemView.setAlpha(1.0f);
        mItemTouchHelper.setExternalDragItem(mViewHolder);

        // Start with hideItemWhileDragging = true.
        mItemTouchHelper.onExternalDragStart(10.0f, 20.0f, /* hideItemWhileDragging= */ true);
        assertEquals("Item should be hidden via alpha.", 0f, mItemView.getAlpha(), 0.0f);

        // Stop and recover.
        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ true);
        assertEquals("Item alpha should be restored.", 1.0f, mItemView.getAlpha(), 0.0f);
    }

    @Test
    public void testExternalDrag_DoNotRecoverItem() {
        mItemView.setAlpha(1.0f);
        mItemTouchHelper.setExternalDragItem(mViewHolder);

        // Start with hideItemWhileDragging = true.
        mItemTouchHelper.onExternalDragStart(10.0f, 20.0f, /* hideItemWhileDragging= */ true);
        assertEquals(0f, mItemView.getAlpha(), 0.0f);

        // Stop but do NOT recover.
        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ false);
        assertEquals(
                "Item alpha should NOT be restored when recoverItem is false.",
                0f,
                mItemView.getAlpha(),
                0.0f);
    }

    @Test
    public void testSetExternalDragItem_RestoresRecyclability() {
        mItemTouchHelper.setExternalDragItem(mViewHolder);
        assertFalse(mViewHolder.isRecyclable());

        // Replace with null, should restore previous item's recyclability.
        mItemTouchHelper.setExternalDragItem(null);
        assertTrue("Item should be recyclable again.", mViewHolder.isRecyclable());
    }

    @Test
    public void testExternalDrag_Visibility_MultipleCalls() {
        RecyclerView.LayoutParams layoutParams = new RecyclerView.LayoutParams(100, 200);
        mItemView.setLayoutParams(layoutParams);

        mItemTouchHelper.setExternalDragItem(mViewHolder);

        // Call clear multiple times.
        mItemTouchHelper.clearExternalDragItemVisibility();

        // Mutate params slightly to ensure they don't get saved over the original state.
        RecyclerView.LayoutParams newParams =
                (RecyclerView.LayoutParams) mItemView.getLayoutParams();
        newParams.width = 50;

        mItemTouchHelper.clearExternalDragItemVisibility();

        mItemTouchHelper.restoreExternalDragItemVisibility(/* isOSNewWindowDrop= */ false);

        RecyclerView.LayoutParams restoredParams =
                (RecyclerView.LayoutParams) mItemView.getLayoutParams();
        assertEquals("Original width should be restored.", 100, restoredParams.width);
    }

    @Test
    public void testExternalDrag_ClearRestoreClearRestore() {
        RecyclerView.LayoutParams layoutParams = new RecyclerView.LayoutParams(100, 200);
        mItemView.setLayoutParams(layoutParams);
        mItemTouchHelper.setExternalDragItem(mViewHolder);
        mItemTouchHelper.clearExternalDragItemVisibility();
        mItemTouchHelper.restoreExternalDragItemVisibility(/* isOSNewWindowDrop= */ false);
        mItemTouchHelper.clearExternalDragItemVisibility();
        mItemTouchHelper.restoreExternalDragItemVisibility(/* isOSNewWindowDrop= */ false);
        assertEquals(100, mItemView.getLayoutParams().width);
    }

    @Test
    public void testExternalDrag_OSNewWindowDrop_Detached_RestoresCleanState() {
        RecyclerView.LayoutParams layoutParams = new RecyclerView.LayoutParams(100, 200);
        mItemView.setLayoutParams(layoutParams);

        mItemTouchHelper.setExternalDragItem(mViewHolder);
        mItemTouchHelper.clearExternalDragItemVisibility();

        // Mutate params physically simulating collapse.
        RecyclerView.LayoutParams newParams =
                (RecyclerView.LayoutParams) mItemView.getLayoutParams();
        assertEquals(0, newParams.width);

        // Call restore with true (OS new window drop).
        mItemTouchHelper.restoreExternalDragItemVisibility(true);

        // It should still be GONE because the restore is delayed.
        assertEquals(
                "Item should still be GONE because restoration is delayed.",
                View.GONE,
                mItemView.getVisibility());

        // Simulate detachment (item successfully removed from adapter).
        if (mItemTouchHelper.mDelayedExternalItemRestorationRunnable != null) {
            mItemTouchHelper.mDelayedExternalItemRestorationRunnable.run();
        }

        // Visibility and dimensions should be instantly restored for the recycle pool.
        assertEquals(
                "Item should be VISIBLE instantly on detach.",
                View.VISIBLE,
                mItemView.getVisibility());
        RecyclerView.LayoutParams restoredParams =
                (RecyclerView.LayoutParams) mItemView.getLayoutParams();
        assertEquals("Original width should be restored.", 100, restoredParams.width);
    }
}
