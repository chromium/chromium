// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.SearchManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.RemoteException;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.PackageManagerUtils;
import org.chromium.content_public.browser.ActionModeCallback;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.ActionModeItemType;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * A class that handles selection action mode for WebLayer.
 */
public final class WebLayerActionModeCallback extends ActionModeCallback {
    private final ActionModeCallbackHelper mHelper;
    // Can be null during init.
    private @Nullable ITabClient mTabClient;

    // Bitfield of @ActionModeItemType values.
    private int mActionModeOverride;

    // Convert from content ActionModeCallbackHelper.MENU_ITEM_* values to
    // @ActionModeItemType values.
    private static int contentToWebLayerType(int contentType) {
        switch (contentType) {
            case ActionModeCallbackHelper.MENU_ITEM_SHARE:
                return ActionModeItemType.SHARE;
            case ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH:
                return ActionModeItemType.WEB_SEARCH;
            case ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT:
            case 0:
                return 0;
            default:
                assert false;
                return 0;
        }
    }

    public WebLayerActionModeCallback(WebContents webContents) {
        mHelper =
                SelectionPopupController.fromWebContents(webContents).getActionModeCallbackHelper();
    }

    public void setTabClient(ITabClient tabClient) {
        mTabClient = tabClient;
    }

    public void setOverride(int actionModeItemTypes) {
        mActionModeOverride = actionModeItemTypes;
    }

    @Override
    public final boolean onCreateActionMode(ActionMode mode, Menu menu) {
        int allowedActionModes = ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT
                | ActionModeCallbackHelper.MENU_ITEM_SHARE;
        if ((mActionModeOverride & ActionModeItemType.WEB_SEARCH) != 0 || isWebSearchAvailable()) {
            allowedActionModes |= ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH;
        }
        mHelper.setAllowedMenuItems(allowedActionModes);
        mHelper.onCreateActionMode(mode, menu);
        return true;
    }

    private boolean isWebSearchAvailable() {
        Intent intent = new Intent(Intent.ACTION_WEB_SEARCH);
        intent.putExtra(SearchManager.EXTRA_NEW_SEARCH, true);
        return PackageManagerUtils.canResolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY);
    }

    @Override
    public final boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        return mHelper.onPrepareActionMode(mode, menu);
    }

    @Override
    public final boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        int menuItemType = contentToWebLayerType(mHelper.getAllowedMenuItemIfAny(mode, item));
        if ((menuItemType & mActionModeOverride) == 0) {
            return mHelper.onActionItemClicked(mode, item);
        }
        handleMenuItemClick(menuItemType);
        mode.finish();
        return true;
    }

    @Override
    public boolean onDropdownItemClicked(int groupId, int id, @Nullable Intent intent,
            @Nullable View.OnClickListener clickListener) {
        int menuItemType = contentToWebLayerType(mHelper.getAllowedMenuItemIfAny(groupId, id));
        if ((menuItemType & mActionModeOverride) == 0) {
            return mHelper.onDropdownItemClicked(groupId, id, intent, clickListener);
        }
        handleMenuItemClick(menuItemType);
        mHelper.dismissMenu();
        return true;
    }

    private void handleMenuItemClick(int menuItemType) {
        assert WebLayerFactoryImpl.getClientMajorVersion() >= 88;
        try {
            mTabClient.onActionItemClicked(
                    menuItemType, ObjectWrapper.wrap(mHelper.getSelectedText()));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    public final void onDestroyActionMode(ActionMode mode) {
        mHelper.onDestroyActionMode();
    }

    @Override
    public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
        mHelper.onGetContentRect(mode, view, outRect);
    }
}
