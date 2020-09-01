// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.text.Html;
import android.text.Spanned;
import android.text.style.CharacterStyle;
import android.text.style.ParagraphStyle;
import android.text.style.UpdateAppearance;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.compat.ApiHelperForO;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.ui.R;
import org.chromium.ui.widget.Toast;

import java.io.IOException;
import java.util.List;
import java.util.Locale;

/**
 * Simple proxy that provides C++ code with an access pathway to the Android clipboard.
 */
@JNINamespace("ui")
public class Clipboard implements ClipboardManager.OnPrimaryClipChangedListener {
    @SuppressLint("StaticFieldLeak")
    private static Clipboard sInstance;

    // Necessary for coercing clipboard contents to text if they require
    // access to network resources, etceteras (e.g., URI in clipboard)
    private final Context mContext;

    private ClipboardManager mClipboardManager;

    private long mNativeClipboard;

    private ImageFileProvider mImageFileProvider;

    /**
     * Interface to be implemented for sharing image through FileProvider.
     */
    public interface ImageFileProvider {
        /**
         * Saves the given set of image bytes and provides that URI to a callback for
         * sharing the image.
         *
         * @param context The context used to trigger the action.
         * @param imageData The image data to be shared in |fileExtension| format.
         * @param fileExtension File extension which |imageData| encoded to.
         * @param callback A provided callback function which will act on the generated URI.
         */
        void storeImageAndGenerateUri(final Context context, final byte[] imageData,
                String fileExtension, Callback<Uri> callback);

        /**
         * Store the last image uri we put in the sytstem clipboard, this is special case for
         * Android O.
         */
        void storeLastCopiedImageUri(@NonNull Uri uri);

        /**
         * Get stored the last image uri, this is special case for Android O.
         */
        @Nullable
        Uri getLastCopiedImageUri();

        /**
         * Clear the image uri which stored by |storeLastCopiedImageUri|.
         */
        void clearLastCopiedImageUri();
    }

    /**
     * Get the singleton Clipboard instance (creating it if needed).
     */
    @CalledByNative
    public static Clipboard getInstance() {
        if (sInstance == null) {
            sInstance = new Clipboard();
        }
        return sInstance;
    }

    private Clipboard() {
        mContext = ContextUtils.getApplicationContext();
        mClipboardManager =
                (ClipboardManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CLIPBOARD_SERVICE);
        mClipboardManager.addPrimaryClipChangedListener(this);
    }

