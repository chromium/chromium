// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** A class for cleaning up {@link MVCListAdapter.ModelList}s and their rows. */
@NullMarked
public class ModelListCleaner {

    /**
     * Destroys and clears all rows in the provided {@link MVCListAdapter.ModelList}.
     *
     * <p>For each row, the method searches for the first property in {@code destroyableProperties}
     * that is set in the row's {@link PropertyModel} and destroys the corresponding {@link
     * Destroyable} object.
     *
     * @param modelList The {@link MVCListAdapter.ModelList} to clean up.
     * @param destroyableProperties Properties representing a {@link Destroyable} to be destroyed
     *     for each row.
     */
    @SafeVarargs
    public static void destroyAndClearAllRows(
            MVCListAdapter.ModelList modelList,
            WritableObjectPropertyKey<? extends Destroyable>... destroyableProperties) {
        for (MVCListAdapter.ListItem listItem : modelList) {
            PropertyModel model = listItem.model;
            for (WritableObjectPropertyKey<? extends Destroyable> destroyableProperty :
                    destroyableProperties) {
                @Nullable Destroyable destroyable =
                        model.containsKey(destroyableProperty)
                                ? model.get(destroyableProperty)
                                : null;
                if (destroyable != null) {
                    destroyable.destroy();
                    break;
                }
            }
        }
        modelList.clear();
    }
}
