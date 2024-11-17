// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modaldialog;

import androidx.annotation.Nullable;

import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogPriority;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Consumer;

/**
 * A container class to provide basic operations for pending dialogs with attributes {@link
 * ModalDialogType} and {@link ModalDialogPriority}.
 */
class PendingDialogContainer {
    /** A class representing the attributes of a pending dialog. */
    static class PendingDialogType {
        public final PropertyModel propertyModel;
        public final @ModalDialogType int dialogType;
        public final @ModalDialogPriority int dialogPriority;

        PendingDialogType(
                PropertyModel model, @ModalDialogType int type, @ModalDialogPriority int priority) {
            propertyModel = model;
            dialogType = type;
            dialogPriority = priority;
        }
    }

    /**
     * Mapping of the lists of pending dialogs based on {@link ModalDialogType} and {@link
     * ModalDialogPriority} of the dialogs.
     *
     * The key of this map is 10 * {@link ModalDialogType} + {@link ModalDialogPriority}.
     * This allows for efficient retrievals of pending dialogs sliced on {@link ModalDialogType}
     * or {@link ModalDialogType}.
     *
     * Iterating over pending dialogs based on {@link ModalDialogType} is useful when suspending
     * dialogs for certain types.
     *
     * Iterating over pending dialogs based on {@link ModalDialogPriority} is useful when
     * choosing the next dialog to be shown.
     *
     * Please note, this only works if the MAX_RANGE of {@link ModalDialogPriority} is <= 9,
     * otherwise we can have situations where two different dialogs on either {@link
     * ModalDialogType} or {@link ModalDialogPriority} can be mapped to the same key.
     *
     * For example:
     * 10 * (ModalDialogType: 2) + (ModalDialogPriority: 9) = 10 * (ModalDialogType: 1) +
     * (ModalDialogPriority: 19) would map to the same key 29.
     */
    private final HashMap<Integer, List<PropertyModel>> mPendingDialogs = new HashMap<>();

    /**
     * Queues the {@link PropertyModel} model in the container based on the {@link
     * ModalDialogPriority} and {@link ModalDialogType}.
     *
     * @param dialogType The {@link ModalDialogType} of the {@link PropertyModel}.
     * @param dialogPriority The {@link ModalDialogPriority} of the {@link PropertyModel}.
     * @param model The {@link PropertyModel} which contains the {@link ModalDialogProperties} to
     *     launch a dialog.
     * @param showAsNext If set to true, the {@link PropertyModel} |model| would be put as first in
     *     its list of dialog.
     */
    void put(
            @ModalDialogType int dialogType,
            @ModalDialogPriority int dialogPriority,
            PropertyModel model,
            boolean showAsNext) {
        Integer key = computeKey(dialogType, dialogPriority);
        List<PropertyModel> dialogs = mPendingDialogs.get(key);
        if (dialogs == null) mPendingDialogs.put(key, dialogs = new ArrayList<>());
        dialogs.add(showAsNext ? 0 : dialogs.size(), model);
    }

    /**
     * @param dialogType The {@link ModalDialogType} of the list of pending dialog to fetch.
     * @param dialogPriority The {@link ModalDialogPriority} of the list of pending dialog to fetch.
     *
     * @return Returns the list of {@link PropertyModel} with matching {@link ModalDialogType} *and*
     *         {@link ModalDialogPriority}, null otherwise.
     */
    @Nullable
    List<PropertyModel> get(
            @ModalDialogType int dialogType, @ModalDialogPriority int dialogPriority) {
        Integer key = computeKey(dialogType, dialogPriority);
        return mPendingDialogs.get(key);
    }

    /**
     * @param model The {@link PropertyModel} model to check if it contains in the list of
     *         pending dialogs.
     *
     * @return Returns true if model found, otherwise false.
     */
    boolean contains(PropertyModel model) {
        for (Map.Entry<Integer, List<PropertyModel>> entry : mPendingDialogs.entrySet()) {
            List<PropertyModel> dialogs = entry.getValue();
            for (int i = 0; i < dialogs.size(); ++i) {
                if (dialogs.get(i) == model) return true;
            }
        }
        return false;
    }

