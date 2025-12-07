// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

/**
 * Base implementation of DropdownItem which is used to get default settings to
 * show the item.
 */
@NullMarked
public class DropdownItemBase implements DropdownItem {
    @Override
    public @Nullable String getLabel() {
        return null;
    }

    @Override
    public @Nullable String getSecondaryLabel() {
        return null;
    }

    @Override
    public @Nullable String getSublabel() {
        return null;
    }

    @Override
    public @Nullable String getSecondarySublabel() {
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
    public int getLabelFontColorResId() {
        return R.color.default_text_color_list_baseline;
    }

    @Override
    public @Nullable GURL getCustomIconUrl() {
        return null;
    }
}
