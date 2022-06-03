// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.shadows;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.asynclayoutinflater.view.AsyncLayoutInflater;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;

/**
 * Shadow implementation of AsyncLayoutInflater that inflates on the UI thread then posts the
 * callback on the UI thread delayed.
 */
@Implements(AsyncLayoutInflater.class)
public class ShadowAsyncLayoutInflater {
    @Implementation
    public void inflate(@LayoutRes int resid, @Nullable ViewGroup parent,
            @NonNull AsyncLayoutInflater.OnInflateFinishedListener callback) {
        Context context =
                parent != null ? parent.getContext() : ContextUtils.getApplicationContext();
        View inflatedView = LayoutInflater.from(context).inflate(resid, parent, false);
        ThreadUtils.postOnUiThreadDelayed(
                () -> callback.onInflateFinished(inflatedView, inflatedView.getId(), parent), 500);
    }
}
