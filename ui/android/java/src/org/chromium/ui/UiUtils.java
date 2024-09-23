// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Environment;
import android.os.StrictMode;
import android.text.TextUtils;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;
import android.widget.AbsListView;
import android.widget.ListAdapter;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StyleableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.Insets;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Utility functions for common Android UI tasks.
 * This class is not supposed to be instantiated.
 */
public class UiUtils {
    private static final String TAG = "UiUtils";

    public static final String EXTERNAL_IMAGE_FILE_PATH = "browser-images";
    // Keep this variable in sync with the value defined in file_paths.xml.
    public static final String IMAGE_FILE_PATH = "images";

    // crbug.com/1413586: Prevent potentially unintentional user interaction with any prompt for
    // this long after the prompt is displayed.
    public static long PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS = 600;

    /**
     * A static map of manufacturers to the version where theming Android UI is completely
     * supported. If there is no entry, it means the manufacturer supports theming at the same
     * version Android did.
     */
    private static final Map<String, Integer> sAndroidUiThemeBlocklist = new HashMap<>();

    static {
        // HTC doesn't respect theming flags on activity restart until Android O; this affects both
        // the system nav and status bar. More info at https://crbug.com/831737.
        sAndroidUiThemeBlocklist.put("htc", Build.VERSION_CODES.O);
    }

    /** Whether theming the Android system UI has been disabled. */
    private static Boolean sSystemUiThemingDisabled;

    /** Guards this class from being instantiated. */
    private UiUtils() {}

