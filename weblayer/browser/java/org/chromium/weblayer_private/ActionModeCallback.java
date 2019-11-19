// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.SearchManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

import org.chromium.base.PackageManagerUtils;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;

/**
 * A class that handles selection action mode for WebLayer.
 */
public final class ActionModeCallback implements ActionMode.Callback {
    private final ActionModeCallbackHelper mHelper;

    public ActionModeCallback(WebContents webContents) {
        mHelper =
                SelectionPopupController.fromWebContents(webContents).getActionModeCallbackHelper();
    }

    @Override
    public final boolean onCreateActionMode(ActionMode mode, Menu menu) {
        int allowedActionModes = ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT
                | ActionModeCallbackHelper.MENU_ITEM_SHARE;
        if (isWebSearchAvailable()) {
            allowedActionModes |= ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH;
        }
        mHelper.setAllowedMenuItems(allowedActionModes);
        mHelper.onCreateActionMode(mode, menu);
        return true;
    }

    private boolean isWebSearchAvailable() {
        Intent intent = new Intent(Intent.ACTION_WEB_SEARCH);
        intent.putExtra(SearchManager.EXTRA_NEW_SEARCH, true);
        return !PackageManagerUtils.queryIntentActivities(intent, PackageManager.MATCH_DEFAULT_ONLY)
                        .isEmpty();
    }

    @Override
    public final boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        return mHelper.onPrepareActionMode(mode, menu);
    }

    @Override
    public final boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        return mHelper.onActionItemClicked(mode, item);
    }

    @Override
    public final void onDestroyActionMode(ActionMode mode) {
        mHelper.onDestroyActionMode();
    }
}
