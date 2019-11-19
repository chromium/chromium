// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Bitmap;
import android.graphics.Rect;

import org.chromium.ui.resources.ResourceFactory;
import org.chromium.ui.resources.statics.NinePatchData;

/**
 * A basic implementation of {@link DynamicResource} to handle updatable bitmaps.
 */
public class BitmapDynamicResource extends DynamicResource {
    private final int mResId;
    private Bitmap mBitmap;
    private final Rect mSize = new Rect();
    private boolean mIsDirty = true;

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
        mIsDirty = true;
        mBitmap = bitmap;
        mSize.set(0, 0, mBitmap.getWidth(), mBitmap.getHeight());
    }

    @Override
    public Bitmap getBitmap() {
        super.getBitmap();
        assert mBitmap != null: "setBitmap() should be called before calling getBitmap() again";
        mIsDirty = false;
        Bitmap bitmap = mBitmap;
        mBitmap = null;
        return bitmap;
    }

    @Override
    public Rect getBitmapSize() {
        return mSize;
    }

    @Override
    public long createNativeResource() {
        return ResourceFactory.createBitmapResource(null);
    }

    @Override
    public NinePatchData getNinePatchData() {
        return null;
    }

    @Override
    public boolean isDirty() {
        return mIsDirty;
    }
}
