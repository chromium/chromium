// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.theme.FillInContextThemeWrapper;

/** Helper class that inflates view from XML layout. */
@NullMarked
public class LayoutViewBuilder<T extends View> implements ViewBuilder<T> {
    @LayoutRes private final int mLayoutResId;
    private @Nullable LayoutInflater mInflater;

    public LayoutViewBuilder(@LayoutRes int res) {
        mLayoutResId = res;
    }

    /**
     * Inflate a new view from resource id passed to the constructor. Uses parent view to also
     * supply correct LayoutParams to newly constructed view.
     *
     * @param parent Parent view.
     * @return Newly inflated view.
     */
    @Override
    public final T buildView(ViewGroup parent) {
        if (mInflater == null) {
            var wrappedContext =
                    new FillInContextThemeWrapper(
                            parent.getContext(), R.style.ThemeOverlay_UI_AdaptiveDensityDefaults);
            mInflater = LayoutInflater.from(wrappedContext);
        }

        T view = (T) mInflater.inflate(mLayoutResId, parent, false);
        return postInflationInit(view);
    }

    /**
     * Allows for additional post processing of the view following inflation.
     *
     * @param view The view to be initialized.
     * @return The view with post inflation processing completed.
     */
    protected T postInflationInit(T view) {
        return view;
    }
}
