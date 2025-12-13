// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.theme;

import android.content.Context;
import android.content.res.Resources;
import android.view.ContextThemeWrapper;

import androidx.annotation.StyleRes;

import org.chromium.build.annotations.NullMarked;

/**
 * A {@link ContextThemeWrapper} that fills in missing attributes of the base context's theme from
 * the provided theme. It does not override or replace any attribute that already exists in the base
 * context's theme.
 *
 * <p>For example, you can use this class to provide default values for attributes that might be
 * missing.
 */
@NullMarked
public class FillInContextThemeWrapper extends ContextThemeWrapper {

    /**
     * Creates a new instance with the specified theme.
     *
     * @param base the base context.
     * @param themeResId the resource id of the theme to be applied.
     */
    public FillInContextThemeWrapper(Context base, @StyleRes int themeResId) {
        super(base, themeResId);
    }

    @Override
    protected void onApplyThemeResource(Resources.Theme theme, int resId, boolean first) {
        theme.applyStyle(resId, /* force= */ false);
    }
}
