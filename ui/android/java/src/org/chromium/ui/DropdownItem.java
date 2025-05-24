// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

/** Dropdown item interface used to access all the information needed to show the item. */
@NullMarked
public interface DropdownItem {
    // A stand in for a resource ID which indicates no icon should be shown.
    public static final int NO_ICON = 0;

    /** Returns the first part of the first line that should be shown in the dropdown. */
    @Nullable
    String getLabel();

    /** Returns the second part of the first line that should be shown in the dropdown. */
    @Nullable
    String getSecondaryLabel();

    /** Returns the first part of the second line that should be shown in the dropdown. */
    @Nullable String getSublabel();

    /** Returns the second part of the second line that should be shown in the dropdown. */
    @Nullable String getSecondarySublabel();

    /**
     * Returns the drawable id of the icon that should be shown in the dropdown, or NO_ICON. Note:
     * If the getCustomIconUrl() is present, then it'll be preferred over the drawable id returned
     * by getIconId().
     */
    int getIconId();

    /**
     * Returns the url for the icon to be downloaded. If present, the downloaded icon should be
     * preferred over the resource id returned by getIconId().
     */
    @Nullable GURL getCustomIconUrl();

    /** Returns true if the item should be enabled in the dropdown. */
    boolean isEnabled();

    /** Returns true if the item should be a group header in the dropdown. */
    boolean isGroupHeader();

    /** Returns resource ID of label's font color. */
    int getLabelFontColorResId();
}
