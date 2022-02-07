// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.graphics.Bitmap;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import org.chromium.base.compat.ApiHelperForN;

/**
 * Drag and drop helper class in charge of building the clip data, wrapping calls to
 * {@link android.view.View#startDragAndDrop}. Also used for mocking out real function calls to
 * Android.
 */
class DragAndDropDelegateImpl implements ViewAndroidDelegate.DragAndDropDelegate {
    /**
     * Wrapper to call {@link android.view.View#startDragAndDrop}.
     * @see ViewAndroidDelegate#startDragAndDrop(Bitmap, DropDataAndroid)
     * */
    @Override
    @RequiresApi(api = VERSION_CODES.N)
    public boolean startDragAndDrop(
            @NonNull View view, @NonNull Bitmap shadowImage, @NonNull DropDataAndroid dropData) {
        ImageView imageView = new ImageView(view.getContext());
        imageView.setImageBitmap(shadowImage);
        imageView.layout(0, 0, shadowImage.getWidth(), shadowImage.getHeight());

        return ApiHelperForN.startDragAndDrop(view, buildClipData(dropData),
                new View.DragShadowBuilder(imageView), null, View.DRAG_FLAG_GLOBAL);
    }

    protected ClipData buildClipData(DropDataAndroid dropData) {
        return ClipData.newPlainText(null, dropData.text);
    }
}
