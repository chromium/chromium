// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;

/**
 * Class responsible for binding the model of the ListMenuItem and the view. Each item is expected
 * to have at the bare minimum a title (TITLE_ID, or TITLE) or an icon (START_ICON_ID,
 * START_ICON_DRAWABLE). All other properties while recommended, are optional.
 *
 * <p>As for when a list item contains an icon, it is expected that it either has a start icon OR an
 * end icon, not both.
 */
@NullMarked
public class ListMenuItemViewBinder {
    public static void binder(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView =
                (view instanceof TextView text) ? text : view.findViewById(R.id.menu_item_text);
        @Nullable ImageView startIcon = view.findViewById(R.id.menu_item_icon);
        @Nullable ImageView endIcon = view.findViewById(R.id.menu_item_end_icon);
        boolean keepIconSpacing =
                model.containsKey(ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN)
                        && model.get(ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN);
        if (propertyKey == ListMenuItemProperties.TITLE_ID) {
            @StringRes int titleId = model.get(ListMenuItemProperties.TITLE_ID);
            if (titleId != 0) {
                textView.setText(titleId);
            }
        } else if (propertyKey == ListMenuItemProperties.TITLE) {
            CharSequence title = model.get(ListMenuItemProperties.TITLE);
            if (title != null) {
                textView.setText(title);
            }
        } else if (propertyKey == ListMenuItemProperties.SUBTITLE) {
            TextView subtitleView = view.findViewById(R.id.menu_item_subtitle);
            CharSequence subtitleText = model.get(ListMenuItemProperties.SUBTITLE);
            subtitleView.setText(subtitleText != null ? subtitleText : "");
            subtitleView.setVisibility(TextUtils.isEmpty(subtitleText) ? View.GONE : View.VISIBLE);
        } else if (propertyKey == ListMenuItemProperties.IS_SUBTITLE_ELLIPSIZED_AT_END) {
            TextView subtitleView = view.findViewById(R.id.menu_item_subtitle);
            if (model.get(ListMenuItemProperties.IS_SUBTITLE_ELLIPSIZED_AT_END)) {
                subtitleView.setMaxLines(1);
                subtitleView.setEllipsize(TextUtils.TruncateAt.END);
            } else {
                subtitleView.setEllipsize(null);
                subtitleView.setMaxLines(Integer.MAX_VALUE);
            }
        } else if (propertyKey == ListMenuItemProperties.CONTENT_DESCRIPTION) {
            textView.setContentDescription(model.get(ListMenuItemProperties.CONTENT_DESCRIPTION));
        } else if (propertyKey == ListMenuItemProperties.TOOLTIP) {
            view.setTooltipText(model.get(ListMenuItemProperties.TOOLTIP));
        } else if (propertyKey == ListMenuItemProperties.START_ICON_ID
                || propertyKey == ListMenuItemProperties.END_ICON_ID) {
            int id = model.get((ReadableIntPropertyKey) propertyKey);
            Drawable drawable =
                    id == 0 ? null : AppCompatResources.getDrawable(view.getContext(), id);
            if (propertyKey == ListMenuItemProperties.START_ICON_ID) {
                setStartIcon(startIcon, endIcon, drawable, keepIconSpacing);
            } else {
                setEndIcon(startIcon, endIcon, drawable, keepIconSpacing);
            }
        } else if (propertyKey == ListMenuItemProperties.START_ICON_DRAWABLE) {
            Drawable drawable = model.get(ListMenuItemProperties.START_ICON_DRAWABLE);
            setStartIcon(startIcon, endIcon, drawable, keepIconSpacing);
        } else if (propertyKey == ListMenuItemProperties.START_ICON_BITMAP) {
            Bitmap bitmap = model.get(ListMenuItemProperties.START_ICON_BITMAP);
            if (bitmap == null) {
                hideStartIcon(startIcon, keepIconSpacing);
            } else {
                Drawable drawable = new BitmapDrawable(view.getResources(), bitmap);
                setStartIcon(startIcon, endIcon, drawable, keepIconSpacing);
            }
        } else if (propertyKey == ListMenuItemProperties.GROUP_ID) {
            // Not tracked intentionally because it's mainly for clients to know which group a
            // menu item belongs to.
        } else if (propertyKey == ListMenuItemProperties.MENU_ITEM_ID) {
            // Not tracked intentionally because it's mainly for clients to know which menu item is
            // clicked.
        } else if (propertyKey == ListMenuItemProperties.CLICK_LISTENER) {
            // Not tracked intentionally because it's mainly for setting a custom click listener
            // for an item. The click listener will be expected to be retrieved and used
            // by the component using this binder and not the binder itself.
        } else if (propertyKey == ListMenuItemProperties.HOVER_LISTENER) {
            view.setOnHoverListener(model.get(ListMenuItemProperties.HOVER_LISTENER));
        } else if (propertyKey == ListMenuItemProperties.IS_HIGHLIGHTED) {
            view.setHovered(model.get(ListMenuItemProperties.IS_HIGHLIGHTED));
        } else if (propertyKey == ListMenuItemProperties.INTENT) {
            // Not tracked intentionally because it's mainly for setting a custom intent
            // for an item. The intent will be expected to be retrieved and used
            // by the component using this binder and not the binder itself.
        } else if (propertyKey == ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN) {
            if (startIcon.getVisibility() != View.VISIBLE) {
                // Update the "hidden" visibility type as needed.
                hideStartIcon(
                        startIcon,
                        model.get(ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN));
            }
        } else if (propertyKey == ListMenuItemProperties.ENABLED) {
            // Set enabled state on view, textView, and icons (because with some layout files,
            // textView and icons inherit state from view, and sometimes they don't)
            view.setEnabled(model.get(ListMenuItemProperties.ENABLED));
            textView.setEnabled(model.get(ListMenuItemProperties.ENABLED));
            if (startIcon != null) startIcon.setEnabled(model.get(ListMenuItemProperties.ENABLED));
            if (endIcon != null) endIcon.setEnabled(model.get(ListMenuItemProperties.ENABLED));
        } else if (propertyKey == ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID) {
            @ColorRes
            int tintColorId = model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);
            if (tintColorId != 0) {
                ImageViewCompat.setImageTintList(
                        startIcon,
                        AppCompatResources.getColorStateList(
                                view.getContext(),
                                model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID)));
                ImageViewCompat.setImageTintList(
                        endIcon,
                        AppCompatResources.getColorStateList(
                                view.getContext(),
                                model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID)));
            } else {
                // No tint.
                ImageViewCompat.setImageTintList(startIcon, null);
                ImageViewCompat.setImageTintList(endIcon, null);
            }
        } else if (propertyKey == ListMenuItemProperties.TEXT_APPEARANCE_ID) {
            textView.setTextAppearance(model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        } else if (propertyKey == ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END) {
            if (model.get(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END)) {
                textView.setMaxLines(1);
                textView.setEllipsize(TextUtils.TruncateAt.END);
            } else {
                textView.setEllipsize(null);
            }
        } else if (propertyKey == ListMenuItemProperties.KEY_LISTENER) {
            view.setOnKeyListener(model.get(ListMenuItemProperties.KEY_LISTENER));
        } else if (propertyKey == ListMenuItemProperties.TOUCH_LISTENER) {
            view.setOnTouchListener(model.get(ListMenuItemProperties.TOUCH_LISTENER));
        } else if (propertyKey == ListMenuItemProperties.ORDER) {
            // Not tracked intentionally because it's used by clients to keep track of items. The
            // order field is used to recreate a SelectionMenuItem when an item is clicked.
        } else {
            assert false : "Supplied propertyKey not implemented in ListMenuItemProperties.";
        }
    }

    private static void setStartIcon(
            ImageView startIcon,
            @Nullable ImageView endIcon,
            @Nullable Drawable drawable,
            boolean keepStartIconSpacing) {
        if (drawable != null) {
            startIcon.setImageDrawable(drawable);
            startIcon.setVisibility(View.VISIBLE);
            hideEndIcon(endIcon);
        } else {
            hideStartIcon(startIcon, keepStartIconSpacing);
        }
    }

    private static void setEndIcon(
            @Nullable ImageView startIcon,
            ImageView endIcon,
            @Nullable Drawable drawable,
            boolean keepStartIconSpacing) {
        if (drawable != null) {
            // Move to the end.
            endIcon.setImageDrawable(drawable);
            endIcon.setVisibility(View.VISIBLE);
            hideStartIcon(startIcon, keepStartIconSpacing);
        } else {
            hideEndIcon(endIcon);
        }
    }

    private static void hideStartIcon(@Nullable ImageView startIcon, boolean keepIconSpacing) {
        if (startIcon == null) return;
        startIcon.setImageDrawable(null);
        startIcon.setVisibility(keepIconSpacing ? View.INVISIBLE : View.GONE);
    }

    private static void hideEndIcon(@Nullable ImageView endIcon) {
        if (endIcon == null) return;
        endIcon.setImageDrawable(null);
        endIcon.setVisibility(View.GONE);
    }
}
