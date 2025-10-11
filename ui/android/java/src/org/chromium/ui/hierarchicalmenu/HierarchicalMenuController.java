// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;

/**
 * A controller to manage the logic for hierarchical menus i.e., flyout and drilldown.
 *
 * <p>This class centralizes the logic for handling submenu interactions. It uses a {@link
 * HierarchicalMenuKeyProvider} to interact with a menu's PropertyModel.
 *
 * @param <T> The type of the object that the {@link FlyoutHandler} manages (e.g., PopupWindow).
 */
@NullMarked
public class HierarchicalMenuController<T> {
    private final @Nullable FlyoutController<T> mFlyoutController;

    /**
     * Creates an instance of the controller.
     *
     * @param keyProvider The {@link HierarchicalMenuKeyProvider} for the controller to use.
     * @param flyoutHandler The {@link FlyoutHandler} for the controller to use for displaying
     *     flyout popups.
     */
    public HierarchicalMenuController(
            HierarchicalMenuKeyProvider keyProvider, @Nullable FlyoutHandler<T> flyoutHandler) {
        mFlyoutController =
                flyoutHandler != null ? new FlyoutController<T>(flyoutHandler, keyProvider) : null;
    }

    /**
     * Gets the {@link FlyoutController}.
     *
     * @return The {@link FlyoutController} that this controller manages.
     */
    public @Nullable FlyoutController<T> getFlyoutController() {
        return mFlyoutController;
    }
}
