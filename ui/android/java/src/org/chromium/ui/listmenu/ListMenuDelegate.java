// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/** A delegate used to populate the menu. */
@NullMarked
public interface ListMenuDelegate {
    /**
     * @param listMenuHostingView The anchor for the {@link ListMenu}.
     * @return A {@link RectProvider} representing a position in screen space.
     */
    default RectProvider getRectProvider(View listMenuHostingView) {
        ViewRectProvider provider = new ViewRectProvider(listMenuHostingView);
        provider.setIncludePadding(true);
        return provider;
    }

    /**
     * @return The {@link ListMenu} displayed by the list menu hosting view.
     */
    ListMenu getListMenu();

    /**
     * @param The parent {@ListItem} that contains submenu items.
     * @return The {@link ListMenu} with the contents of the submenu.
     */
    default @Nullable ListMenu getListMenuFromParentListItem(ListItem item) {
        return null;
    }
}
