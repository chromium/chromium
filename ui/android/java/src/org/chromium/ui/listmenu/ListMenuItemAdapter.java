// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType;
import org.chromium.ui.modelutil.ModelListAdapter;

/** Default adapter for use with {@link ListMenu}. */
public class ListMenuItemAdapter extends ModelListAdapter {
    public ListMenuItemAdapter(ModelList data) {
        super(data);
    }

    @Override
    public boolean areAllItemsEnabled() {
        for (int i = 0; i < getCount(); i++) {
            if (!isEnabled(i)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public boolean isEnabled(int position) {
        return getItemViewType(position) != ListMenuItemType.DIVIDER
                && ((ListItem) getItem(position)).model.get(ListMenuItemProperties.ENABLED);
    }

    @Override
    public long getItemId(int position) {
        return ((ListItem) getItem(position)).model.get(ListMenuItemProperties.MENU_ITEM_ID);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View view = super.getView(position, convertView, parent);

        // https://crbug.copm/802284 Pre-Q only.
        // The view needs to have an OnClickListener for TalkBack to announce the disabled state. In
        // this case, we need to let the ListView handle the click.
        view.setOnClickListener(
                (v) -> {
                    long id =
                            ((ListItem) getItem(position))
                                    .model.get(ListMenuItemProperties.MENU_ITEM_ID);
                    ((ListView) parent).performItemClick(v, position, id);
                });
        // some items have styles specific for disabled state
        view.setEnabled(isEnabled(position));

        return view;
    }
}
