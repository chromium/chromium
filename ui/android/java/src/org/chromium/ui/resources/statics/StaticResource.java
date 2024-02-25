// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.statics;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.ResourceFactory;

/**
 * A representation of a static resource and all related information for drawing it.  In general
 * this means a {@link Bitmap} and a potential {@link NinePatchData}.
 */
public class StaticResource implements Resource {
    private Bitmap mBitmap;
    private final NinePatchData mNinePatchData;
    private final Rect mBitmapSize;

    /**
     * Creates a {@link StaticResource} that represents {@code bitmap}.  This will automatically
     * pull out the {@link NinePatchData} from {@code bitmap} if it exists.
     * @param bitmap The {@link Bitmap} to build a {@link StaticResource} of.
     */
    public StaticResource(Bitmap bitmap) {
        mBitmap = bitmap;
        mNinePatchData = NinePatchData.create(mBitmap);
        mBitmapSize = new Rect(0, 0, mBitmap.getWidth(), mBitmap.getHeight());
    }

    @Override
    public NinePatchData getNinePatchData() {
        return mNinePatchData;
    }

    @Override
    public Bitmap getBitmap() {
        assert mBitmap != null : "StaticResource#getBitmap can only be called once per lifecycle";
        Bitmap bitmap = mBitmap;
        mBitmap = null;
        return bitmap;
    }

    @Override
    public boolean shouldRemoveResourceOnNullBitmap() {
        return false;
    }

    @Override
    public Rect getBitmapSize() {
        return mBitmapSize;
    }

    @Override
    public long createNativeResource() {
        return ResourceFactory.createBitmapResource(mNinePatchData);
    }

    /**
     * Attempts to load the Android resource specified by {@code resId} from {@code resources}. This
     * will attempt to first load the resource as a {@code Bitmap}. If that fails it will try to
     * load the resource as a {@link Drawable}.
     *
     * @param resources The {@link Resources} instance to load from.
     * @param resId The id of the Android resource to load.
     * @param fitWidth The smallest width the image can be. The image will be shrunk to scale to try
     *     to get close to this value. Or use {@code 0} to use the intrinsic size.
     * @param fitHeight The smallest height the image can be. The image will be shrunk to scale to
     *     try to get close to this value. Or use {@code 0} to use the intrinsic size.
     * @return The loaded {@link StaticResource} or {@code null} if the resource could not be
     *     loaded.
     */
    public static StaticResource create(
            Resources resources, int resId, int fitWidth, int fitHeight) {
        if (resId <= 0) return null;
        Bitmap bitmap = decodeBitmap(resources, resId, fitWidth, fitHeight);
        if (bitmap == null) bitmap = decodeDrawable(resources, resId, fitWidth, fitHeight);
        if (bitmap == null) return null;

        return new StaticResource(bitmap);
    }

    private static Bitmap decodeBitmap(
            Resources resources, int resId, int fitWidth, int fitHeight) {
        BitmapFactory.Options options = createOptions(resources, resId, fitWidth, fitHeight);
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        Bitmap bitmap = BitmapFactory.decodeResource(resources, resId, options);

        if (bitmap == null) return null;
        if (bitmap.getConfig() == options.inPreferredConfig) return bitmap;

        Bitmap convertedBitmap =
                Bitmap.createBitmap(
                        bitmap.getWidth(), bitmap.getHeight(), options.inPreferredConfig);
        Canvas canvas = new Canvas(convertedBitmap);
        canvas.drawBitmap(bitmap, 0, 0, null);
        bitmap.recycle();
        return convertedBitmap;
    }

    private static Bitmap decodeDrawable(
            Resources resources, int resId, int fitWidth, int fitHeight) {
        try {
            Drawable drawable = ApiCompatibilityUtils.getDrawable(resources, resId);
            int width = Math.max(drawable.getMinimumWidth(), Math.max(fitWidth, 1));
            int height = Math.max(drawable.getMinimumHeight(), Math.max(fitHeight, 1));
            Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            drawable.setBounds(0, 0, width, height);
            drawable.draw(canvas);
            return bitmap;
        } catch (Resources.NotFoundException ex) {
            return null;
        }
    }

    private static BitmapFactory.Options createOptions(
            Resources resources, int resId, int fitWidth, int fitHeight) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        if (fitWidth == 0 || fitHeight == 0) return options;

        options.inJustDecodeBounds = true;
        BitmapFactory.decodeResource(resources, resId, options);
        options.inJustDecodeBounds = false;

        if (options.outHeight <= fitHeight && options.outWidth <= fitWidth) return options;

        int heightRatio = Math.round((float) options.outHeight / (float) fitHeight);
        int widthRatio = Math.round((float) options.outWidth / (float) fitWidth);
        options.inSampleSize = Math.min(heightRatio, widthRatio);

        return options;
    }
}
