// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/**
 * An interface to provide {@link PropertyKey}s for a hierarchical menu system.
 *
 * <p>This interface decouples the generic submenu logic in HierarchicalMenuController from specific
 * menu implementations. By implementing this interface, a menu system can tell the generic
 * controller which {@code PropertyKey}s to use for accessing essential properties like the title,
 * click listeners, and the list of submenu items from its PropertyModel.
 */
@NullMarked
public interface HierarchicalMenuKeyProvider {
    WritableObjectPropertyKey<View.@Nullable OnClickListener> getClickListenerKey();

    WritableBooleanPropertyKey getEnabledKey();

    WritableObjectPropertyKey<View.@Nullable OnHoverListener> getHoverListenerKey();

    WritableObjectPropertyKey<CharSequence> getTitleKey();

    WritableIntPropertyKey getTitleIdKey();

    WritableObjectPropertyKey<View.OnKeyListener> getKeyListenerKey();

    WritableObjectPropertyKey<List<ListItem>> getSubmenuItemsKey();

    WritableBooleanPropertyKey getIsHighlightedKey();
}
