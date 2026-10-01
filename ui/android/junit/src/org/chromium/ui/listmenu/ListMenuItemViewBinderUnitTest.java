// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ListMenuItemViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ListMenuItemViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnGenericMotionListener mOnGenericMotionListener;

    private Context mContext;
    private ViewGroup mListItemView;
    private TextView mTextView;
    private ImageView mStartIcon;
    private ImageView mEndIcon;
    private ImageView mSubmenuArrow;
    private TextView mSubtitleView;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        // Hierarchy from list_menu_item.xml:
        // mListItemView (LinearLayout)
        //   - mStartIcon (ImageView)
        //   - mInnerLayout (LinearLayout)
        //      - mTextView (TextView)
        //      - mSubtitleView (TextView)
        //   - mEndIcon (ImageView)
        //   - mSubmenuArrow (ImageView)
        mListItemView = new LinearLayout(mContext);
        mStartIcon = new ImageView(mContext);
        mStartIcon.setId(R.id.menu_item_icon);
        mStartIcon.setVisibility(View.GONE);
        LinearLayout innerLayout = new LinearLayout(mContext);
        mTextView = new TextView(mContext);
        mTextView.setId(R.id.menu_item_text);
        mSubtitleView = new TextView(mContext);
        mSubtitleView.setId(R.id.menu_item_subtitle);
        mSubtitleView.setVisibility(View.GONE);
        mEndIcon = new ImageView(mContext);
        mEndIcon.setId(R.id.menu_item_end_icon);
        mEndIcon.setVisibility(View.GONE);
        mSubmenuArrow = new ImageView(mContext);
        mSubmenuArrow.setId(R.id.submenu_arrow);

        innerLayout.addView(mTextView);
        innerLayout.addView(mSubtitleView);

        mListItemView.addView(mStartIcon);
        mListItemView.addView(innerLayout);
        mListItemView.addView(mEndIcon);
        mListItemView.addView(mSubmenuArrow);
    }

    @Test
    public void testSetTitle() {
        String title = "Test Title";
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .build();
        ListMenuItemViewBinder.binder(propertyModel, mListItemView, ListMenuItemProperties.TITLE);
        Assert.assertEquals(title, mTextView.getText().toString());
    }

    @Test
    public void testSetSubtitle() {
        String subtitle = "Test Subtitle";
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.SUBTITLE, subtitle)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SUBTITLE);
        Assert.assertEquals(subtitle, mSubtitleView.getText().toString());
        Assert.assertEquals(View.VISIBLE, mSubtitleView.getVisibility());

        propertyModel.set(ListMenuItemProperties.SUBTITLE, "");
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SUBTITLE);
        Assert.assertEquals("", mSubtitleView.getText().toString());
        Assert.assertEquals(View.GONE, mSubtitleView.getVisibility());
    }

    @Test
    public void testSubtitleTextAppearance() {
        int customStyleId = 123;
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.SUBTITLE_TEXT_APPEARANCE_ID, customStyleId)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SUBTITLE_TEXT_APPEARANCE_ID);
        Assert.assertEquals(customStyleId, Shadows.shadowOf(mSubtitleView).getTextAppearanceId());

        // Verify resetting when Resources.ID_NULL.
        PropertyModel resetModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.SUBTITLE_TEXT_APPEARANCE_ID, Resources.ID_NULL)
                        .build();
        ListMenuItemViewBinder.binder(
                resetModel, mListItemView, ListMenuItemProperties.SUBTITLE_TEXT_APPEARANCE_ID);
        Assert.assertEquals(
                R.style.TextAppearance_ListMenuItem_Subtitle,
                Shadows.shadowOf(mSubtitleView).getTextAppearanceId());
    }

    @Test
    public void testVerticalPadding() {
        int verticalPadding = 24;
        int paddingStart = 16;
        int paddingEnd = 16;
        mListItemView.setPaddingRelative(paddingStart, 0, paddingEnd, 0);

        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.VERTICAL_PADDING, verticalPadding)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.VERTICAL_PADDING);

        Assert.assertEquals(paddingStart, mListItemView.getPaddingStart());
        Assert.assertEquals(verticalPadding, mListItemView.getPaddingTop());
        Assert.assertEquals(paddingEnd, mListItemView.getPaddingEnd());
        Assert.assertEquals(verticalPadding, mListItemView.getPaddingBottom());

        // Verify resetting when vertical padding is 0.
        PropertyModel resetModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.VERTICAL_PADDING, 0)
                        .build();
        ListMenuItemViewBinder.binder(
                resetModel, mListItemView, ListMenuItemProperties.VERTICAL_PADDING);

        Assert.assertEquals(paddingStart, mListItemView.getPaddingStart());
        Assert.assertEquals(0, mListItemView.getPaddingTop());
        Assert.assertEquals(paddingEnd, mListItemView.getPaddingEnd());
        Assert.assertEquals(0, mListItemView.getPaddingBottom());
    }

    @Test
    public void testStartIconBitmap() {
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_BITMAP, bitmap)
                        .build();

        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_BITMAP);
        Assert.assertTrue(mStartIcon.getDrawable() instanceof BitmapDrawable);
        Assert.assertEquals(View.VISIBLE, mStartIcon.getVisibility());

        propertyModel.set(ListMenuItemProperties.START_ICON_BITMAP, null);
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_BITMAP);
        Assert.assertNull(mStartIcon.getDrawable());
        Assert.assertEquals(View.GONE, mStartIcon.getVisibility());
    }

    @Test
    public void testStartIconBitmapWithKeepSpacing() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN, true)
                        .with(ListMenuItemProperties.START_ICON_BITMAP, null)
                        .build();

        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_BITMAP);

        Assert.assertNull(mStartIcon.getDrawable());
        Assert.assertEquals(View.INVISIBLE, mStartIcon.getVisibility());
    }

    @Test
    public void testEnabledState() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.ENABLED, false)
                        .build();
        ListMenuItemViewBinder.binder(propertyModel, mListItemView, ListMenuItemProperties.ENABLED);
        Assert.assertFalse(mListItemView.isEnabled());
        Assert.assertFalse(mTextView.isEnabled());
        Assert.assertFalse(mStartIcon.isEnabled());
        Assert.assertFalse(mEndIcon.isEnabled());

        propertyModel.set(ListMenuItemProperties.ENABLED, true);
        ListMenuItemViewBinder.binder(propertyModel, mListItemView, ListMenuItemProperties.ENABLED);
        Assert.assertTrue(mListItemView.isEnabled());
        Assert.assertTrue(mTextView.isEnabled());
        Assert.assertTrue(mStartIcon.isEnabled());
        Assert.assertTrue(mEndIcon.isEnabled());
    }

    @Test
    public void testIconTint() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);

        ColorStateList expectedTint =
                mContext.getColorStateList(R.color.default_text_color_link_baseline);
        Assert.assertEquals(expectedTint, mStartIcon.getImageTintList());
        Assert.assertEquals(expectedTint, mEndIcon.getImageTintList());
        Assert.assertEquals(expectedTint, mSubmenuArrow.getImageTintList());

        propertyModel.set(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID, Resources.ID_NULL);
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);
        Assert.assertNull(mStartIcon.getImageTintList());
        Assert.assertNull(mEndIcon.getImageTintList());
        Assert.assertNull(mSubmenuArrow.getImageTintList());
    }

    @Test
    public void testIconTint_shouldNotTintEndIcon() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .with(ListMenuItemProperties.SHOULD_TINT_END_ICON, false)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);

        ColorStateList expectedTint =
                mContext.getColorStateList(R.color.default_text_color_link_baseline);
        Assert.assertEquals(expectedTint, mStartIcon.getImageTintList());
        Assert.assertEquals(expectedTint, mSubmenuArrow.getImageTintList());
        // End icon should have its tint cleared (set to null)
        Assert.assertNull(mEndIcon.getImageTintList());
    }

    @Test
    public void testShouldTintEndIconProperty() {
        mEndIcon.setImageTintList(
                mContext.getColorStateList(R.color.default_text_color_link_baseline));
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.SHOULD_TINT_END_ICON, false)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SHOULD_TINT_END_ICON);

        // End icon should have its tint cleared (set to null)
        Assert.assertNull(mEndIcon.getImageTintList());
    }

    @Test
    public void testShouldTintEndIconProperty_recyclesToTrue() {
        ColorStateList expectedTint =
                mContext.getColorStateList(R.color.default_text_color_link_baseline);
        mEndIcon.setImageTintList(expectedTint);
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .with(ListMenuItemProperties.SHOULD_TINT_END_ICON, false)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SHOULD_TINT_END_ICON);

        // End icon should have its tint cleared (set to null)
        Assert.assertNull(mEndIcon.getImageTintList());

        // Set shouldTintEndIcon to true
        propertyModel.set(ListMenuItemProperties.SHOULD_TINT_END_ICON, true);
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SHOULD_TINT_END_ICON);

        // Now end icon should be tinted using ICON_TINT_COLOR_STATE_LIST_ID
        Assert.assertEquals(expectedTint, mEndIcon.getImageTintList());
    }

    @Test
    public void testStartIconId() {
        mEndIcon.setVisibility(View.VISIBLE);
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_ID, R.drawable.ic_delete_fill_24dp)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_ID);
        Assert.assertNotNull(mStartIcon.getDrawable());
        Assert.assertEquals(View.VISIBLE, mStartIcon.getVisibility());
        Assert.assertEquals(View.VISIBLE, mEndIcon.getVisibility());
    }

    @Test
    public void testEndIconId() {
        mStartIcon.setVisibility(View.VISIBLE);
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.END_ICON_ID, R.drawable.ic_delete_fill_24dp)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.END_ICON_ID);
        Assert.assertNotNull(mEndIcon.getDrawable());
        Assert.assertEquals(View.VISIBLE, mEndIcon.getVisibility());
        Assert.assertEquals(View.VISIBLE, mStartIcon.getVisibility());
    }

    @Test
    public void testSubmenuHeaderIconTint() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .build();

        // Verifies that ListMenuSubmenuHeaderViewBinder correctly tints all icons in the view.
        ListMenuSubmenuHeaderViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);

        ColorStateList expectedTint =
                mContext.getColorStateList(R.color.default_text_color_link_baseline);
        Assert.assertEquals(expectedTint, mStartIcon.getImageTintList());
        Assert.assertEquals(expectedTint, mEndIcon.getImageTintList());
        Assert.assertEquals(expectedTint, mSubmenuArrow.getImageTintList());

        propertyModel.set(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID, Resources.ID_NULL);
        ListMenuSubmenuHeaderViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);
        Assert.assertNull(mStartIcon.getImageTintList());
        Assert.assertNull(mEndIcon.getImageTintList());
        Assert.assertNull(mSubmenuArrow.getImageTintList());
    }

    @Test
    public void testStartIconWidth() {
        int width = 12;
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_WIDTH, width)
                        .build();

        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_WIDTH);

        Assert.assertEquals(width, mStartIcon.getLayoutParams().width);
    }

    @Test
    public void testBothIcons() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_ID, R.drawable.ic_delete_fill_24dp)
                        .with(ListMenuItemProperties.END_ICON_ID, R.drawable.ic_delete_fill_24dp)
                        .build();

        // Bind START_ICON_ID. It should show start icon and NOT hide end icon.
        mEndIcon.setVisibility(View.VISIBLE);
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_ID);
        Assert.assertNotNull(mStartIcon.getDrawable());
        Assert.assertEquals(View.VISIBLE, mStartIcon.getVisibility());
        Assert.assertEquals(View.VISIBLE, mEndIcon.getVisibility());

        // Bind END_ICON_ID. It should show end icon and NOT hide start icon.
        mEndIcon.setVisibility(View.GONE);
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.END_ICON_ID);
        Assert.assertNotNull(mEndIcon.getDrawable());
        Assert.assertEquals(View.VISIBLE, mEndIcon.getVisibility());
        // Verify start icon wasn't hidden.
        Assert.assertEquals(View.VISIBLE, mStartIcon.getVisibility());
    }

    @Test
    public void testStartIconBitmapDoesNotOverrideDrawable() {
        Drawable drawable =
                AppCompatResources.getDrawable(mContext, R.drawable.ic_delete_fill_24dp);
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_DRAWABLE, drawable)
                        .with(ListMenuItemProperties.START_ICON_BITMAP, null)
                        .build();

        // 1. Bind START_ICON_DRAWABLE. Should set drawable and make visible.
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_DRAWABLE);
        Assert.assertEquals(drawable, mStartIcon.getDrawable());
        Assert.assertEquals(View.VISIBLE, mStartIcon.getVisibility());

        // 2. Bind START_ICON_BITMAP (which is null). Should NOT hide the icon.
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_BITMAP);
        Assert.assertEquals(drawable, mStartIcon.getDrawable());
        Assert.assertEquals(View.VISIBLE, mStartIcon.getVisibility());
    }

    @Test
    public void testStartIconDrawableDoesNotOverrideBitmap() {
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_BITMAP, bitmap)
                        .with(ListMenuItemProperties.START_ICON_DRAWABLE, null)
                        .build();

        // 1. Bind START_ICON_BITMAP. Should set drawable and make visible.
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_BITMAP);
        Assert.assertTrue(mStartIcon.getDrawable() instanceof BitmapDrawable);
        Assert.assertEquals(View.VISIBLE, mStartIcon.getVisibility());

        // 2. Bind START_ICON_DRAWABLE (which is null). Should NOT hide the icon.
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_DRAWABLE);
        Assert.assertTrue(mStartIcon.getDrawable() instanceof BitmapDrawable);
        Assert.assertEquals(View.VISIBLE, mStartIcon.getVisibility());
    }

    @Test
    public void testGenericMotionListener() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.GENERIC_MOTION_LISTENER,
                                mOnGenericMotionListener)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.GENERIC_MOTION_LISTENER);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_BUTTON_PRESS, 0, 0, 0);
        mListItemView.dispatchGenericMotionEvent(event);
        verify(mOnGenericMotionListener).onGenericMotion(mListItemView, event);
    }

    @Test
    public void testCheckableAndChecked_CheckedTrue() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.CHECKABLE, true)
                        .with(ListMenuItemProperties.CHECKED, true)
                        .with(ListMenuItemProperties.POSITION, 1)
                        .build();

        View view =
                new TextView(mContext) {
                    @Override
                    public void setAccessibilityDelegate(AccessibilityDelegate delegate) {
                        super.setAccessibilityDelegate(delegate);
                        AccessibilityNodeInfo nodeInfo = AccessibilityNodeInfo.obtain();
                        delegate.onInitializeAccessibilityNodeInfo(this, nodeInfo);
                        Assert.assertEquals(RadioButton.class.getName(), nodeInfo.getClassName());
                        Assert.assertTrue(nodeInfo.isCheckable());
                        Assert.assertTrue(nodeInfo.isChecked());
                        Assert.assertNotNull(nodeInfo.getCollectionItemInfo());
                        Assert.assertEquals(1, nodeInfo.getCollectionItemInfo().getRowIndex());
                        Assert.assertEquals(0, nodeInfo.getCollectionItemInfo().getColumnIndex());
                    }
                };

        ListMenuItemViewBinder.binder(propertyModel, view, ListMenuItemProperties.CHECKABLE);
    }

    @Test
    public void testCheckableAndChecked_CheckedFalse() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.CHECKABLE, true)
                        .with(ListMenuItemProperties.CHECKED, false)
                        .with(ListMenuItemProperties.POSITION, 2)
                        .build();

        View view =
                new TextView(mContext) {
                    @Override
                    public void setAccessibilityDelegate(AccessibilityDelegate delegate) {
                        super.setAccessibilityDelegate(delegate);
                        AccessibilityNodeInfo nodeInfo = AccessibilityNodeInfo.obtain();
                        delegate.onInitializeAccessibilityNodeInfo(this, nodeInfo);
                        Assert.assertEquals(RadioButton.class.getName(), nodeInfo.getClassName());
                        Assert.assertTrue(nodeInfo.isCheckable());
                        Assert.assertFalse(nodeInfo.isChecked());
                        Assert.assertNotNull(nodeInfo.getCollectionItemInfo());
                        Assert.assertEquals(2, nodeInfo.getCollectionItemInfo().getRowIndex());
                        Assert.assertEquals(0, nodeInfo.getCollectionItemInfo().getColumnIndex());
                    }
                };

        ListMenuItemViewBinder.binder(propertyModel, view, ListMenuItemProperties.CHECKABLE);
    }

    @Test
    public void testCheckableAndChecked_NotCheckedProperty() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.CHECKABLE, true)
                        .build();

        View view =
                new TextView(mContext) {
                    @Override
                    public void setAccessibilityDelegate(AccessibilityDelegate delegate) {
                        super.setAccessibilityDelegate(delegate);
                        AccessibilityNodeInfo nodeInfo = AccessibilityNodeInfo.obtain();
                        delegate.onInitializeAccessibilityNodeInfo(this, nodeInfo);
                        Assert.assertEquals(RadioButton.class.getName(), nodeInfo.getClassName());
                        Assert.assertTrue(nodeInfo.isCheckable());
                        Assert.assertFalse(nodeInfo.isChecked());
                        Assert.assertNotNull(nodeInfo.getCollectionItemInfo());
                        Assert.assertEquals(0, nodeInfo.getCollectionItemInfo().getRowIndex());
                        Assert.assertEquals(0, nodeInfo.getCollectionItemInfo().getColumnIndex());
                    }
                };

        ListMenuItemViewBinder.binder(propertyModel, view, ListMenuItemProperties.CHECKABLE);
    }
}
