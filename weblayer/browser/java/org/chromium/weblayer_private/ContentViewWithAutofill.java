// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.os.Build;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventOffsetHandler;

/**
 * API level 26 implementation that includes autofill.
 */
public class ContentViewWithAutofill extends ContentView.ContentViewApi23 {
    public static ContentView createContentView(
            Context context, EventOffsetHandler eventOffsetHandler) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return new ContentViewWithAutofill(context, eventOffsetHandler);
        }
        return ContentView.createContentView(context, eventOffsetHandler, null /* webContents */);
    }

    private TabImpl mTab;

    private ContentViewWithAutofill(Context context, EventOffsetHandler eventOffsetHandler) {
        super(context, eventOffsetHandler, null /* webContents */);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // The Autofill system-level infrastructure has heuristics for which Views it considers
            // important for autofill; only these Views will be queried for their autofill
            // structure on notifications that a new (virtual) View was entered. By default,
            // FrameLayout is not considered important for autofill. Thus, for ContentView to be
            // queried for its autofill structure, we must explicitly inform the autofill system
            // that this View is important for autofill.
            setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_YES);
        }
    }

    @Override
    public void setWebContents(WebContents webContents) {
        mTab = TabImpl.fromWebContents(webContents);
        super.setWebContents(webContents);
    }

    @Override
    public void onProvideAutofillVirtualStructure(ViewStructure structure, int flags) {
        // A new (virtual) View has been entered, and the autofill system-level
        // infrastructure wants us to populate |structure| with the autofill structure of the
        // (virtual) View. Forward this on to TabImpl to accomplish.
        if (mTab != null) {
            mTab.onProvideAutofillVirtualStructure(structure, flags);
        }
    }

    @Override
    public void autofill(final SparseArray<AutofillValue> values) {
        // The autofill system-level infrastructure has information that we can use to
        // autofill the current (virtual) View. Forward this on to TabImpl to
        // accomplish.
        if (mTab != null) {
            mTab.autofill(values);
        }
    }
}
