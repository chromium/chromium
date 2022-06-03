// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;

import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;

/**
 * Helper class that inflates view from XML layout.
 */
public class LayoutViewBuilder<T extends View> implements ViewBuilder<T> {
    @LayoutRes
    private final int mLayoutResId;
    private LayoutInflater mInflater;

    public LayoutViewBuilder(@LayoutRes int res) {
        mLayoutResId = res;
    }

    /**
     * Inflate a new view from resource id passed to the constructor.
     * Uses parent view to also supply correct LayoutParams to newly constructed view.
     *
     * @param parent Parent view.
     * @return Newly inflated view.
     */
    @Override
    public final T buildView(ViewGroup parent) {
        if (mInflater == null) {
            mInflater = LayoutInflater.from(parent.getContext());
        }

        T view = (T) mInflater.inflate(mLayoutResId, parent, false);
        return view;
    }
}
