// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.view.View;

import androidx.annotation.DimenRes;
import androidx.annotation.Px;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding the list section divider. This is primarily needed to enable
 * customization of the list section divider.
 */
public class ListSectionDividerViewBinder {

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ListSectionDividerProperties.LEFT_PADDING_DIMEN_ID) {
            final @DimenRes int leftPaddingId =
                    model.get(ListSectionDividerProperties.LEFT_PADDING_DIMEN_ID);
            if (leftPaddingId != 0) {
                final @Px int leftPaddingPx =
                        view.getContext().getResources().getDimensionPixelSize(leftPaddingId);
                view.setPadding(
                        leftPaddingPx,
                        view.getPaddingTop(),
                        view.getPaddingRight(),
                        view.getPaddingBottom());
            }
        } else if (propertyKey == ListSectionDividerProperties.RIGHT_PADDING_DIMEN_ID) {
            final @DimenRes int rightPaddingId =
                    model.get(ListSectionDividerProperties.RIGHT_PADDING_DIMEN_ID);
            if (rightPaddingId != 0) {
                final @Px int rightPaddingPx =
                        view.getContext().getResources().getDimensionPixelSize(rightPaddingId);
                view.setPadding(
                        view.getPaddingLeft(),
                        view.getPaddingTop(),
                        rightPaddingPx,
                        view.getPaddingBottom());
            }
        }
    }
}
