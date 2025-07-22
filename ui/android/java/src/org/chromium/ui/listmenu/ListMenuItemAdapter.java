// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;

import java.util.Collection;
import java.util.Set;

/** Default adapter for use with {@link ListMenu}. */
@NullMarked
public class ListMenuItemAdapter extends ModelListAdapter {

    private static final int INVALID_ITEM_ID = -1;
    private final Collection<Integer> mDisabledTypes;

    /** Returns a {@link ListMenuItemAdapter} for a list containing {@param data}. */
    public ListMenuItemAdapter(ModelList data) {
        this(data, Set.of());
    }

    /**
     * Returns a {@link ListMenuItemAdapter} for a list containing {@param data}. {@param
     * disabledTypes} contains the type enums which should be disabled in the adapter (i.e. not
     * keyboard-focusable or interactable).
     *
     * @param data The {@link ModelList} whose data we should show.
     * @param disabledTypes The type enums which should be disabled in the adapter (i.e. not
     *     keyboard-focusable or interactable).
     */
    public ListMenuItemAdapter(ModelList data, Collection<Integer> disabledTypes) {
        super(data);
        mDisabledTypes = disabledTypes;
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
        int type = getItemViewType(position);
        return type != ListItemType.DIVIDER && !mDisabledTypes.contains(type);
    }

    @Override
    public long getItemId(int position) {
        if (!isEnabled(position)) return INVALID_ITEM_ID;
        ListItem item = (ListItem) getItem(position);
        if (!item.model.containsKey(MENU_ITEM_ID)) return INVALID_ITEM_ID;
        return item.model.get(MENU_ITEM_ID);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View view = super.getView(position, convertView, parent);

        ListItem item = ((ListItem) getItem(position));

        if (!view.hasOnClickListeners()) {
            // https://crbug.copm/802284 Pre-Q only.
            // The view needs to have an OnClickListener for TalkBack to announce the disabled
            // state. In
            // this case, we need to let the ListView handle the click.
            view.setOnClickListener(
                    (v) -> {
                        long id = getItemId(position);
                        ((ListView) parent).performItemClick(v, position, id);
                    });
        }
        // some items have styles specific for disabled state
        view.setEnabled(item.model.containsKey(ENABLED) && item.model.get(ENABLED));

        return view;
    }
}
