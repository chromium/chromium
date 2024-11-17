// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.annotation.SuppressLint;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.url.GURL;

/** Simple proxy that provides C++ code with an access pathway to the Android clipboard. */
@JNINamespace("ui")
public class Clipboard {
    private static final String TAG = "Clipboard";

    @SuppressLint("StaticFieldLeak")
    private static Clipboard sInstance;

    private long mNativeClipboard;

    /** Interface to be implemented for sharing image through FileProvider. */
    public interface ImageFileProvider {
        /** The helper class to load Clipboard file metadata. */
        public class ClipboardFileMetadata {
            public static final long INVALID_TIMESTAMP = 0;
            public final Uri uri;
            public final long timestamp;

            public ClipboardFileMetadata(Uri uri, long timestamp) {
                this.uri = uri;
                this.timestamp = timestamp;
            }
        }

        /**
         * Saves the given set of image bytes and provides that URI to a callback for
         * sharing the image.
         *
         * @param imageData The image data to be shared in |fileExtension| format.
         * @param fileExtension File extension which |imageData| encoded to.
         * @param callback A provided callback function which will act on the generated URI.
         */
        void storeImageAndGenerateUri(
                final byte[] imageData, String fileExtension, Callback<Uri> callback);

        /**
         * Store the last image uri and its timestamp we put in the sytstem clipboard.
         * On Android O and O_MR1, URI is stored for revoking permissions later.
         * @param clipboardFileMetadata The metadata needs to be stored.
         */
        void storeLastCopiedImageMetadata(@NonNull ClipboardFileMetadata clipboardFileMetadata);

        /** Get stored the last image uri and its timestamp. */
        @Nullable
        ClipboardFileMetadata getLastCopiedImageMetadata();

        /**
         * Clear the image uri and its timestamp which are stored by |storeLastCopiedImageMetadata|.
         * This can be called any time after the system clipboard does not contain the uri we
         * stored. On Android O and O_MR1, this can be called either after the permission is
         * revoked.
         */
        void clearLastCopiedImageMetadata();
    }

    /** Get the singleton Clipboard instance (creating it if needed). */
    @CalledByNative
    public static Clipboard getInstance() {
        if (sInstance == null) {
            ClipboardManager clipboardManager =
                    (ClipboardManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.CLIPBOARD_SERVICE);
            if (clipboardManager != null) {
                sInstance = new ClipboardImpl(clipboardManager);
            } else {
                sInstance = new Clipboard();
            }
        }
        return sInstance;
    }

    /**
     * Resets the clipboard instance for testing.
     *
     * Particularly relevant for robolectric tests where the application context is not shared
     * across test runs and the Clipboard instance would hold onto an older no longer used
     * application context instance.
     */
    public static void resetForTesting() {
        sInstance = null;
    }

