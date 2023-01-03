// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;

import androidx.annotation.Nullable;

import org.chromium.base.StrictModeContext;

/**
 * {@link LayoutInflater} wrapper class which suppresses strict mode violations. A helper class is
 * used for strict mode suppression instead of
 * {@link org.chromium.components.strictmode.browser.ThreadStrictModeInterceptor.Builder}
 * because we only want to suppress strict mode violations caused by Chromium usage of
 * LayoutInflater and not usage by embedders of Web Layer or Web View.
 */
public class LayoutInflaterUtils {
    public static View inflate(Context context, int resource, @Nullable ViewGroup root) {
        return inflate(context, resource, root, root != null);
    }

    public static View inflate(
            Context context, int resource, @Nullable ViewGroup root, boolean attachToRoot) {
        return inflateImpl(LayoutInflater.from(context), resource, root, attachToRoot);
    }

    public static View inflate(Window window, int resource, @Nullable ViewGroup root) {
        return inflate(window, resource, root, root != null);
    }

    public static View inflate(
            Window window, int resource, @Nullable ViewGroup root, boolean attachToRoot) {
        return inflateImpl(window.getLayoutInflater(), resource, root, attachToRoot);
    }

    public static View inflate(
            LayoutInflater layoutInflater, int resource, @Nullable ViewGroup root) {
        return inflateImpl(layoutInflater, resource, root, root != null);
    }

    private static View inflateImpl(
            LayoutInflater inflater, int resource, ViewGroup root, boolean attachToRoot) {
        // LayoutInflater may trigger accessing disk.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return inflater.inflate(resource, root, attachToRoot);
        }
    }
}