    /**
     * Gets the set of locales supported by the current enabled Input Methods.
     * @param context A {@link Context} instance.
     * @return A possibly-empty {@link Set} of locale strings.
     */
    public static Set<String> getIMELocales(Context context) {
        LinkedHashSet<String> locales = new LinkedHashSet<String>();
        InputMethodManager imManager =
                (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
        List<InputMethodInfo> enabledMethods = imManager.getEnabledInputMethodList();
        for (int i = 0; i < enabledMethods.size(); i++) {
            List<InputMethodSubtype> subtypes =
                    imManager.getEnabledInputMethodSubtypeList(enabledMethods.get(i), true);
            if (subtypes == null) continue;
            for (int j = 0; j < subtypes.size(); j++) {
                String locale = subtypes.get(j).getLanguageTag();
                if (!TextUtils.isEmpty(locale)) locales.add(locale);
            }
        }
        return locales;
    }

    /**
     * Inserts a {@link View} into a {@link ViewGroup} after directly before a given {@View}.
     * @param container The {@link View} to add newView to.
     * @param newView The new {@link View} to add.
     * @param existingView The {@link View} to insert the newView before.
     * @return The index where newView was inserted, or -1 if it was not inserted.
     */
    public static int insertBefore(ViewGroup container, View newView, View existingView) {
        return insertView(container, newView, existingView, false);
    }

    /**
     * Inserts a {@link View} into a {@link ViewGroup} after directly after a given {@View}.
     * @param container The {@link View} to add newView to.
     * @param newView The new {@link View} to add.
     * @param existingView The {@link View} to insert the newView after.
     * @return The index where newView was inserted, or -1 if it was not inserted.
     */
    public static int insertAfter(ViewGroup container, View newView, View existingView) {
        return insertView(container, newView, existingView, true);
    }

    private static int insertView(
            ViewGroup container, View newView, View existingView, boolean after) {
        // See if the view has already been added.
        int index = container.indexOfChild(newView);
        if (index >= 0) return index;

        // Find the location of the existing view.
        index = container.indexOfChild(existingView);
        if (index < 0) return -1;

        // Add the view.
        if (after) index++;
        container.addView(newView, index);
        return index;
    }

    /**
     * Generates a scaled screenshot of the given view.  The maximum size of the screenshot is
     * determined by maximumDimension.
     *
     * @param currentView      The view to generate a screenshot of.
     * @param maximumDimension The maximum width or height of the generated screenshot.  The bitmap
     *                         will be scaled to ensure the maximum width or height is equal to or
     *                         less than this.  Any value <= 0, will result in no scaling.
     * @param bitmapConfig     Bitmap config for the generated screenshot (ARGB_8888 or RGB_565).
     * @return The screen bitmap of the view or null if a problem was encountered.
     */
    public static Bitmap generateScaledScreenshot(
            View currentView, int maximumDimension, Bitmap.Config bitmapConfig) {
        Bitmap screenshot = null;
        boolean drawingCacheEnabled = currentView.isDrawingCacheEnabled();
        try {
            prepareViewHierarchyForScreenshot(currentView, true);
            if (!drawingCacheEnabled) currentView.setDrawingCacheEnabled(true);
            // Android has a maximum drawing cache size and if the drawing cache is bigger
            // than that, getDrawingCache() returns null.
            Bitmap originalBitmap = currentView.getDrawingCache();
            if (originalBitmap != null) {
                double originalHeight = originalBitmap.getHeight();
                double originalWidth = originalBitmap.getWidth();
                int newWidth = (int) originalWidth;
                int newHeight = (int) originalHeight;
                if (maximumDimension > 0) {
                    double scale = maximumDimension / Math.max(originalWidth, originalHeight);
                    newWidth = (int) Math.round(originalWidth * scale);
                    newHeight = (int) Math.round(originalHeight * scale);
                }
                Bitmap scaledScreenshot =
                        Bitmap.createScaledBitmap(originalBitmap, newWidth, newHeight, true);
                if (scaledScreenshot.getConfig() != bitmapConfig) {
                    screenshot = scaledScreenshot.copy(bitmapConfig, false);
                    scaledScreenshot.recycle();
                    scaledScreenshot = null;
                } else {
                    screenshot = scaledScreenshot;
                }
            } else if (currentView.getMeasuredHeight() > 0 && currentView.getMeasuredWidth() > 0) {
                double originalHeight = currentView.getMeasuredHeight();
                double originalWidth = currentView.getMeasuredWidth();
                int newWidth = (int) originalWidth;
                int newHeight = (int) originalHeight;
                if (maximumDimension > 0) {
                    double scale = maximumDimension / Math.max(originalWidth, originalHeight);
                    newWidth = (int) Math.round(originalWidth * scale);
                    newHeight = (int) Math.round(originalHeight * scale);
                }
                Bitmap bitmap = Bitmap.createBitmap(newWidth, newHeight, bitmapConfig);
                Canvas canvas = new Canvas(bitmap);
                canvas.scale(
                        (float) (newWidth / originalWidth), (float) (newHeight / originalHeight));
                currentView.draw(canvas);
                screenshot = bitmap;
            }
        } catch (OutOfMemoryError e) {
            Log.d(TAG, "Unable to capture screenshot and scale it down." + e.getMessage());
        } finally {
            if (!drawingCacheEnabled) currentView.setDrawingCacheEnabled(false);
            prepareViewHierarchyForScreenshot(currentView, false);
        }
        return screenshot;
    }

    private static void prepareViewHierarchyForScreenshot(View view, boolean takingScreenshot) {
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                prepareViewHierarchyForScreenshot(viewGroup.getChildAt(i), takingScreenshot);
            }
        } else if (view instanceof SurfaceView) {
            view.setWillNotDraw(!takingScreenshot);
        }
    }

    /**
     * Get a directory for the image capture operation. For devices with JB MR2
     * or latter android versions, the directory is IMAGE_FILE_PATH directory.
     * For ICS devices, the directory is CAPTURE_IMAGE_DIRECTORY.
     *
     * @param context The application context.
     * @return directory for the captured image to be stored.
     */
    public static File getDirectoryForImageCapture(Context context) throws IOException {
        // Temporarily allowing disk access while fixing. TODO: http://crbug.com/562173
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            File path;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                path = new File(context.getFilesDir(), IMAGE_FILE_PATH);
                if (!path.exists() && !path.mkdir()) {
                    throw new IOException("Folder cannot be created.");
                }
            } else {
                File externalDataDir =
                        Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM);
                path =
                        new File(
                                externalDataDir.getAbsolutePath()
                                        + File.separator
                                        + EXTERNAL_IMAGE_FILE_PATH);
                if (!path.exists() && !path.mkdirs()) {
                    path = externalDataDir;
                }
            }
            return path;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Removes the view from its parent {@link ViewGroup}. No-op if the {@link View} is not yet
     * attached to the view hierarchy.
     *
     * @param view The view to be removed from the parent.
     */
    public static void removeViewFromParent(View view) {
        ViewGroup parent = (ViewGroup) view.getParent();
        if (parent == null) return;
        parent.removeView(view);
    }

    /**
     * Computes the max width of the widest list item & the total height of all of the items. The
     * height returned in unbounded and may be larger than the available window space.
     *
     * <p>WARNING: do not call this on a ListAdapter with more than a handful of items, the
     * performance will be terrible since it measures every single item.
     *
     * @param adapter The adapter for the list.
     * @param parentView The parent view for the list.
     * @return int array representing the max width of the menu items stored at index 0 & the total
     *     height of all items stored at index 1.
     */
    public static int[] computeListAdapterContentDimensions(
            ListAdapter adapter, ViewGroup parentView) {
        final int widthMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        final int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        AbsListView.LayoutParams params =
                new AbsListView.LayoutParams(
                        AbsListView.LayoutParams.WRAP_CONTENT,
                        AbsListView.LayoutParams.WRAP_CONTENT);

        int[] result = new int[] {0, 0};
        View[] itemViews = new View[adapter.getViewTypeCount()];
        for (int i = 0; i < adapter.getCount(); ++i) {
            View itemView;
            int type = adapter.getItemViewType(i);
            if (type < 0) {
                // Type is negative for header/footer views, or views the adapter does not want
                // recycled.
                itemView = adapter.getView(i, null, parentView);
            } else {
                itemViews[type] = adapter.getView(i, itemViews[type], parentView);
                itemView = itemViews[type];
            }

            itemView.setLayoutParams(params);
            itemView.measure(widthMeasureSpec, heightMeasureSpec);
            result[0] = Math.max(result[0], itemView.getMeasuredWidth());
            result[1] += itemView.getMeasuredHeight();
        }

        return result;
    }

    /**
     * Get the index of a child {@link View} in a {@link ViewGroup}.
     * @param child The child to find the index of.
     * @return The index of the child in its parent. -1 if the child has no parent.
     */
    public static int getChildIndexInParent(View child) {
        if (child.getParent() == null) return -1;
        ViewGroup parent = (ViewGroup) child.getParent();
        int indexInParent = -1;
        for (int i = 0; i < parent.getChildCount(); i++) {
            if (parent.getChildAt(i) == child) {
                indexInParent = i;
                break;
            }
        }
        return indexInParent;
    }

    /**
     * Loads a {@link Drawable} from an attribute.  Uses {@link AppCompatResources} to support all
     * modern {@link Drawable} types.
     * @param context The associated context.
     * @param attrs The attributes from which to load the drawable resource.
     * @param attrId The attribute id that holds the drawable resource.
     * @return A new {@link Drawable} or {@code null} if the attribute wasn't set.
     */
    public static @Nullable Drawable getDrawable(
            Context context, @Nullable TypedArray attrs, @StyleableRes int attrId) {
        if (attrs == null) return null;

        @DrawableRes int resId = attrs.getResourceId(attrId, -1);
        if (resId == -1) return null;
        return AppCompatResources.getDrawable(context, resId);
    }

    /**
     * Gets a drawable from the resources and applies the specified tint to it. Uses Support Library
     * for vector drawables and tinting on older Android versions.
     * @param drawableId The resource id for the drawable.
     * @param tintColorId The resource id for the color to build ColorStateList with.
     */
    public static Drawable getTintedDrawable(
            Context context, @DrawableRes int drawableId, @ColorRes int tintColorId) {
        return getTintedDrawable(
                context, drawableId, AppCompatResources.getColorStateList(context, tintColorId));
    }

    /**
     * Gets a drawable from the resources and applies the specified tint to it. Uses Support Library
     * for vector drawables and tinting on older Android versions.
     * @param drawableId The resource id for the drawable.
     * @param colorStateList The color state list to apply to the drawable.
     */
    public static Drawable getTintedDrawable(
            Context context, @DrawableRes int drawableId, ColorStateList list) {
        Drawable drawable = AppCompatResources.getDrawable(context, drawableId);
        assert drawable != null;
        drawable = DrawableCompat.wrap(drawable).mutate();
        DrawableCompat.setTintList(drawable, list);
        return drawable;
    }

    /**
     * @return Whether the support for theming on a particular device has been completely disabled
     *         due to lack of support by the OEM.
     */
    public static boolean isSystemUiThemingDisabled() {
        if (sSystemUiThemingDisabled == null) {
            sSystemUiThemingDisabled = false;
            if (sAndroidUiThemeBlocklist.containsKey(Build.MANUFACTURER.toLowerCase(Locale.US))) {
                sSystemUiThemingDisabled =
                        Build.VERSION.SDK_INT
                                < sAndroidUiThemeBlocklist.get(
                                        Build.MANUFACTURER.toLowerCase(Locale.US));
            }
        }
        return sSystemUiThemingDisabled;
    }

    /**
     * Sets the navigation bar icons to dark or light. Note that this is only valid for Android
     * O+.
     * @param rootView The root view used to request updates to the system UI theme.
     * @param useDarkIcons Whether the navigation bar icons should be dark.
     */
    public static void setNavigationBarIconColor(View rootView, boolean useDarkIcons) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        int systemUiVisibility = rootView.getSystemUiVisibility();
        if (useDarkIcons) {
            systemUiVisibility |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        } else {
            systemUiVisibility &= ~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        }
        rootView.setSystemUiVisibility(systemUiVisibility);
    }

    /**
     * @see android.view.Window#setStatusBarColor(int color).
     */
    public static void setStatusBarColor(Window window, @ColorInt int statusBarColor) {
        if (0
                == (window.getAttributes().flags
                        & WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS)) {
            window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        }
        // The status bar should always be black in automotive devices to match the black back
        // button toolbar.
        if (BuildInfo.getInstance().isAutomotive) {
            window.setStatusBarColor(Color.BLACK);
        } else {
            window.setStatusBarColor(statusBarColor);
        }
    }

    /**
     * Sets the status bar icons to dark or light. Note that this is only valid for
     * Android M+.
     *
     * TODO: migrate to WindowInsetsController API for Android R+ (API 30+)
     *
     * @param rootView The root view used to request updates to the system UI theming.
     * @param useDarkIcons Whether the status bar icons should be dark.
     */
    public static void setStatusBarIconColor(View rootView, boolean useDarkIcons) {
        int systemUiVisibility = rootView.getSystemUiVisibility();
        // The status bar should always be black in automotive devices to match the black back
        // button toolbar, so we should not use dark icons.
        if (useDarkIcons && !BuildInfo.getInstance().isAutomotive) {
            systemUiVisibility |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        } else {
            systemUiVisibility &= ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        }
        rootView.setSystemUiVisibility(systemUiVisibility);
    }

    /**
     * @return True if a hardware keyboard is detected.
     */
    public static boolean isHardwareKeyboardAttached() {
        return ContextUtils.getApplicationContext().getResources().getConfiguration().keyboard
                != Configuration.KEYBOARD_NOKEYS;
    }

    /**
     * @param window The application window which includes the decor view.
     * @return True if gesture navigation mode is on.
     */
    public static boolean isGestureNavigationMode(Window window) {
        // https://stackoverflow.com/a/70514883
        WindowInsetsCompat windowInsets =
                WindowInsetsCompat.toWindowInsetsCompat(
                        window.getDecorView().getRootWindowInsets());
        // Use systemGestures rather than tappableElements.
        // In some devices, like Samsung Fold, which has a dock, the bottom inset of
        // tappableElements is non-zero even when gesture mode is on.
        Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemGestures());
        return insets.left > 0;
    }

    /**
     * Draws a badge (a {@code badgeColorResId} colored dot) on icon.
     *
     * @param context The activity context.
     * @param icon The icon to draw the badge on.
     * @param iconColorResId The resource id of the color to color the icon with.
     * @param badgeRadius The size of the badge to be drawn (resource id).
     * @param badgeBorderSize The size of the transparent border that will surround the badge
     *     (resource id).
     * @param badgeColorResId The resource id of the color of the badge to be drawn.
     * @return A new drawable that portrays a badge on the passed icon.
     */
    public static Drawable drawIconWithBadge(
            Context context,
            Drawable icon,
            @ColorRes int iconColorResId,
            @DimenRes int badgeSizeResId,
            @DimenRes int badgeBorderSizeResId,
            @ColorRes int badgeColorResId) {
        if (icon == null || icon.getIntrinsicWidth() <= 0 || icon.getIntrinsicHeight() <= 0) {
            return icon;
        }

        int width = icon.getIntrinsicWidth();
        int height = icon.getIntrinsicHeight();

        // Create new drawable.
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);

        Canvas canvas = new Canvas(bitmap);
        icon.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        icon.draw(canvas);

        // Color the icon.
        canvas.drawColor(context.getColor(iconColorResId), PorterDuff.Mode.SRC_IN);

        int badgeRadius = context.getResources().getDimensionPixelSize(badgeSizeResId) / 2;
        int badgeCenterX = width - badgeRadius;
        int badgeCenterY = height / 2 - badgeRadius;

        // Cut a transparent hole through the background icon. This will serve as a border to
        // the badge being overlaid.
        Paint hole = new Paint();
        hole.setAntiAlias(true);
        hole.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        canvas.drawCircle(
                badgeCenterX,
                badgeCenterY,
                badgeRadius + context.getResources().getDimensionPixelSize(badgeBorderSizeResId),
                hole);

        // Draw the red badge.
        Paint badge = new Paint();
        hole.setAntiAlias(true);
        badge.setColor(context.getColor(badgeColorResId));
        canvas.drawCircle(badgeCenterX, badgeCenterY, badgeRadius, badge);

        return new BitmapDrawable(context.getResources(), bitmap);
    }
}