    /** Cleans up clipboard on native side. */
    public static void cleanupNativeForTesting() {
        ClipboardJni.get().cleanupForTesting();
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
    protected String getCoercedText() {
        return null;
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected boolean hasCoercedText() {
        return false;
    }

    public String clipDataToHtmlText(ClipData clipData) {
        return null;
    }

    /**
     * Gets the HTML text of top item on the primary clip on the Android clipboard.
     *
     * @return a Java string with the html text if any, or null if there is no html
     *         text or no entries on the primary clip.
     */
    @CalledByNative
    protected String getHTMLText() {
        return null;
    }

    /**
     * Check if the system clipboard contains HTML text or plain text with stlyed text.
     * Since Spanned could be copied to Clipboard as plain_text MIME type, and it can be converted
     * to HTML by android.text.Html#toHtml().
     * @return True if the system clipboard contains the html text or the styled text.
     */
    @CalledByNative
    public boolean hasHTMLOrStyledText() {
        return false;
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean hasUrl() {
        return false;
    }

    /**
     * On Pre S, we return the whole clipboard content if the clipboard content is a URL.
     * On S+, we return the first URL in the content. ex, If clipboard contains "text www.foo.com
     * www.bar.com", then "www.foo.com" will be returned.
     * @return The URL in the clipboard, or the first URL on the clipbobard.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    String getUrl() {
        return null;
    }

    /**
     * Gets the Uri of top item on the primary clip on the Android clipboard if the mime type is
     * image.
     *
     * @return an Uri if mime type is image type, or null if there is no Uri or no entries on the
     *         primary clip.
     */
    public @Nullable Uri getImageUri() {
        return null;
    }

    /** Return the image URI in the system clipboard if the URI is shared by this app */
    public @Nullable Uri getImageUriIfSharedByThisApp() {
        return null;
    }

    @CalledByNative
    protected String getImageUriString() {
        return null;
    }

    /**
     * Reads the Uri of top item on the primary clip on the Android clipboard,
     * returning a byte array of PNG data.
     * Fetching images can result in I/O, so should not be called on UI thread.
     *
     * @return a byte array of PNG data if available, otherwise null.
     */
    @CalledByNative
    public byte[] getPng() {
        return null;
    }

    @CalledByNative
    protected boolean hasImage() {
        return false;
    }

    /**
     * Gets a list of content URIs on the primary clip on the Android Clipboard and their display
     * names.
     *
     * @return list of content URIs and display names. item[i][0] is the URI, and item[i][1] is the
     *     optional display name which will be an empty string when unknown.
     */
    @CalledByNative
    protected String[][] getFilenames() {
        return null;
    }

    /**
     * Check if the system clipboard contains any content URIs (filenames).
     *
     * @return True if the system clipboard contains any content URIs (filenames).
     */
    @CalledByNative
    public boolean hasFilenames() {
        return false;
    }

    /**
     * Emulates the behavior of the now-deprecated
     * {@link android.text.ClipboardManager#setText(CharSequence)}, setting the
     * clipboard's current primary clip to a plain-text clip that consists of
     * the specified string.
     * @param text  will become the content of the clipboard's primary clip.
     */
    @CalledByNative
    public void setText(final String text) {
        Log.w(TAG, "setText is a no-op because Clipboard service isn't available");
    }

    /**
     * Writes text to the clipboard.
     *
     * @param label the label for the clip data.
     * @param text  will become the content of the clipboard's primary clip.
     */
    public void setText(final String label, final String text) {
        Log.w(TAG, "setText is a no-op because Clipboard service isn't available");
    }

    /**
     * Writes text to the clipboard.
     *
     * @param label the label for the clip data.
     * @param text  will become the content of the clipboard's primary clip.
     * @param notifyOnSuccess whether show a notification, e.g. a toast, to the user when success.
     */
    public void setText(final String label, final String text, boolean notifyOnSuccess) {
        Log.w(TAG, "setText is a no-op because Clipboard service isn't available");
    }

    /**
     * Writes HTML to the clipboard, together with a plain-text representation
     * of that very data.
     *
     * @param html  The HTML content to be pasted to the clipboard.
     * @param text  Plain-text representation of the HTML content.
     */
    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setHTMLText(final String html, final String text) {
        Log.w(TAG, "setHTMLText is a no-op because Clipboard service isn't available");
    }

    /**
     * Writes password to the clipboard, and set the Clipdata is sensitive.
     * @param password  will become the content of the clipboard's primary clip.
     */
    @CalledByNative
    public void setPassword(final String password) {
        Log.w(TAG, "setPassword is a no-op because Clipboard service isn't available");
    }

    /**
     * Setting the clipboard's current primary clip to an image. This method requires background
     * work and might not be immediately committed upon returning from this method.
     *
     * @param uri The {@link Uri} will become the content of the clipboard's primary clip.
     */
    public void setImageUri(final Uri uri) {
        Log.w(TAG, "setImageUri is a no-op because Clipboard service isn't available");
    }

    /**
     * Setting the clipboard's current primary clip to an image. This method requires background
     * work and might not be immediately committed upon returning from this method.
     *
     * @see #setImageUri(Uri)
     * @param uri The {@link Uri} will become the content of the clipboard's primary clip.
     * @param notifyOnSuccess Whether show a notification when success.
     */
    public void setImageUri(final Uri uri, boolean notifyOnSuccess) {
        Log.w(TAG, "setImageUriAndNotify is a no-op because Clipboard service isn't available");
    }

    /**
     * Setting the clipboard's current primary clip to an image.
     * @param imageData The image data to be shared in |extension| format.
     * @param extension Image file extension which |imageData| encoded to.
     */
    @CalledByNative
    @VisibleForTesting
    public void setImage(final byte[] imageData, final String extension) {
        Log.w(TAG, "setImage is a no-op because Clipboard service isn't available");
    }

    /**
     * Writes content URI filenames to the clipboard.
     *
     * @param uriList list of content URIs.
     */
    @CalledByNative
    public void setFilenames(final String[] uriList) {
        Log.w(TAG, "setFilenames is a no-op because Clipboard service isn't available");
    }

    /** Clears the Clipboard Primary clip. */
    @CalledByNative
    protected void clear() {}

    @CalledByNative
    private void setNativePtr(long nativeClipboard) {
        mNativeClipboard = nativeClipboard;
    }

    /**
     * Set {@link ImageFileProvider} for sharing image.
     * @param imageFileProvider The implementation of {@link ImageFileProvider}.
     */
    public void setImageFileProvider(ImageFileProvider imageFileProvider) {
        Log.w(TAG, "setImageFileProvider is a no-op because Clipboard service isn't available");
    }

    /**
     * Copy the specified URL to the clipboard and show a toast indicating the action occurred.
     * @param url The URL to copy to the clipboard.
     */
    public void copyUrlToClipboard(GURL url) {}

    /**
     * Because Android may not notify apps in the background that the content of clipboard has
     * changed, this method proactively considers clipboard invalidated, when the app loses focus.
     * @param hasFocus Whether or not {@code activity} gained or lost focus.
     */
    public void onWindowFocusChanged(boolean hasFocus) {}

    /**
     * Gets the last modified timestamp observed by the native side ClipboardAndroid, not the
     * Android framework.
     *
     * @return the last modified time in millisecond.
     */
    public long getLastModifiedTimeMs() {
        return 0;
    }

    /**
     * Check if the system clipboard has content to be pasted.
     * @return true if the system clipboard contains anything, otherwise, return false.
     */
    public boolean canPaste() {
        return false;
    }

    /**
     * Check if the clipboard can be used for copy. false if the backed clipboard service isn't
     * available.
     *
     * @return Whether clipboard supports copy operation.
     */
    public boolean canCopy() {
        return false;
    }

    protected void notifyPrimaryClipChanged() {
        if (mNativeClipboard == 0) return;
        ClipboardJni.get().onPrimaryClipChanged(mNativeClipboard, this);
    }

    protected void notifyPrimaryClipTimestampInvalidated(long timestamp) {
        if (mNativeClipboard == 0) return;
        ClipboardJni.get().onPrimaryClipTimestampInvalidated(mNativeClipboard, this, timestamp);
    }

    protected long getLastModifiedTimeToJavaTime() {
        if (mNativeClipboard == 0) return 0;
        return ClipboardJni.get().getLastModifiedTimeToJavaTime(mNativeClipboard);
    }

    @NativeMethods
    interface Natives {
        void onPrimaryClipChanged(long nativeClipboardAndroid, Clipboard caller);

        void onPrimaryClipTimestampInvalidated(
                long nativeClipboardAndroid, Clipboard caller, long timestamp);

        long getLastModifiedTimeToJavaTime(long nativeClipboardAndroid);

        void cleanupForTesting();
    }
}
