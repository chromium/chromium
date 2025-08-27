// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.theme;

import android.content.Context;
import android.content.ContextWrapper;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;

/** Interface that owns a ThemeResourceWrapper, and allow users to attach observers to it. */
@NullMarked
public interface ThemeResourceWrapperProvider {
    /**
     * Return if this instance can change its theme resource during run time. For example, if the
     * underlying ThemeResourceWrapper is null, then this instance cannot change the theme resource.
     */
    boolean hasThemeResourceWrapper();

    /**
     * Attach an observer to the ThemeResourceWrapper.
     *
     * @param observer The observer to attach.
     */
    void attachThemeObserver(ThemeResourceWrapper.ThemeObserver observer);

    /**
     * Detach an observer from the ThemeResourceWrapper.
     *
     * @param observer The observer to detach.
     */
    void detachThemeObserver(ThemeResourceWrapper.ThemeObserver observer);

    /**
     * Get the ThemeResourceWrapperProvider from the given context.
     *
     * @param context The context to get the ThemeResourceWrapperProvider from.
     * @return The ThemeResourceWrapperProvider if found, otherwise null.
     */
    static @Nullable ThemeResourceWrapperProvider getFromContext(Context context) {
        while (context instanceof ContextWrapper) {
            if (context instanceof ThemeResourceWrapperProvider) {
                return (ThemeResourceWrapperProvider) context;
            }

            context = ((ContextWrapper) context).getBaseContext();
        }
        return null;
    }
}
