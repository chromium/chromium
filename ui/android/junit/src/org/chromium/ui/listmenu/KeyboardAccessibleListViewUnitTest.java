// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.accessibility.AccessibilityStateTestHelper;
import org.chromium.ui.base.TestActivity;

import java.util.List;

/** Unit tests for {@link KeyboardAccessibleListView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class KeyboardAccessibleListViewUnitTest {

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private KeyboardAccessibleListView mListView;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mListView = new KeyboardAccessibleListView(mActivity);
    }

    @Test
    public void testOnKeyDown_emptyList_doesNotCrash() {
        assertNull("Focused child should be null", mListView.getFocusedChild());
        assertEquals("Count should be 0", 0, mListView.getCount());

        // DOWN / TAB navigation on empty list should not throw NullPointerException.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_DOWN,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN)));
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));

        // UP / Shift+TAB navigation on empty list should not throw NullPointerException.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_UP,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP)));
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_SHIFT_ON)));
    }

    @Test
    public void testOnKeyDown_noFocusedChild_doesNotCrash() {
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 1", "Item 2", "Item 3"));
        mListView.setAdapter(adapter);
        assertNull("Focused child should be null initially", mListView.getFocusedChild());

        // Pressing UP when selection is at default (-1) should not throw NullPointerException.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_UP,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP)));

        // Pressing Shift+TAB when selection is at default (-1) should not throw
        // NullPointerException.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_SHIFT_ON)));
    }

    @Test
    public void testOnKeyDown_focusSearchFallbackToListView() {
        LinearLayout layout = new LinearLayout(mActivity);
        layout.setOrientation(LinearLayout.VERTICAL);

        View topView = new View(mActivity);
        topView.setFocusable(true);
        layout.addView(topView, new LinearLayout.LayoutParams(100, 100));

        layout.addView(mListView, new LinearLayout.LayoutParams(100, 100));

        View bottomView = new View(mActivity);
        bottomView.setFocusable(true);
        layout.addView(bottomView, new LinearLayout.LayoutParams(100, 100));

        mActivity.setContentView(layout);

        layout.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY));
        layout.layout(0, 0, 100, 300);

        // On empty list, TAB/DOWN should fallback to list view focusSearch and focus bottomView.
        mListView.onKeyDown(
                KeyEvent.KEYCODE_TAB, new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB));
        assertTrue("Expected bottomView to have focus", bottomView.hasFocus());

        // On empty list, Shift+TAB/UP should fallback to list view focusSearch and focus topView.
        mListView.onKeyDown(
                KeyEvent.KEYCODE_DPAD_UP,
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP));
        assertTrue("Expected topView to have focus", topView.hasFocus());
    }

    @Test
    public void testOnKeyDown_dpadDown_initialUnset_selectsFirstVisibleItem() {
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 0", "Item 1", "Item 2"));
        mListView.setAdapter(adapter);
        assertEquals(ListView.INVALID_POSITION, mListView.getSelectedItemPosition());

        // First DPAD_DOWN should select the first item (position 0).
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_DOWN,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN)));
        assertEquals(0, mListView.getSelectedItemPosition());

        // Subsequent DPAD_DOWN should advance to position 1.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_DOWN,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN)));
        assertEquals(1, mListView.getSelectedItemPosition());
    }

    @Test
    public void testOnKeyDown_tab_initialUnset_selectsFirstVisibleItem() {
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 0", "Item 1", "Item 2"));
        mListView.setAdapter(adapter);
        assertEquals(ListView.INVALID_POSITION, mListView.getSelectedItemPosition());

        // First TAB should select the first item (position 0).
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));
        assertEquals(0, mListView.getSelectedItemPosition());

        // Subsequent TAB should advance to position 1.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));
        assertEquals(1, mListView.getSelectedItemPosition());
    }

    @Test
    public void testOnKeyDown_dpadUp_scrolledDown_movesUp() {
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 0", "Item 1", "Item 2", "Item 3", "Item 4"));
        mListView.setAdapter(adapter);
        mListView.setSelection(3);
        assertEquals(3, mListView.getSelectedItemPosition());

        // DPAD_UP should move from position 3 to position 2.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_UP,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP)));
        assertEquals(2, mListView.getSelectedItemPosition());

        // Another DPAD_UP should move to position 1.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_UP,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP)));
        assertEquals(1, mListView.getSelectedItemPosition());
    }

    @Test
    public void testOnKeyDown_shiftTab_scrolledDown_movesUp() {
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 0", "Item 1", "Item 2", "Item 3", "Item 4"));
        mListView.setAdapter(adapter);
        mListView.setSelection(3);
        assertEquals(3, mListView.getSelectedItemPosition());

        // Shift+TAB should move from position 3 to position 2.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_SHIFT_ON)));
        assertEquals(2, mListView.getSelectedItemPosition());
    }

    @Test
    public void testOnKeyDown_dpadDown_atBottomBoundary_triggersFocusSearch() {
        LinearLayout layout = new LinearLayout(mActivity);
        layout.setOrientation(LinearLayout.VERTICAL);

        mListView.setAdapter(
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 0", "Item 1")));
        layout.addView(mListView, new LinearLayout.LayoutParams(100, 100));

        View bottomView = new View(mActivity);
        bottomView.setFocusable(true);
        layout.addView(bottomView, new LinearLayout.LayoutParams(100, 100));

        mActivity.setContentView(layout);
        layout.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        layout.layout(0, 0, 100, 200);

        mListView.setSelection(1);
        assertEquals(1, mListView.getSelectedItemPosition());

        // Pressing DPAD_DOWN at the bottom boundary should search for focus outside and focus
        // bottomView.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_DOWN,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN)));
        assertTrue("Expected bottomView to have focus", bottomView.hasFocus());
    }

    @Test
    public void testOnKeyDown_dpadUp_atTopBoundary_triggersFocusSearch() {
        LinearLayout layout = new LinearLayout(mActivity);
        layout.setOrientation(LinearLayout.VERTICAL);

        View topView = new View(mActivity);
        topView.setFocusable(true);
        layout.addView(topView, new LinearLayout.LayoutParams(100, 100));

        mListView.setAdapter(
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 0", "Item 1")));
        layout.addView(mListView, new LinearLayout.LayoutParams(100, 100));

        mActivity.setContentView(layout);
        layout.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        layout.layout(0, 0, 100, 200);

        mListView.setSelection(0);
        assertEquals(0, mListView.getSelectedItemPosition());

        // Pressing DPAD_UP at the top boundary should search for focus outside and focus topView.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_UP,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP)));
        assertTrue("Expected topView to have focus", topView.hasFocus());
    }

    @Test
    public void testSelectedItemPosition_withFocusedChild() {
        LinearLayout layout = new LinearLayout(mActivity);
        layout.setOrientation(LinearLayout.VERTICAL);

        mListView.setItemsCanFocus(true);
        mListView.setAdapter(
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 0", "Item 1", "Item 2")));
        layout.addView(mListView, new LinearLayout.LayoutParams(100, 300));
        mActivity.setContentView(layout);

        layout.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY));
        layout.layout(0, 0, 100, 300);

        View child1 = mListView.getChildAt(1);
        if (child1 != null) {
            child1.setFocusable(true);
            child1.requestFocus();
            assertEquals(1, mListView.getSelectedItemPosition());
        }
    }

    @Test
    public void testOnKeyDown_skipsDisabledItemsAndFocusesChild() {
        LinearLayout layout = new LinearLayout(mActivity);
        layout.setOrientation(LinearLayout.VERTICAL);

        View topView = new View(mActivity);
        topView.setFocusable(true);
        layout.addView(topView, new LinearLayout.LayoutParams(100, 100));

        // Positions 0 ("Header"), 1 ("Divider 1"), and 3 ("Divider 2") are disabled.
        // Positions 2 ("Item 1") and 4 ("Item 2") are enabled.
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Header", "Divider 1", "Item 1", "Divider 2", "Item 2")) {
                    @Override
                    public boolean areAllItemsEnabled() {
                        return false;
                    }

                    @Override
                    public boolean isEnabled(int position) {
                        return position == 2 || position == 4;
                    }

                    @Override
                    public View getView(int position, View convertView, ViewGroup parent) {
                        View view = super.getView(position, convertView, parent);
                        view.setFocusable(isEnabled(position));
                        return view;
                    }
                };
        mListView.setItemsCanFocus(true);
        mListView.setAdapter(adapter);
        layout.addView(mListView, new LinearLayout.LayoutParams(100, 500));

        View bottomView = new View(mActivity);
        bottomView.setFocusable(true);
        layout.addView(bottomView, new LinearLayout.LayoutParams(100, 100));

        mActivity.setContentView(layout);
        layout.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(700, View.MeasureSpec.EXACTLY));
        layout.layout(0, 0, 100, 700);
        mListView.clearFocus();

        // Initial DPAD_DOWN should skip disabled positions 0 and 1, selecting and focusing 2.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_DOWN,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN)));
        assertEquals(2, mListView.getSelectedItemPosition());
        assertTrue(mListView.getChildAt(2).hasFocus());

        // DPAD_DOWN from position 2 should skip disabled position 3, selecting and focusing 4.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_DOWN,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN)));
        assertEquals(4, mListView.getSelectedItemPosition());
        assertTrue(mListView.getChildAt(4).hasFocus());

        // DPAD_UP from position 4 should skip disabled position 3, selecting and focusing 2.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_UP,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP)));
        assertEquals(2, mListView.getSelectedItemPosition());
        assertTrue(mListView.getChildAt(2).hasFocus());

        // DPAD_UP from position 2 (first enabled item) should skip disabled positions 1 and 0 and
        // focus topView outside the list.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_UP,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP)));
        assertTrue("Expected topView to have focus", topView.hasFocus());
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_setsMenuChromeRole() {
        AccessibilityStateTestHelper.setIsKnownScreenReaderEnabledForTesting(true);
        AccessibilityNodeInfo emptyInfo = AccessibilityNodeInfo.obtain();
        mListView.onInitializeAccessibilityNodeInfo(emptyInfo);
        assertNull(emptyInfo.getExtras().getCharSequence("AccessibilityNodeInfo.chromeRole"));

        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity, android.R.layout.simple_list_item_1, List.of("Item 1"));
        mListView.setAdapter(adapter);
        AccessibilityNodeInfo populatedInfo = AccessibilityNodeInfo.obtain();
        mListView.onInitializeAccessibilityNodeInfo(populatedInfo);
        assertEquals(
                "menu",
                populatedInfo.getExtras().getCharSequence("AccessibilityNodeInfo.chromeRole"));

        // When no known screen reader is enabled, the extra should not be set (and should be
        // removed if previously present on a cached AccessibilityNodeInfo).
        AccessibilityStateTestHelper.setIsKnownScreenReaderEnabledForTesting(false);
        mListView.onInitializeAccessibilityNodeInfo(populatedInfo);
        assertNull(populatedInfo.getExtras().getCharSequence("AccessibilityNodeInfo.chromeRole"));
    }

    @Test
    public void testOnKeyDown_tabNavigatesMultipleFocusableViewsWithinRow() {
        // Row 0 has two focusable child views (mainItem and inlineButton), similar to a Share row
        // with an inline quick-share button. Row 1 has a single focusable view.
        LinearLayout row0 = new LinearLayout(mActivity);
        row0.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
        View mainItem0 = new View(mActivity);
        mainItem0.setFocusable(true);
        View inlineButton0 = new View(mActivity);
        inlineButton0.setFocusable(true);
        row0.addView(mainItem0, new LinearLayout.LayoutParams(50, 50));
        row0.addView(inlineButton0, new LinearLayout.LayoutParams(50, 50));

        View row1 = new View(mActivity);
        row1.setFocusable(true);

        List<View> rows = List.of(row0, row1);
        ArrayAdapter<View> adapter =
                new ArrayAdapter<>(mActivity, 0, rows) {
                    @Override
                    public View getView(int position, View convertView, ViewGroup parent) {
                        return rows.get(position);
                    }
                };
        mListView.setItemsCanFocus(true);
        mListView.setAdapter(adapter);
        mActivity.setContentView(mListView);
        mListView.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        mListView.layout(0, 0, 100, 200);
        mListView.clearFocus();

        // First Tab focuses mainItem0 in row 0.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));
        assertEquals(0, mListView.getSelectedItemPosition());
        assertTrue(mainItem0.hasFocus());

        // Second Tab moves to inlineButton0 within row 0 before advancing to row 1.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));
        assertEquals(0, mListView.getSelectedItemPosition());
        assertTrue(inlineButton0.hasFocus());

        // Third Tab advances from inlineButton0 in row 0 to row 1.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB)));
        assertEquals(1, mListView.getSelectedItemPosition());
        assertTrue(row1.hasFocus());

        // Shift+Tab from row 1 enters row 0 backwards, focusing inlineButton0 first.
        KeyEvent shiftTabEvent =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_TAB,
                        0,
                        KeyEvent.META_SHIFT_ON);
        assertTrue(mListView.onKeyDown(KeyEvent.KEYCODE_TAB, shiftTabEvent));
        assertEquals(0, mListView.getSelectedItemPosition());
        assertTrue(inlineButton0.hasFocus());

        // Shift+Tab from inlineButton0 moves back to mainItem0 within row 0.
        assertTrue(mListView.onKeyDown(KeyEvent.KEYCODE_TAB, shiftTabEvent));
        assertEquals(0, mListView.getSelectedItemPosition());
        assertTrue(mainItem0.hasFocus());

        // DPAD_DOWN from mainItem0 steps directly to row 1 (skipping inlineButton0).
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_DOWN,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN)));
        assertEquals(1, mListView.getSelectedItemPosition());
        assertTrue(row1.hasFocus());

        // DPAD_UP from row 1 focuses mainItem0 (the primary item of row 0).
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_DPAD_UP,
                        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_UP)));
        assertEquals(0, mListView.getSelectedItemPosition());
        assertTrue(mainItem0.hasFocus());
    }

    @Test
    public void testOnKeyDown_shiftTabWithOffScreenDisabledHeader_fallsBackToFocusSearch() {
        LinearLayout layout = new LinearLayout(mActivity);
        layout.setOrientation(LinearLayout.VERTICAL);

        View topView = new View(mActivity);
        topView.setFocusable(true);
        layout.addView(topView, new LinearLayout.LayoutParams(100, 40));

        // Position 0 ("Header") and position 2 ("Footer") are disabled; position 1 ("Item 1") is
        // enabled.
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Header", "Item 1", "Footer")) {
                    @Override
                    public boolean areAllItemsEnabled() {
                        return false;
                    }

                    @Override
                    public boolean isEnabled(int position) {
                        return position == 1;
                    }

                    @Override
                    public View getView(int position, View convertView, ViewGroup parent) {
                        View view = super.getView(position, convertView, parent);
                        view.setFocusable(isEnabled(position));
                        return view;
                    }
                };
        mListView.setItemsCanFocus(true);
        mListView.setAdapter(adapter);
        layout.addView(mListView, new LinearLayout.LayoutParams(100, 60));
        mActivity.setContentView(layout);

        // Scroll to position 1 so disabled position 0 is off-screen (firstVisiblePosition = 1).
        mListView.setSelection(1);
        layout.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY));
        layout.layout(0, 0, 100, 100);
        assertEquals(1, mListView.getFirstVisiblePosition());
        // Position position 1's view at top = 2 (>= getArrowScrollPreviewLength()) so
        // ListView#arrowScroll(FOCUS_UP) returns false while firstVisiblePosition is still 1.
        mListView.getChildAt(0).offsetTopAndBottom(2);
        assertTrue(mListView.getChildAt(0).requestFocus());
        // Ensure ListView#onKeyDown was called once so AbsListView.mSelectedPosition is synced.
        mListView.setSelection(1);

        KeyEvent shiftTab =
                new KeyEvent(
                        0,
                        0,
                        KeyEvent.ACTION_DOWN,
                        KeyEvent.KEYCODE_TAB,
                        0,
                        KeyEvent.META_SHIFT_ON);
        // First Shift+Tab syncs ListView's internal selection; second Shift+Tab cannot scroll
        // further (super.onKeyDown returns false while firstVisiblePosition is still 1 > 0), so
        // it falls back to searchAndRequestNextFocus(FOCUS_UP) and focuses topView.
        assertTrue(mListView.onKeyDown(KeyEvent.KEYCODE_TAB, shiftTab));
        if (!topView.hasFocus()) {
            assertTrue(mListView.onKeyDown(KeyEvent.KEYCODE_TAB, shiftTab));
        }
        assertTrue("Expected topView to have focus", topView.hasFocus());
    }

    @Test
    public void testOnWindowFocusChanged_focusesAndPreservesChildFocus() {
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 1", "Item 2")) {
                    @Override
                    public View getView(int position, View convertView, ViewGroup parent) {
                        View view = super.getView(position, convertView, parent);
                        view.setFocusable(true);
                        return view;
                    }
                };
        mListView.setItemsCanFocus(true);
        mListView.setAdapter(adapter);
        mActivity.setContentView(mListView);
        mListView.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        mListView.layout(0, 0, 100, 200);
        mListView.clearFocus();

        final int[] focusedEventCount = {0};
        mListView.setAccessibilityDelegate(
                new View.AccessibilityDelegate() {
                    @Override
                    public void sendAccessibilityEvent(View host, int eventType) {
                        if (eventType == AccessibilityEvent.TYPE_VIEW_FOCUSED) {
                            focusedEventCount[0]++;
                        }
                        super.sendAccessibilityEvent(host, eventType);
                    }
                });

        mListView.onWindowFocusChanged(false);
        assertEquals(0, focusedEventCount[0]);

        // Gaining window focus when unfocused should focus the list and emit TYPE_VIEW_FOCUSED
        // once.
        mListView.onWindowFocusChanged(true);
        assertTrue(mListView.hasFocus());
        assertEquals(1, focusedEventCount[0]);

        // Gaining window focus when a child already has focus should preserve that child's focus.
        View child1 = mListView.getChildAt(1);
        assertTrue(child1.requestFocus());
        assertTrue(child1.hasFocus());
        mListView.onWindowFocusChanged(true);
        assertTrue("Expected child 1 to retain focus", child1.hasFocus());
        assertEquals(1, mListView.getSelectedItemPosition());
    }

    @Test
    public void testOnKeyDown_offScreenTarget_clearsOldChildFocusAndUpdatesSelection() {
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(
                        mActivity,
                        android.R.layout.simple_list_item_1,
                        List.of("Item 0", "Item 1", "Item 2"));
        mListView.setItemsCanFocus(true);
        mListView.setAdapter(adapter);
        mActivity.setContentView(mListView, new ViewGroup.LayoutParams(100, 40));

        // Scroll to position 2 and lay out at 40px height so only position 2 (48px tall) is
        // visible (firstVisiblePosition = 2) and focus its child view.
        mListView.setSelection(2);
        mListView.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(40, View.MeasureSpec.EXACTLY));
        mListView.layout(0, 0, 100, 40);
        assertEquals(2, mListView.getFirstVisiblePosition());
        mListView.getChildAt(0).setFocusable(true);
        assertTrue(mListView.getChildAt(0).requestFocus());
        assertEquals(2, mListView.getSelectedItemPosition());

        // Press Shift+TAB to move up to off-screen position 1 (childIndex = 1 - 2 = -1 < 0).
        // Because ListView#commonKey does not handle Shift+TAB, focusChildAtPosition must clear
        // focus from position 2's view so getSelectedItemPosition() returns 1 instead of 2.
        assertTrue(
                mListView.onKeyDown(
                        KeyEvent.KEYCODE_TAB,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_TAB,
                                0,
                                KeyEvent.META_SHIFT_ON)));
        assertNull(mListView.getFocusedChild());
        assertEquals(1, mListView.getSelectedItemPosition());
    }
}
