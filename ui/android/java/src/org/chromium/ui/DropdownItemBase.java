// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

/**
 * Base implementation of DropdownItem which is used to get default settings to
 * show the item.
 */
public class DropdownItemBase implements DropdownItem {
    @Override
    public String getLabel() {
        return null;
    }

    @Override
    public String getSublabel() {
        return null;
    }

    @Override
    public int getIconId() {
        return NO_ICON;
    }

    @Override
    public boolean isEnabled() {
        return true;
    }

    @Override
    public boolean isGroupHeader() {
        return false;
    }

    @Override
    public boolean isMultilineLabel() {
        return false;
    }

    @Override
    public boolean isBoldLabel() {
        return false;
    }

    @Override
    public int getLabelFontColorResId() {
        return R.color.default_text_color_list;
    }

    @Override
    public int getSublabelFontSizeResId() {
        return R.dimen.text_size_small;
    }

    @Override
    public boolean isIconAtStart() {
        return false;
    }

    @Override
    public int getIconSizeResId() {
        return 0;
    }

    @Override
    public int getIconMarginResId() {
        return R.dimen.dropdown_icon_margin;
    }
}
