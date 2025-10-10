// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuUtils.hasClickListener;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.listmenu.ListMenu.Delegate;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;

import java.util.Collection;
import java.util.Set;

/** Default adapter for use with {@link ListMenu}. */
@NullMarked
public class ListMenuItemAdapter extends ModelListAdapter {

    private static final int INVALID_ITEM_ID = -1;
    private final Collection<Integer> mDisabledTypes;
    private final @Nullable Delegate mDelegate;

    /** Returns a {@link ListMenuItemAdapter} for a list containing {@param data}. */
    public ListMenuItemAdapter(ModelList data) {
        this(data, Set.of(), /* delegate= */ null);
    }

    /**
     * Returns a {@link ListMenuItemAdapter} for a list containing {@param data}. {@param
     * disabledTypes} contains the type enums which should be disabled in the adapter (i.e. not
     * keyboard-focusable or interactable).
     *
     * @param data The {@link ModelList} whose data we should show.
     * @param disabledTypes The type enums which should be disabled in the adapter (i.e. not
     *     keyboard-focusable or interactable).
     * @param delegate The {@link Delegate} used to handle menu clicks. If not provided, the item's
     *     CLICK_LISTENER or listMenu's onMenuItemSelected method will be used. If provided, both
     *     will run.
     */
    public ListMenuItemAdapter(
            ModelList data, Collection<Integer> disabledTypes, @Nullable Delegate delegate) {
        super(data);
        mDisabledTypes = disabledTypes;
        mDelegate = delegate;
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
        ListItem item = ((ListItem) getItem(position));
        if (canReuseView(convertView, item.type)) {
            // If the view is going to be reused, strip existing onClickListeners before binding
            convertView.setOnClickListener(null);
        }

        View view = super.getView(position, convertView, parent);

        if (!view.hasOnClickListeners()) {
            view.setOnClickListener(
                    (v) -> {
                        if (mDelegate != null) {
                            // Delegate, if possible.
                            mDelegate.onItemSelected(item.model, v);
                        } else if (hasClickListener(item)) {
                            // The item had a click listener in the model, but none was bound by the
                            // ViewBinder, and there is no click delegate to use. In this case, call
                            // the model click listener directly.
                            item.model.get(CLICK_LISTENER).onClick(view);
                        } else {
                            // As a fallback, use ListView's performItemClick.
                            long id = getItemId(position);
                            ((ListView) parent).performItemClick(v, position, id);
                        }
                    });
        }
        // some items have styles specific for disabled state
        view.setEnabled(item.model.containsKey(ENABLED) && item.model.get(ENABLED));

        return view;
    }
}
