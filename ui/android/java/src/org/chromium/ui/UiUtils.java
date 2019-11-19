// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Environment;
import android.os.StrictMode;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatDelegate;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;
import android.widget.AbsListView;
import android.widget.ListAdapter;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;

import org.chromium.base.ApiCompatibilityUtils;
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

    /**
     * A static map of manufacturers to the version where theming Android UI is completely
     * supported. If there is no entry, it means the manufacturer supports theming at the same
     * version Android did.
     */
    private static final Map<String, Integer> sAndroidUiThemeBlacklist = new HashMap<>();
    static {
        // Xiaomi doesn't support SYSTEM_UI_FLAG_LIGHT_STATUS_BAR until Android N; more info at
        // https://crbug.com/823264.
        sAndroidUiThemeBlacklist.put("xiaomi", Build.VERSION_CODES.N);
        // HTC doesn't respect theming flags on activity restart until Android O; this affects both
        // the system nav and status bar. More info at https://crbug.com/831737.
        sAndroidUiThemeBlacklist.put("htc", Build.VERSION_CODES.O);
    }

    /** Whether theming the Android system UI has been disabled. */
    private static Boolean sSystemUiThemingDisabled;

    /**
     * Guards this class from being instantiated.
     */
    private UiUtils() {
    }

    /** A delegate for the photo picker. */
    private static PhotoPickerDelegate sPhotoPickerDelegate;

    /** A delegate for the contacts picker. */
    private static ContactsPickerDelegate sContactsPickerDelegate;

    /**
     * A delegate interface for the contacts picker.
     */
    public interface ContactsPickerDelegate {
        /**
         * Called to display the contacts picker.
         * @param context  The context to use.
         * @param listener The listener that will be notified of the action the user took in the
         *                 picker.
         * @param allowMultiple Whether to allow multiple contacts to be picked.
         * @param includeNames Whether to include names of the contacts shared.
         * @param includeEmails Whether to include emails of the contacts shared.
         * @param includeTel Whether to include telephone numbers of the contacts shared.
         * @param includeAddresses Whether to include addresses of the contacts shared.
         * @param includeIcons Whether to include addresses of the contacts shared.
         * @param formattedOrigin The origin the data will be shared with, formatted for display
         *                        with the scheme omitted.
         */
        void showContactsPicker(Context context, ContactsPickerListener listener,
                boolean allowMultiple, boolean includeNames, boolean includeEmails,
                boolean includeTel, boolean includeAddresses, boolean includeIcons,
                String formattedOrigin);

        /**
         * Called when the contacts picker dialog has been dismissed.
         */
        void onContactsPickerDismissed();
    }

    /**
     * A delegate interface for the photo picker.
     */
    public interface PhotoPickerDelegate {
        /**
         * Called to display the photo picker.
         * @param context  The context to use.
         * @param listener The listener that will be notified of the action the user took in the
         *                 picker.
         * @param allowMultiple Whether the dialog should allow multiple images to be selected.
         * @param mimeTypes A list of mime types to show in the dialog.
         */
        void showPhotoPicker(Context context, PhotoPickerListener listener, boolean allowMultiple,
                List<String> mimeTypes);

        /**
         * Called when the photo picker dialog has been dismissed.
         */
        void onPhotoPickerDismissed();

        /**
         * Returns whether video decoding support is supported in the photo picker.
         */
        boolean supportsVideos();
    }

    // ContactsPickerDelegate:

    /**
     * Allows setting a delegate for an Android contacts picker.
     * @param delegate A {@link ContactsPickerDelegate} instance.
     */
    public static void setContactsPickerDelegate(ContactsPickerDelegate delegate) {
        sContactsPickerDelegate = delegate;
    }

    /**
     * Called to display the contacts picker.
     * @param context  The context to use.
     * @param listener The listener that will be notified of the action the user took in the
     *                 picker.
     * @param allowMultiple Whether to allow multiple contacts to be selected.
     * @param includeNames Whether to include names in the contact data returned.
     * @param includeEmails Whether to include emails in the contact data returned.
     * @param includeTel Whether to include telephone numbers in the contact data returned.
     * @param includeAddresses Whether to include addresses of the contacts shared.
     * @param includeIcons Whether to include icons of the contacts shared.
     * @param formattedOrigin The origin the data will be shared with.
     */
    public static boolean showContactsPicker(Context context, ContactsPickerListener listener,
            boolean allowMultiple, boolean includeNames, boolean includeEmails, boolean includeTel,
            boolean includeAddresses, boolean includeIcons, String formattedOrigin) {
        if (sContactsPickerDelegate == null) return false;
        sContactsPickerDelegate.showContactsPicker(context, listener, allowMultiple, includeNames,
                includeEmails, includeTel, includeAddresses, includeIcons, formattedOrigin);
        return true;
    }

    /**
     * Called when the contacts picker dialog has been dismissed.
     */
    public static void onContactsPickerDismissed() {
        if (sContactsPickerDelegate == null) return;
        sContactsPickerDelegate.onContactsPickerDismissed();
    }

    // PhotoPickerDelegate:

    /**
     * Allows setting a delegate to override the default Android stock photo picker.
     * @param delegate A {@link PhotoPickerDelegate} instance.
     */
    public static void setPhotoPickerDelegate(PhotoPickerDelegate delegate) {
        sPhotoPickerDelegate = delegate;
    }

    /**
     * Returns whether a photo picker should be called.
     */
    public static boolean shouldShowPhotoPicker() {
        return sPhotoPickerDelegate != null;
    }

    /**
     * Returns whether the photo picker supports showing videos.
     */
    public static boolean photoPickerSupportsVideo() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return false;
        if (!shouldShowPhotoPicker()) return false;
        return sPhotoPickerDelegate.supportsVideos();
    }

    /**
     * Called to display the photo picker.
     * @param context  The context to use.
     * @param listener The listener that will be notified of the action the user took in the
     *                 picker.
     * @param allowMultiple Whether the dialog should allow multiple images to be selected.
     * @param mimeTypes A list of mime types to show in the dialog.
     */
    public static boolean showPhotoPicker(Context context, PhotoPickerListener listener,
            boolean allowMultiple, List<String> mimeTypes) {
        if (sPhotoPickerDelegate == null) return false;
        sPhotoPickerDelegate.showPhotoPicker(context, listener, allowMultiple, mimeTypes);
        return true;
    }

    /**
     * Called when the photo picker dialog has been dismissed.
     */
    public static void onPhotoPickerDismissed() {
        if (sPhotoPickerDelegate == null) return;
        sPhotoPickerDelegate.onPhotoPickerDismissed();
    }

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
                String locale = ApiCompatibilityUtils.getLocale(subtypes.get(j));
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
                canvas.scale((float) (newWidth / originalWidth),
                        (float) (newHeight / originalHeight));
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
                path = new File(externalDataDir.getAbsolutePath()
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
     * Creates a {@link Typeface} that represents medium-weighted text.  This function returns
     * Roboto Medium when it is available (Lollipop and up) and Roboto Bold where it isn't.
     *
     * @return Typeface that can be applied to a View.
     */
    public static Typeface createRobotoMediumTypeface() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Roboto Medium, regular.
            return Typeface.create("sans-serif-medium", Typeface.NORMAL);
        } else {
            return Typeface.create("sans-serif", Typeface.BOLD);
        }
    }

    /**
     * Iterates through all items in the specified ListAdapter (including header and footer views)
     * and returns the width of the widest item (when laid out with height and width set to
     * WRAP_CONTENT).
     *
     * WARNING: do not call this on a ListAdapter with more than a handful of items, the performance
     * will be terrible since it measures every single item.
     *
     * @param adapter The ListAdapter whose widest item's width will be returned.
     * @return The measured width (in pixels) of the widest item in the passed-in ListAdapter.
     */
    public static int computeMaxWidthOfListAdapterItems(ListAdapter adapter) {
        final int widthMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        final int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        AbsListView.LayoutParams params = new AbsListView.LayoutParams(
                AbsListView.LayoutParams.WRAP_CONTENT, AbsListView.LayoutParams.WRAP_CONTENT);

        int maxWidth = 0;
        View[] itemViews = new View[adapter.getViewTypeCount()];
        for (int i = 0; i < adapter.getCount(); ++i) {
            View itemView;
            int type = adapter.getItemViewType(i);
            if (type < 0) {
                // Type is negative for header/footer views, or views the adapter does not want
                // recycled.
                itemView = adapter.getView(i, null, null);
            } else {
                itemViews[type] = adapter.getView(i, itemViews[type], null);
                itemView = itemViews[type];
            }

            itemView.setLayoutParams(params);
            itemView.measure(widthMeasureSpec, heightMeasureSpec);
            maxWidth = Math.max(maxWidth, itemView.getMeasuredWidth());
        }

        return maxWidth;
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
     * Gets a drawable from the resources and applies the specified tint to it. Uses Support Library
     * for vector drawables and tinting on older Android versions.
     * @param drawableId The resource id for the drawable.
     * @param tintColorId The resource id for the color or ColorStateList.
     */
    public static Drawable getTintedDrawable(
            Context context, @DrawableRes int drawableId, @ColorRes int tintColorId) {
        Drawable drawable = AppCompatResources.getDrawable(context, drawableId);
        assert drawable != null;
        drawable = DrawableCompat.wrap(drawable).mutate();
        DrawableCompat.setTintList(
                drawable, AppCompatResources.getColorStateList(context, tintColorId));
        return drawable;
    }

    /**
     * @return Whether the support for theming on a particular device has been completely disabled
     *         due to lack of support by the OEM.
     */
    public static boolean isSystemUiThemingDisabled() {
        if (sSystemUiThemingDisabled == null) {
            sSystemUiThemingDisabled = false;
            if (sAndroidUiThemeBlacklist.containsKey(Build.MANUFACTURER.toLowerCase(Locale.US))) {
                sSystemUiThemingDisabled = Build.VERSION.SDK_INT
                        < sAndroidUiThemeBlacklist.get(Build.MANUFACTURER.toLowerCase(Locale.US));
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
     * Extends {@link AlertDialog.Builder} to work around issues in support library. Note that
     * any AlertDialogs shown in CustomTabActivity should be created from this class.
     */
    public static class CompatibleAlertDialogBuilder extends AlertDialog.Builder {
        private final boolean mIsInNightMode;

        public CompatibleAlertDialogBuilder(@NonNull Context context) {
            super(context);
            mIsInNightMode = isInNightMode(context);
        }

        public CompatibleAlertDialogBuilder(@NonNull Context context, int themeResId) {
            super(context, themeResId);
            mIsInNightMode = isInNightMode(context);
        }

        @Override
        public AlertDialog create() {
            AlertDialog dialog = super.create();
            // Sets local night mode state to reflect the night mode state of the owner activity.
            // This is to work around an issue in the support library that the dialog night mode
            // state is not inheriting the night mode state of the owner activity, and also resets
            // the night mode state of the owner activity. See https://crbug.com/966002 for details.
            // TODO(https://crbug.com/966101): Remove this class once support library is updated to
            // AndroidX.
            dialog.getDelegate().setLocalNightMode(mIsInNightMode
                            ? AppCompatDelegate.MODE_NIGHT_YES
                            : AppCompatDelegate.MODE_NIGHT_NO);
            return dialog;
        }

        private static boolean isInNightMode(Context context) {
            return (context.getResources().getConfiguration().uiMode
                           & Configuration.UI_MODE_NIGHT_MASK)
                    == Configuration.UI_MODE_NIGHT_YES;
        }
    }
}
