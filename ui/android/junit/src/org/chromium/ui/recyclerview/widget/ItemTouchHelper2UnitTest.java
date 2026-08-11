// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.recyclerview.widget;

import static androidx.recyclerview.widget.ItemTouchHelper.ACTION_STATE_DRAG;
import static androidx.recyclerview.widget.ItemTouchHelper.DOWN;
import static androidx.recyclerview.widget.ItemTouchHelper.LEFT;
import static androidx.recyclerview.widget.ItemTouchHelper.RIGHT;
import static androidx.recyclerview.widget.ItemTouchHelper.UP;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.MotionEvent;
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

/** Unit tests for {@link ItemTouchHelper2}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
        private int mMovementFlags = makeMovementFlags(0, 0);
        private RecyclerView.ViewHolder mLiveViewHolder;

        private RecyclerView.ViewHolder mReboundOldHolder;
        private RecyclerView.ViewHolder mReboundNewHolder;

        public void setMovementFlags(int flags) {
            mMovementFlags = flags;
        }

        public void setLiveViewHolder(RecyclerView.ViewHolder liveViewHolder) {
            mLiveViewHolder = liveViewHolder;
        }

        @Override
        public void onExternalDragItemRebound(
                RecyclerView.ViewHolder oldHolder, RecyclerView.ViewHolder newHolder) {
            mReboundOldHolder = oldHolder;
            mReboundNewHolder = newHolder;
        }

        @Override
        public boolean shouldAllowDragPastLayout() {
            return true;
        }

        @Override
        public RecyclerView.ViewHolder findLiveViewHolder(
                RecyclerView recyclerView, RecyclerView.ViewHolder current) {
            return mLiveViewHolder;
        }

        @Override
        public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
            return mMovementFlags;
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
        assertEquals(
                "mSelected should be set to mViewHolder.", mViewHolder, mItemTouchHelper.mSelected);

        // Stop external drag.
        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ true);

        assertNull(mItemTouchHelper.mSelected);
        assertTrue("Item should be recyclable after drag stops.", mViewHolder.isRecyclable());
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
    public void testExternalDrag_TouchOffsetInitialization_FloatNaNAndEdgeAndCenterFallback() {
        assertTrue(Float.isNaN(mItemTouchHelper.mTouchOffsetWithinItemX));
        assertTrue(Float.isNaN(mItemTouchHelper.mTouchOffsetWithinItemY));

        // Position item view at (100, 200, 300, 400) -> width=200, height=200
        mItemView.layout(100, 200, 300, 400);
        mItemTouchHelper.setExternalDragItem(mViewHolder);

        // 1. Edge touch: (x=100f, y=200f) exactly at top-left corner -> offset must be 0.0f
        mItemTouchHelper.onExternalDragStart(100.0f, 200.0f, /* hideItemWhileDragging= */ false);
        assertEquals(
                "Offset X for edge touch should be 0.0f.",
                0.0f,
                mItemTouchHelper.mTouchOffsetWithinItemX,
                0.0f);
        assertEquals(
                "Offset Y for edge touch should be 0.0f.",
                0.0f,
                mItemTouchHelper.mTouchOffsetWithinItemY,
                0.0f);

        // Stop drag -> offsets must reset to Float.NaN
        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ false);
        assertTrue(Float.isNaN(mItemTouchHelper.mTouchOffsetWithinItemX));
        assertTrue(Float.isNaN(mItemTouchHelper.mTouchOffsetWithinItemY));

        // 2. Out-of-bounds touch: (x=50f, y=500f) -> must fall back to center (width/2=100f,
        // height/2=100f)
        mItemTouchHelper.setExternalDragItem(mViewHolder);
        mItemTouchHelper.onExternalDragStart(50.0f, 500.0f, /* hideItemWhileDragging= */ false);
        assertEquals(
                "Offset X out-of-bounds should fall back to center.",
                100.0f,
                mItemTouchHelper.mTouchOffsetWithinItemX,
                0.0f);
        assertEquals(
                "Offset Y out-of-bounds should fall back to center.",
                100.0f,
                mItemTouchHelper.mTouchOffsetWithinItemY,
                0.0f);

        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ false);
        assertTrue(Float.isNaN(mItemTouchHelper.mTouchOffsetWithinItemX));
        assertTrue(Float.isNaN(mItemTouchHelper.mTouchOffsetWithinItemY));
    }

    @Test
    public void testExternalDrag_DisplacementCalculations_VerticalAndAllDirections() {
        mItemView.layout(100, 200, 300, 400); // left=100, top=200, width=200, height=200
        mItemTouchHelper.setExternalDragItem(mViewHolder);

        // Configure vertical-only movement flags
        ((TestCallback) mCallback)
                .setMovementFlags(ItemTouchHelper2.Callback.makeMovementFlags(UP | DOWN, 0));

        // Start drag at (150f, 260f) -> touchOffsetX = 50f, touchOffsetY = 60f
        mItemTouchHelper.onExternalDragStart(150.0f, 260.0f, /* hideItemWhileDragging= */ false);
        assertEquals(50.0f, mItemTouchHelper.mTouchOffsetWithinItemX, 0.0f);
        assertEquals(60.0f, mItemTouchHelper.mTouchOffsetWithinItemY, 0.0f);

        // Move to (180f, 340f)
        mItemTouchHelper.onExternalDragLocation(180.0f, 340.0f);
        // Vertical-only: mDx masked to 0, mDy = (340 - 60) - 200 = 80
        assertEquals("mDx should be 0 for vertical-only drag.", 0.0f, mItemTouchHelper.mDx, 0.0f);
        assertEquals("mDy should follow vertical displacement.", 80.0f, mItemTouchHelper.mDy, 0.0f);

        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ false);

        // Configure all-directions movement flags
        ((TestCallback) mCallback)
                .setMovementFlags(
                        ItemTouchHelper2.Callback.makeMovementFlags(UP | DOWN | LEFT | RIGHT, 0));
        mItemTouchHelper.setExternalDragItem(mViewHolder);
        mItemTouchHelper.onExternalDragStart(150.0f, 260.0f, /* hideItemWhileDragging= */ false);

        // Move to (180f, 340f)
        mItemTouchHelper.onExternalDragLocation(180.0f, 340.0f);
        // All-directions: mDx = (180 - 50) - 100 = 30, mDy = (340 - 60) - 200 = 80
        assertEquals(
                "mDx should follow horizontal displacement.", 30.0f, mItemTouchHelper.mDx, 0.0f);
        assertEquals("mDy should follow vertical displacement.", 80.0f, mItemTouchHelper.mDy, 0.0f);

        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ false);
    }

    @Test
    public void testExternalDrag_ActionCancelAndUp_PreservesSelectionWhenExternalDragItemSet() {
        ((TestCallback) mCallback)
                .setMovementFlags(ItemTouchHelper2.Callback.makeMovementFlags(UP | DOWN, 0));
        mItemTouchHelper.select(mViewHolder, ACTION_STATE_DRAG);
        assertEquals(mViewHolder, mItemTouchHelper.mSelected);

        // When mExternalDragItem == null and mExternalDragInProgress == false, ACTION_CANCEL clears
        // selection.
        long downTime = 0L;
        long eventTime = 0L;
        MotionEvent cancelEvent =
                MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_CANCEL, 0f, 0f, 0);
        mItemTouchHelper.mOnItemTouchListener.onInterceptTouchEvent(mRecyclerView, cancelEvent);
        assertNull(
                "Selection should be cleared on ACTION_CANCEL during regular drag.",
                mItemTouchHelper.mSelected);
        cancelEvent.recycle();

        // When mExternalDragItem != null, ACTION_CANCEL preserves selection.
        mItemTouchHelper.setExternalDragItem(mViewHolder);
        mItemTouchHelper.select(mViewHolder, ACTION_STATE_DRAG);
        assertEquals(mViewHolder, mItemTouchHelper.mSelected);

        MotionEvent cancelEvent2 =
                MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_CANCEL, 0f, 0f, 0);
        mItemTouchHelper.mOnItemTouchListener.onInterceptTouchEvent(mRecyclerView, cancelEvent2);
        assertEquals(
                "Selection should be preserved on ACTION_CANCEL when external drag item is set.",
                mViewHolder,
                mItemTouchHelper.mSelected);
        cancelEvent2.recycle();

        // ACTION_UP on onTouchEvent also preserves selection when mExternalDragItem != null.
        MotionEvent upEvent =
                MotionEvent.obtain(downTime, eventTime, MotionEvent.ACTION_UP, 0f, 0f, 0);
        mItemTouchHelper.mOnItemTouchListener.onTouchEvent(mRecyclerView, upEvent);
        assertEquals(
                "Selection should be preserved on ACTION_UP when external drag item is set.",
                mViewHolder,
                mItemTouchHelper.mSelected);
        upEvent.recycle();

        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ false);
    }

    @Test
    public void testExternalDrag_RebindsToLiveViewHolderWhenDetached() {
        mItemTouchHelper.setExternalDragItem(mViewHolder);
        mItemTouchHelper.onExternalDragStart(10.0f, 20.0f, /* hideItemWhileDragging= */ false);

        // Simulate mViewHolder getting detached (parent == null and position == NO_POSITION)
        // Create a new ViewHolder that represents the rebound item in the RecyclerView
        View newItemView = new View(mContext);
        newItemView.setLayoutParams(
                new RecyclerView.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        TestViewHolder newViewHolder = new TestViewHolder(newItemView);
        mRecyclerView.addView(newItemView);

        ((TestCallback) mCallback).setLiveViewHolder(newViewHolder);

        // Location update should discover the new live view holder, replace mExternalDragItem &
        // mSelected
        mItemTouchHelper.onExternalDragLocation(15.0f, 25.0f);
        assertEquals(
                "mExternalDragItem should update to live view holder.",
                newViewHolder,
                mItemTouchHelper.mExternalDragItem);
        assertEquals(
                "mSelected should update to live view holder.",
                newViewHolder,
                mItemTouchHelper.mSelected);
        assertTrue("Old ViewHolder should be marked recyclable.", mViewHolder.isRecyclable());
        assertFalse(
                "New ViewHolder should be marked non-recyclable.", newViewHolder.isRecyclable());
        assertEquals(
                "Callback should be notified of rebound old holder.",
                mViewHolder,
                ((TestCallback) mCallback).mReboundOldHolder);
        assertEquals(
                "Callback should be notified of rebound new holder.",
                newViewHolder,
                ((TestCallback) mCallback).mReboundNewHolder);

        mItemTouchHelper.onExternalDragStop(/* recoverItem= */ false);
    }
}
