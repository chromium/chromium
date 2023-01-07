// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import androidx.annotation.NonNull;

import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Observes and notifies when any of the filtered {@link PropertyKey}s are changed inside the
 * {@link ModelList}.
 */
public class ModelListPropertyChangeFilter
        implements ListObserver<Void>, PropertyObserver<PropertyKey> {
    private final Runnable mOnPropertyChange;
    private final ModelList mModelList;
    private final Set<PropertyKey> mPropertyKeySet;

    // Used to keep track of removed PropertyModels so we can remove ourself as observers when
    // they're no longer in the ModelList.
    private Set<PropertyModel> mTrackedPropertyModels = new HashSet<>();

    /**
     * Creates a filter that will notify the runnable whenever a specified property in the model
     * list changes.
     * @param onPropertyChange The callback to invoke when a property changes.
     * @param modelList The filter will observe every PropertyModel in this list.
     * @param filterPropertyKeySet The properties that are worth notifying on.
     */
    public ModelListPropertyChangeFilter(
            Runnable onPropertyChange, ModelList modelList, Set<PropertyKey> filterPropertyKeySet) {
        mOnPropertyChange = onPropertyChange;
        mModelList = modelList;
        mPropertyKeySet = filterPropertyKeySet;

        mModelList.addObserver(this);
        onItemRangeInserted(mModelList, 0, mModelList.size());
    }

    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        for (int i = 0; i < count; i++) {
            ListItem listItem = mModelList.get(index + i);
            listItem.model.addObserver(this);
            mTrackedPropertyModels.add(listItem.model);
        }
        mOnPropertyChange.run();
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        Set<PropertyModel> newPropertyModels = new HashSet<>();
        for (int i = 0; i < mModelList.size(); i++) {
            newPropertyModels.add(mModelList.get(i).model);
        }
        prunePropertyModels(newPropertyModels);
        mOnPropertyChange.run();
    }

    @Override
    public void onPropertyChanged(PropertyObservable<PropertyKey> source, PropertyKey propertyKey) {
        if (mPropertyKeySet.contains(propertyKey)) {
            mOnPropertyChange.run();
        }
    }

    /** Remove all observers. */
    public void destroy() {
        mModelList.removeObserver(this);
        prunePropertyModels(Collections.emptySet());
    }

    /**
     * When a {@link PropertyModel} is removed from the {@link ModelList}, the notification method
     * does not contain the PropertyModel objects that have been removed. They are no longer in the
     * ModelList either. But we've called {@link PropertyModel#addObserver(PropertyObserver)} on
     * them, and we need to remove ourselves as observers. So this filter class is tracking all of
     * the observed PropertyModel objects we've subscribed to, and in this method we compare the old
     * set and the new set, and call  {@link PropertyModel#removeObserver(PropertyObserver)} on any
     * we figure out have been removed.
     */
    private void prunePropertyModels(@NonNull Set<PropertyModel> newPropertyModels) {
        for (PropertyModel existingPropertyModel : mTrackedPropertyModels) {
            if (!newPropertyModels.contains(existingPropertyModel)) {
                existingPropertyModel.removeObserver(this);
            }
        }
        mTrackedPropertyModels = newPropertyModels;
    }
}
