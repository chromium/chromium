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
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

import java.util.List;

/** Unit tests for {@link KeyboardAccessibleListView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
}
