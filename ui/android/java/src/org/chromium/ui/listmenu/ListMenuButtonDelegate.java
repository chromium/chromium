// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.view.View;

import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/** A delegate used to populate the menu. */
public interface ListMenuButtonDelegate {
    /**
     * @param listMenuButton The anchor for the {@link ListMenu}.
     * @return A {@link RectProvider} representing a position in screen space.
     */
    default RectProvider getRectProvider(View listMenuButton) {
        ViewRectProvider provider = new ViewRectProvider(listMenuButton);
        provider.setIncludePadding(true);
        return provider;
    }

    /**
     * @return The {@link ListMenu} displayed by {@link ListMenuButton}.
     */
    ListMenu getListMenu();
}
