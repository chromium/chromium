// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
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

        ClipData clipdata = buildClipData(dropData);
        if (clipdata == null) {
            return false;
        }
        return ApiHelperForN.startDragAndDrop(
                view, clipdata, new View.DragShadowBuilder(imageView), null, buildFlags(dropData));
    }

    protected ClipData buildClipData(DropDataAndroid dropData) {
        if (dropData.isPlainText()) {
            return ClipData.newPlainText(null, dropData.text);
        } else if (dropData.hasImage()) {
            Uri uri = DropDataContentProvider.cache(
                    dropData.imageContent, dropData.imageContentExtension);
            return ClipData.newUri(
                    ContextUtils.getApplicationContext().getContentResolver(), null, uri);
            // TODO: ensure MIME type of ClipData is correct
        } else {
            // TODO(crbug.com/1289393): handle link dragging
            return null;
        }
    }

    @RequiresApi(api = VERSION_CODES.N)
    protected int buildFlags(DropDataAndroid dropData) {
        if (dropData.isPlainText()) {
            return View.DRAG_FLAG_GLOBAL;
        } else if (dropData.hasImage()) {
            return View.DRAG_FLAG_GLOBAL | View.DRAG_FLAG_GLOBAL_URI_READ;
        } else {
            return 0;
        }
    }
}