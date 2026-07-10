// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

@NullMarked
/**
 * Provider interface to display a Material Date Picker dialog. This interface is implemented in
 * higher layers and registered via ServiceLoader.
 */
public interface MaterialDatePickerProvider {
    /**
     * Shows a Material Date Picker dialog.
     *
     * @param context The Android context to launch the dialog from.
     * @param minTime The minimum selectable date in UTC milliseconds since epoch.
     * @param maxTime The maximum selectable date in UTC milliseconds since epoch.
     * @param selectionCallback The callback invoked when the user selects a date.
     * @param onDismiss The callback invoked when the dialog is dismissed or canceled.
     * @return Whether the Material Date Picker dialog was successfully shown.
     */
    boolean showDatePicker(
            Context context,
            long minTime,
            long maxTime,
            Callback<Long> selectionCallback,
            Runnable onDismiss);
}