    /**
     * @return True, if there are no pending dialogs, false otherwise.
     */
    boolean isEmpty() {
        for (Map.Entry<Integer, List<PropertyModel>> entry : mPendingDialogs.entrySet()) {
            if (!entry.getValue().isEmpty()) return false;
        }
        return true;
    }

    /**
     * @return Total number of list of pending dialogs.
     */
    int size() {
        return mPendingDialogs.size();
    }

    /**
     * @param model The {@link PropertyModel} model to remove from pending dialogs.
     *
     * @return True, if the |model| was found and removed, false otherwise.
     */
    boolean remove(PropertyModel model) {
        Iterator<Map.Entry<Integer, List<PropertyModel>>> iterator =
                mPendingDialogs.entrySet().iterator();
        while (iterator.hasNext()) {
            Map.Entry<Integer, List<PropertyModel>> entry = iterator.next();
            List<PropertyModel> dialogs = entry.getValue();
            for (int i = 0; i < dialogs.size(); ++i) {
                if (dialogs.get(i) == model) {
                    dialogs.remove(i);
                    if (dialogs.isEmpty()) {
                        mPendingDialogs.remove(entry.getKey());
                    }
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Removes the pending dialogs matching the |dialogType| and applies the operation provided by
     * |consumer| before removing.
     *
     * @param dialogType The {@link ModalDialogType} of the dialog to be removed from the list
     *         of pending dialogs.
     * @param consumer The {@link Consumer <PropertyModel>} which would be performed before
     *         removing the {@link PropertyModel} matching the |dialogType| from the pending
     *         dialog list.
     *
     * @return True, if any dialogs were removed, false otherwise.
     */
    boolean remove(@ModalDialogType int dialogType, Consumer<PropertyModel> consumer) {
        boolean dialogRemoved = false;
        for (@ModalDialogPriority int priority = ModalDialogPriority.RANGE_MIN;
                priority <= ModalDialogPriority.RANGE_MAX;
                ++priority) {
            Integer key = computeKey(dialogType, priority);
            List<PropertyModel> dialogs = mPendingDialogs.get(key);
            // No matching dialogs of type |dialogType| found with |priority|, continue searching
            // with other priorities.
            if (dialogs == null) continue;

            for (int i = 0; i < dialogs.size(); ++i) {
                dialogRemoved = true;
                consumer.accept(dialogs.get(i));
            }
            mPendingDialogs.remove(key);
        }
        return dialogRemoved;
    }

    /**
     * Returns the next dialog whose {@link ModalDialogType} is not suspended from showing and also
     * removes it from the pending dialogs.
     *
     * @return The {@link PropertyModel}, the {@link ModalDialogType} and the {@link
     *         ModalDialogPriority} of the model associated with the next dialog to be shown,
     *         otherwise null if not found.
     */
    @Nullable
    PendingDialogType getNextPendingDialog(Set<Integer> suspendedTypes) {
        for (@ModalDialogPriority int priority = ModalDialogPriority.RANGE_MAX;
                priority >= ModalDialogPriority.RANGE_MIN;
                --priority) {
            for (@ModalDialogType int type = ModalDialogType.RANGE_MAX;
                    type >= ModalDialogType.RANGE_MIN;
                    --type) {
                if (suspendedTypes.contains(type)) continue;
                Integer key = computeKey(type, priority);
                List<PropertyModel> dialogs = mPendingDialogs.get(key);
                if (dialogs != null && !dialogs.isEmpty()) {
                    PropertyModel model = dialogs.get(0);
                    dialogs.remove(0);
                    if (dialogs.isEmpty()) {
                        mPendingDialogs.remove(key);
                    }
                    return new PendingDialogType(model, type, priority);
                }
            }
        }
        return null;
    }

    // A method for computing the key of the mPendingDialog HashMap.
    private Integer computeKey(
            @ModalDialogType int dialogType, @ModalDialogPriority int dialogPriority) {
        assert dialogPriority >= 1 && dialogPriority <= 9;
        return dialogType * 10 + dialogPriority;
    }
}
