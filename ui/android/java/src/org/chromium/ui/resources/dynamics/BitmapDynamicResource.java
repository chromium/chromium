// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.ResourceFactory;

/**
 * A basic implementation of {@link DynamicResource} to handle updatable bitmaps.
 */
public class BitmapDynamicResource implements DynamicResource {
    private final int mResId;
    private Bitmap mBitmap;
    private final Rect mSize = new Rect();

    @Nullable
    private Callback<Resource> mOnResourceReady;

    public BitmapDynamicResource(int resourceId) {
        mResId = resourceId;
    }

    /**
     * @return A unique id for this resource.
     */
    public int getResId() {
        return mResId;
    }

    /**
     * @param bitmap A bitmap to update this resource.
     */
    public void setBitmap(Bitmap bitmap) {
        // Not updating bitmap is still bad, but better than a crash. We will still crash if there
        // is no bitmap to start with. See http://crbug.com/471234 for more.
        if (bitmap == null) return;
        mBitmap = bitmap;
        mSize.set(0, 0, mBitmap.getWidth(), mBitmap.getHeight());
    }

    @Override
    public void onResourceRequested() {
        if (mOnResourceReady != null && mBitmap != null) {
            Resource resource = new DynamicResourceSnapshot(
                    mBitmap, false, mSize, ResourceFactory.createBitmapResource(null));
            mOnResourceReady.onResult(resource);
            mBitmap = null;
        }
    }

    @Override
    public void setOnResourceReadyCallback(Callback<Resource> onResourceReady) {
        mOnResourceReady = onResourceReady;
    }
}