    /**
     * Emulates the behavior of the now-deprecated
     * {@link android.text.ClipboardManager#getText()} by invoking
     * {@link android.content.ClipData.Item#coerceToText(Context)} on the first
     * item in the clipboard (if any) and returning the result as a string.
     * <p>
     * This is quite different than simply calling {@link Object#toString()} on
     * the clip; consumers of this API should familiarize themselves with the
     * process described in
     * {@link android.content.ClipData.Item#coerceToText(Context)} before using
     * this method.
     *
     * @return a string representation of the first item on the clipboard, if
     *         the clipboard currently has an item and coercion of the item into
     *         a string is possible; otherwise, <code>null</code>
     */
    @SuppressWarnings("javadoc")
    @CalledByNative
    private String getCoercedText() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug.com/654802 and b/31501780)
        try {
            return mClipboardManager.getPrimaryClip()
                    .getItemAt(0)
                    .coerceToText(mContext)
                    .toString();
        } catch (Exception e) {
            return null;
        }
    }

    private boolean hasStyleSpan(Spanned spanned) {
        Class<?>[] styleClasses = {
                CharacterStyle.class, ParagraphStyle.class, UpdateAppearance.class};
        for (Class<?> clazz : styleClasses) {
            if (spanned.nextSpanTransition(-1, spanned.length(), clazz) < spanned.length()) {
                return true;
            }
        }
        return false;
    }

    public String clipDataToHtmlText(ClipData clipData) {
        ClipDescription description = clipData.getDescription();
        if (description.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML)) {
            return clipData.getItemAt(0).getHtmlText();
        }

        if (description.hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN)) {
            CharSequence text = clipData.getItemAt(0).getText();
            if (!(text instanceof Spanned)) return null;
            Spanned spanned = (Spanned) text;
            if (hasStyleSpan(spanned)) {
                return ApiCompatibilityUtils.toHtml(
                        spanned, Html.TO_HTML_PARAGRAPH_LINES_CONSECUTIVE);
            }
        }
        return null;
    }

    /**
     * Gets the HTML text of top item on the primary clip on the Android clipboard.
     *
     * @return a Java string with the html text if any, or null if there is no html
     *         text or no entries on the primary clip.
     */
    @CalledByNative
    private String getHTMLText() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug/654802 and b/31501780)
        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            return clipDataToHtmlText(clipData);
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Gets the Uri of top item on the primary clip on the Android clipboard if the mime type is
     * image.
     *
     * @return an Uri if mime type is image type, or null if there is no Uri or no entries on the
     *         primary clip.
     */
    public @Nullable Uri getImageUri() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug.com/654802).
        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            if (clipData == null || clipData.getItemCount() == 0) return null;

            ClipDescription description = clipData.getDescription();
            if (description == null || !description.hasMimeType("image/*")) {
                return null;
            }

            return clipData.getItemAt(0).getUri();
        } catch (Exception e) {
            return null;
        }
    }

    @CalledByNative
    private String getImageUriString() {
        Uri uri = getImageUri();
        return uri == null ? null : uri.toString();
    }

    /**
     * Reads the Uri of top item on the primary clip on the Android clipboard, and try to get the
     * {@link Bitmap}. for that Uri.
     * Fetching images can result in I/O, so should not be called on UI thread.
     *
     * @return an {@link Bitmap} if available, otherwise null.
     */
    @CalledByNative
    public Bitmap getImage() {
        ThreadUtils.assertOnBackgroundThread();
        try {
            Uri uri = getImageUri();
            if (uri == null) return null;

            Bitmap bitmap = ApiCompatibilityUtils.getBitmapByUri(
                    ContextUtils.getApplicationContext().getContentResolver(), uri);
            if (!bitmapSupportByGfx(bitmap)) {
                return bitmap.copy(Bitmap.Config.ARGB_8888, /*mutable=*/false);
            }
            return bitmap;
        } catch (IOException | SecurityException e) {
            return null;
        }
    }

    /**
     * Emulates the behavior of the now-deprecated
     * {@link android.text.ClipboardManager#setText(CharSequence)}, setting the
     * clipboard's current primary clip to a plain-text clip that consists of
     * the specified string.
     * @param text  will become the content of the clipboard's primary clip
     */
    @CalledByNative
    public void setText(final String text) {
        setPrimaryClipNoException(ClipData.newPlainText("text", text));
    }

    /**
     * Writes HTML to the clipboard, together with a plain-text representation
     * of that very data.
     *
     * @param html  The HTML content to be pasted to the clipboard.
     * @param text  Plain-text representation of the HTML content.
     */
    @CalledByNative
    private void setHTMLText(final String html, final String text) {
        setPrimaryClipNoException(ClipData.newHtmlText("html", text, html));
    }

    /**
     * Setting the clipboard's current primary clip to an image.
     * This method requires background work and might not be immediately committed upon returning
     * from this method.
     * @param Uri The {@link Uri} will become the content of the clipboard's primary clip.
     */
    public void setImageUri(final Uri uri) {
        if (uri == null) {
            showCopyToClipboardFailureMessage();
            return;
        }

        grantUriPermission(uri);

        // ClipData.newUri may access the disk (for reading mime types), and cause
        // StrictModeDiskReadViolation if do it on UI thread.
        new AsyncTask<ClipData>() {
            @Override
            protected ClipData doInBackground() {
                return ClipData.newUri(
                        ContextUtils.getApplicationContext().getContentResolver(), "image", uri);
            }
            @Override
            protected void onPostExecute(ClipData clipData) {
                setPrimaryClipNoException(clipData);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Setting the clipboard's current primary clip to an image.
     * @param imageData The image data to be shared in |extension| format.
     * @param extension Image file extension which |imageData| encoded to.
     */
    @CalledByNative
    @VisibleForTesting
    public void setImage(final byte[] imageData, final String extension) {
        if (mImageFileProvider == null) {
            // Since |mImageFileProvider| is set on very early on during process init, and if
            // setImage is called before the file provider is set, we can just drop it on the floor.
            return;
        }

        mImageFileProvider.storeImageAndGenerateUri(
                mContext, imageData, extension, (Uri uri) -> { setImageUri(uri); });
    }

    /**
     * Clears the Clipboard Primary clip.
     *
     */
    @CalledByNative
    private void clear() {
        setPrimaryClipNoException(ClipData.newPlainText(null, null));
    }

    private boolean setPrimaryClipNoException(ClipData clip) {
        final String manufacturer = Build.MANUFACTURER.toLowerCase(Locale.US);
        // See crbug.com/1123727, there are OEM devices having strict mode violations in their
        // Android framework code. Disabling strict mode for non-google devices.
        try (StrictModeContext ignored = manufacturer.equals("google")
                        ? null
                        : StrictModeContext.allowAllThreadPolicies()) {
            mClipboardManager.setPrimaryClip(clip);
            return true;
        } catch (Exception ex) {
            // Ignore any exceptions here as certain devices have bugs and will fail.
            showCopyToClipboardFailureMessage();
            return false;
        }
    }

    private void showCopyToClipboardFailureMessage() {
        String text = mContext.getString(R.string.copy_to_clipboard_failure_message);
        Toast.makeText(mContext, text, Toast.LENGTH_SHORT).show();
    }

    @CalledByNative
    private void setNativePtr(long nativeClipboard) {
        mNativeClipboard = nativeClipboard;
    }

    /**
     * Set {@link ImageFileProvider} for sharing image.
     * @param imageFileProvider The implementation of {@link ImageFileProvider}.
     */
    public void setImageFileProvider(ImageFileProvider imageFileProvider) {
        mImageFileProvider = imageFileProvider;
    }

    /**
     * Tells the C++ Clipboard that the clipboard has changed.
     *
     * Implements OnPrimaryClipChangedListener to listen for clipboard updates.
     */
    @Override
    public void onPrimaryClipChanged() {
        RecordUserAction.record("MobileClipboardChanged");
        revokeUriPermissionForLastSharedImage();
        if (mNativeClipboard != 0) {
            ClipboardJni.get().onPrimaryClipChanged(mNativeClipboard, Clipboard.this);
        }
    }

    /**
     * Copy the specified URL to the clipboard and show a toast indicating the action occurred.
     * @param url The URL to copy to the clipboard.
     */
    public void copyUrlToClipboard(String url) {
        ClipData clip = ClipData.newPlainText("url", url);
        if (setPrimaryClipNoException(clip)) {
            Toast.makeText(mContext, R.string.link_copied, Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * Because Android may not notify apps in the background that the content of clipboard has
     * changed, this method proactively considers clipboard invalidated, when the app loses focus.
     * @param hasFocus Whether or not {@code activity} gained or lost focus.
     */
    public void onWindowFocusChanged(boolean hasFocus) {
        if (mNativeClipboard == 0 || !hasFocus || !BuildInfo.isAtLeastQ()) return;
        onPrimaryClipTimestampInvalidated();
    }

    @TargetApi(Build.VERSION_CODES.O)
    private void onPrimaryClipTimestampInvalidated() {
        ClipDescription clipDescription = mClipboardManager.getPrimaryClipDescription();
        if (clipDescription == null) return;

        long timestamp = ApiHelperForO.getTimestamp(clipDescription);
        ClipboardJni.get().onPrimaryClipTimestampInvalidated(
                mNativeClipboard, Clipboard.this, timestamp);
    }

    /**
     * Gets the last modified timestamp observed by the native side ClipboardAndroid, not the
     * Android framework.
     *
     * @return the last modified time in millisecond.
     */
    public long getLastModifiedTimeMs() {
        if (mNativeClipboard == 0) return 0;
        return ClipboardJni.get().getLastModifiedTimeToJavaTime(mNativeClipboard);
    }

    /**
     * Grant permission to access a specific Uri to other packages. For sharing images through the
     * system’s clipboard, Outside of Android O permissions are already managed properly by the
     * system. But on Android O, sharing images/files needs to grant permission to each app/packages
     * individually. Note: Don't forget to revoke the permission once the clipboard is updated.
     */
    private void grantUriPermission(@NonNull Uri uri) {
        if ((Build.VERSION.SDK_INT != Build.VERSION_CODES.O
                    && Build.VERSION.SDK_INT != Build.VERSION_CODES.O_MR1)
                || mImageFileProvider == null) {
            return;
        }

        // Cache the Uri for revoking permission later.
        mImageFileProvider.storeLastCopiedImageUri(uri);
        List<PackageInfo> installedPackages = mContext.getPackageManager().getInstalledPackages(0);
        for (PackageInfo installedPackage : installedPackages) {
            mContext.grantUriPermission(
                    installedPackage.packageName, uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
    }

    /**
     * Revoke the permission for previously shared image uri. This operation is only needed for
     * Android O.
     */
    private void revokeUriPermissionForLastSharedImage() {
        if (Build.VERSION.SDK_INT != Build.VERSION_CODES.O
                && Build.VERSION.SDK_INT != Build.VERSION_CODES.O_MR1) {
            return;
        }

        if (mImageFileProvider == null) {
            // It is ok to not revoke permission. Since |mImageFileProvider| is set very early on
            // during process init, |mImageFileProvider| == null means we are starting.
            // ShareImageFileUtils#clearSharedImages will clear cached image files during
            // startup if they are not being shared. Therefore even if permission is not revoked,
            // the other package will not get the image. The permission will be revoked later, once
            // onPrimaryClipChanged triggered. Also, since shared images use timestamp as file
            // name, the file name will not be reused.
            return;
        }

        Uri uri = mImageFileProvider.getLastCopiedImageUri();
        // Exit early if the URI is empty or event onPrimaryClipChanges was caused by sharing
        // image.
        if (uri == null || uri.equals(Uri.EMPTY) || uri.equals(getImageUri())) return;

        // https://developer.android.com/reference/android/content/Context#revokeUriPermission(android.net.Uri,%20int)
        // According to the above link, it is not necessary to enumerate all of the packages like
        // what was done in |grantUriPermission|. Context#revokeUriPermission(Uri, int) will revoke
        // all permissions.
        mContext.revokeUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        // Clear uri to avoid revoke over and over.
        mImageFileProvider.clearLastCopiedImageUri();
    }

    /**
     * Check if |bitmap| is support by native side. gfx::CreateSkBitmapFromJavaBitmap only support
     * ARGB_8888 and ALPHA_8.
     */
    private boolean bitmapSupportByGfx(Bitmap bitmap) {
        return bitmap != null
                && (bitmap.getConfig() == Bitmap.Config.ARGB_8888
                        || bitmap.getConfig() == Bitmap.Config.ALPHA_8);
    }

    /**
     * Allows the ClipboardManager Android Service to be replaced with a mock for tests, returning
     * the original so that it can be restored.
     */
    @VisibleForTesting
    public ClipboardManager overrideClipboardManagerForTesting(ClipboardManager manager) {
        ClipboardManager oldManager = mClipboardManager;
        mClipboardManager = manager;
        return oldManager;
    }

    @NativeMethods
    interface Natives {
        void onPrimaryClipChanged(long nativeClipboardAndroid, Clipboard caller);
        void onPrimaryClipTimestampInvalidated(
                long nativeClipboardAndroid, Clipboard caller, long timestamp);
        long getLastModifiedTimeToJavaTime(long nativeClipboardAndroid);
    }
}
