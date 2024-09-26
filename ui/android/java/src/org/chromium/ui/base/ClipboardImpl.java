// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.PersistableBundle;
import android.text.Html;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.CharacterStyle;
import android.text.style.ParagraphStyle;
import android.text.style.UpdateAppearance;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextLinks;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StreamUtil;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.R;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Simple proxy that provides C++ code with an access pathway to the Android clipboard. */
@JNINamespace("ui")
public class ClipboardImpl extends Clipboard
        implements ClipboardManager.OnPrimaryClipChangedListener {
    private static final float CONFIDENCE_THRESHOLD_FOR_URL_DETECTION = 0.99f;

    private static final long MAX_ALLOWED_PNG_SIZE_BYTES = (long) 100e6; // 100 MB.

    // This mime type annotates that clipboard contains a URL.
    private static final String URL_MIME_TYPE = "text/x-moz-url";

    // This mime type annotates that clipboard contains a text.
    private static final String TEXT_MIME_TYPE = "text/*";

    // This mime type annotates that clipboard contains a plain text.
    private static final String PLAIN_TEXT_MIME_TYPE = "text/plain";

    // This mime type annotates that clipboard contains a PNG image.
    private static final String PNG_MIME_TYPE = "image/png";

    private static @Nullable Boolean sSkipImageMimeTypeCheckForTesting;

    // Necessary for coercing clipboard contents to text if they require
    // access to network resources, etceteras (e.g., URI in clipboard)
    private final Context mContext;

    private ClipboardManager mClipboardManager;

    private ImageFileProvider mImageFileProvider;

    private ImageFileProvider.ClipboardFileMetadata mPendingCopiedImageMetadata;

    public ClipboardImpl(ClipboardManager clipboardManager) {
        mContext = ContextUtils.getApplicationContext();
        mClipboardManager = clipboardManager;
        mClipboardManager.addPrimaryClipChangedListener(this);
    }

    @Override
    protected String getCoercedText() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug.com/654802 and b/31501780)
        try {
            return mClipboardManager
                    .getPrimaryClip()
                    .getItemAt(0)
                    .coerceToText(mContext)
                    .toString();
        } catch (Exception e) {
            return null;
        }
    }

    @Override
    protected boolean hasCoercedText() {
        ClipDescription description = mClipboardManager.getPrimaryClipDescription();
        if (description == null) return false;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            // On Pre-P, {@link clear()} uses an empty ClipData#newPlainText to clear the clipboard,
            // which will create an empty MIMETYPE_TEXT_PLAIN in the clipboard, so we need to read
            // the real clipboard data to check.
            return !TextUtils.isEmpty(getCoercedText());
        }

        return description.hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN)
                || description.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML)
                || description.hasMimeType(URL_MIME_TYPE);
    }

    private boolean hasStyleSpan(Spanned spanned) {
        Class<?>[] styleClasses = {
            CharacterStyle.class, ParagraphStyle.class, UpdateAppearance.class
        };
        for (Class<?> clazz : styleClasses) {
            if (spanned.nextSpanTransition(-1, spanned.length(), clazz) < spanned.length()) {
                return true;
            }
        }
        return false;
    }

    @Override
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
                return Html.toHtml(spanned, Html.TO_HTML_PARAGRAPH_LINES_CONSECUTIVE);
            }
        }
        return null;
    }

    @Override
    protected String getHTMLText() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug/654802 and b/31501780)
        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            return clipDataToHtmlText(clipData);
        } catch (Exception e) {
            return null;
        }
    }

    @Override
    public boolean hasHTMLOrStyledText() {
        ClipDescription description = mClipboardManager.getPrimaryClipDescription();
        if (description == null) return false;

        boolean isPlainType = description.hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN);
        return (isPlainType && hasStyledText(description))
                || description.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML);
    }

    @Override
    boolean hasUrl() {
        // ClipDescription#getConfidenceScore is only available on Android S+, so before Android S,
        // we will access the clipboard content and valid by URLUtil#isValidUrl.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ClipDescription description = mClipboardManager.getPrimaryClipDescription();
            if (description == null) return false;
            if (description.hasMimeType(URL_MIME_TYPE)) return true;

            // Only use TextClassifier on text mime type.
            // If getClassificationStatus() is not CLASSIFICATION_COMPLETE,
            // ClipDescription#getConfidenceScore will trows exception.
            if (!description.hasMimeType(TEXT_MIME_TYPE)
                    || !(description.getClassificationStatus()
                            == ClipDescription.CLASSIFICATION_COMPLETE)) {
                return false;
            }

            float score = description.getConfidenceScore(TextClassifier.TYPE_URL);
            return score > CONFIDENCE_THRESHOLD_FOR_URL_DETECTION;
        } else {
            GURL url = new GURL(getCoercedText());
            return url.isValid();
        }
    }

    @Override
    String getUrl() {
        if (!hasUrl()) return null;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return getCoercedText();

        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            ClipDescription description = clipData.getDescription();
            CharSequence firstLinkText = null;
            if (description.hasMimeType(URL_MIME_TYPE)) {
                firstLinkText = getCoercedText();
            } else {
                ClipData.Item item = clipData.getItemAt(0);
                TextLinks textLinks = item.getTextLinks();
                if (textLinks == null || textLinks.getLinks().isEmpty()) return null;

                CharSequence fullText = item.getText();
                TextLinks.TextLink firstLink = textLinks.getLinks().iterator().next();
                firstLinkText = fullText.subSequence(firstLink.getStart(), firstLink.getEnd());
            }
            if (firstLinkText == null) return null;

            // Fixing the URL here since Android thought the string is a URL, but GURL may not
            // recognize the string as a URL. Ex. www.foo.com. Android thinks this is a URL, but
            // GURL doesn't since there is no protocol.
            GURL fixedUrl = UrlFormatter.fixupUrl(firstLinkText.toString());
            return fixedUrl.getSpec();
        } catch (Exception e) {
            return null;
        }
    }

    @Override
    public @Nullable Uri getImageUri() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug.com/654802).
        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            if (clipData == null
                    || clipData.getItemCount() == 0
                    || !hasImageMimeType(clipData.getDescription())) {
                return null;
            }

            return clipData.getItemAt(0).getUri();
        } catch (Exception e) {
            return null;
        }
    }

    @Override
    public @Nullable Uri getImageUriIfSharedByThisApp() {
        if (mImageFileProvider == null) return null;

        ImageFileProvider.ClipboardFileMetadata imageMetadata =
                mImageFileProvider.getLastCopiedImageMetadata();
        if (imageMetadata == null || imageMetadata.uri == null) return null;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // ClipDescription#getTimestamp() only exist in O+, so we just check if getImageUri()
            // same as the stored URI.
            if (!imageMetadata.uri.equals(getImageUri())) {
                mImageFileProvider.clearLastCopiedImageMetadata();
                return null;
            }
            return imageMetadata.uri;
        }

        long clipboardTimeStamp = getImageTimestamp();
        if (clipboardTimeStamp == ImageFileProvider.ClipboardFileMetadata.INVALID_TIMESTAMP
                || mImageFileProvider == null) {
            return null;
        }

        if (clipboardTimeStamp != imageMetadata.timestamp) {
            // The system clipboard does not contain uri from us, we can clean up the data.
            mImageFileProvider.clearLastCopiedImageMetadata();
            return null;
        }

        return imageMetadata.uri;
    }

    @Override
    protected String getImageUriString() {
        Uri uri = getImageUri();
        return uri == null ? null : uri.toString();
    }

    @Override
    public byte[] getPng() {
        ThreadUtils.assertOnBackgroundThread();

        Uri uri = getImageUri();
        if (uri == null) return null;

        ContentResolver cr = ContextUtils.getApplicationContext().getContentResolver();
        String mimeType = cr.getType(uri);
        if (!PNG_MIME_TYPE.equalsIgnoreCase(mimeType)) {
            if (!hasImage()) return null;

            // Android system clipboard contains an image, but it is not a PNG.
            // Try reading it as a bitmap and encoding to a PNG.
            try {
                // TODO(crbug.com/40811473): This uses the unsafe ImageDecoder class.
                Bitmap bitmap = ApiCompatibilityUtils.getBitmapByUri(cr, uri);
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                // |quality| is ignored since PNG encoding is lossless. See
                // https://developer.android.com/reference/android/graphics/Bitmap.CompressFormat#PNG.
                bitmap.compress(Bitmap.CompressFormat.PNG, /* quality= */ 100, baos);
                if (baos.size() > MAX_ALLOWED_PNG_SIZE_BYTES) return null;

                return baos.toByteArray();
            } catch (IOException | OutOfMemoryError e) {
                return null;
            }
        }

        // The image is a PNG. Read and return the raw bytes.
        FileInputStream fileStream = null;
        try (AssetFileDescriptor afd = cr.openAssetFileDescriptor(uri, "r")) {
            if (afd == null
                    || afd.getLength() > MAX_ALLOWED_PNG_SIZE_BYTES
                    || afd.getLength() == AssetFileDescriptor.UNKNOWN_LENGTH) {
                return null;
            }
            byte[] data = new byte[(int) afd.getLength()];
            fileStream = new FileInputStream(afd.getFileDescriptor());
            fileStream.read(data);
            return data;
        } catch (IOException e) {
            return null;
        } finally {
            StreamUtil.closeQuietly(fileStream);
        }
    }

    @Override
    protected boolean hasImage() {
        ClipDescription description = mClipboardManager.getPrimaryClipDescription();
        return hasImageMimeType(description);
    }

    private static boolean hasImageMimeType(ClipDescription description) {
        return (description != null)
                && (description.hasMimeType("image/*")
                        || (sSkipImageMimeTypeCheckForTesting != null
                                && sSkipImageMimeTypeCheckForTesting));
    }

    /**
     * Return the timestamp for the content in the clipboard if the clipboard contains an image.
     * return 0 on Android Pre O since the ClipDescription#getTimestamp() only exist in O+.
     */
    private long getImageTimestamp() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // ClipDescription#getTimestamp() only exist in O+, so we just return 0.
            return ImageFileProvider.ClipboardFileMetadata.INVALID_TIMESTAMP;
        }

        ClipDescription description = mClipboardManager.getPrimaryClipDescription();
        if (description == null || !description.hasMimeType("image/*")) {
            return ImageFileProvider.ClipboardFileMetadata.INVALID_TIMESTAMP;
        }

        return description.getTimestamp();
    }

    @Override
    protected String[][] getFilenames() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug/654802 and b/31501780)
        List<String[]> uris = new ArrayList<String[]>();
        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            for (int i = 0; i < clipData.getItemCount(); i++) {
                Uri uri = clipData.getItemAt(i).getUri();
                if (uri != null) {
                    String uriString = uri.toString();
                    String displayName = ContentUriUtils.maybeGetDisplayName(uriString);
                    if (displayName == null) {
                        displayName = new String();
                    }
                    uris.add(new String[] {uriString, displayName});
                }
            }
        } catch (Exception e) {
            // Return an empty list below if there is an error accessing ClipData.
        }
        return uris.toArray(new String[][] {});
    }

    @Override
    public boolean hasFilenames() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug/654802 and b/31501780)
        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            for (int i = 0; i < clipData.getItemCount(); i++) {
                Uri uri = clipData.getItemAt(i).getUri();
                if (uri != null) {
                    return true;
                }
            }
            return false;
        } catch (Exception e) {
            return false;
        }
    }

    @Override
    public void setText(final String text) {
        setText("text", text, false);
    }

    @Override
    public void setText(final String label, final String text) {
        setText(label, text, false);
    }

    @Override
    public void setText(final String label, final String text, boolean notifyOnSuccess) {
        if (setPrimaryClipNoException(ClipData.newPlainText(label, text)) && notifyOnSuccess) {
            showToastIfNeeded(R.string.copied);
        }
    }

    @Override
    void setHTMLText(final String html, final String text) {
        setPrimaryClipNoException(ClipData.newHtmlText("html", text, html));
    }

    @Override
    public void setPassword(final String password) {
        ClipData clipData = ClipData.newPlainText("password", password);
        PersistableBundle extras = new PersistableBundle();
        extras.putBoolean(ClipDescription.EXTRA_IS_SENSITIVE, true);
        clipData.getDescription().setExtras(extras);
        setPrimaryClipNoException(clipData);
    }

    @Override
    public void setImageUri(final Uri uri) {
        setImageUri(uri, false);
    }

    @Override
    public void setImageUri(final Uri uri, boolean notifyOnSuccess) {
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
                if (setPrimaryClipNoException(clipData) && notifyOnSuccess) {
                    showToastIfNeeded(R.string.image_copied);
                }

                // Storing timestamp is for avoiding accessing the system clipboard data, which may
                // cause the clipboard access notification to show up, when we try to clean up the
                // image file. There is a small chance that the clipboard image is updated between
                // |setPrimaryClipNoException| and |getImageTimestamp|, and we will get a wrong
                // timestamp. But it is okay since the timestamp is for deciding if the image file
                // need to be deleted. If the timestamp is wrong here, we just keep the image file a
                // little longer than expected.
                long imageTimestamp = getImageTimestamp();

                if (mImageFileProvider == null) {
                    mPendingCopiedImageMetadata =
                            new ImageFileProvider.ClipboardFileMetadata(uri, imageTimestamp);
                } else {
                    mImageFileProvider.storeLastCopiedImageMetadata(
                            new ImageFileProvider.ClipboardFileMetadata(uri, imageTimestamp));
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public void setImage(final byte[] imageData, final String extension) {
        if (mImageFileProvider == null) {
            // Since |mImageFileProvider| is set on very early on during process init, and if
            // setImage is called before the file provider is set, we can just drop it on the floor.
            return;
        }

        mImageFileProvider.storeImageAndGenerateUri(
                imageData,
                extension,
                (Uri uri) -> {
                    setImageUri(uri);
                });
    }

    @Override
    public void setFilenames(final String[] uriList) {
        ClipData clipData = null;
        ContentResolver cr = ContextUtils.getApplicationContext().getContentResolver();
        for (int i = 0; i < uriList.length; i++) {
            Uri uri = null;
            try {
                uri = Uri.parse(uriList[i]);
            } catch (Exception e) {
                // Handle null uri below.
            }
            if (uri == null) {
                continue;
            }
            if (clipData == null) {
                clipData = ClipData.newUri(cr, null, uri);
            } else {
                clipData.addItem(cr, new ClipData.Item(uri));
            }
        }
        if (clipData != null) {
            setPrimaryClipNoException(clipData);
        } else {
            clear();
        }
    }

    @Override
    protected void clear() {
        // clearPrimaryClip() has been observed to throw unexpected exceptions for Android P (see
        // crbug/1203377)
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
            setPrimaryClipNoException(ClipData.newPlainText(null, null));
            return;
        }

        try {
            mClipboardManager.clearPrimaryClip();
        } catch (Exception e) {
            // Fall back to set an empty string to the clipboard.
            setPrimaryClipNoException(ClipData.newPlainText(null, null));
            return;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean setPrimaryClipNoException(ClipData clip) {
        final String manufacturer = Build.MANUFACTURER.toLowerCase(Locale.US);
        // See crbug.com/1123727, there are OEM devices having strict mode violations in their
        // Android framework code. Disabling strict mode for non-google devices.
        try (StrictModeContext ignored =
                manufacturer.equals("google") ? null : StrictModeContext.allowAllThreadPolicies()) {
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

    @Override
    public void setImageFileProvider(ImageFileProvider imageFileProvider) {
        mImageFileProvider = imageFileProvider;

        if (mPendingCopiedImageMetadata != null) {
            mImageFileProvider.storeLastCopiedImageMetadata(mPendingCopiedImageMetadata);
            mPendingCopiedImageMetadata = null;
        }
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
        notifyPrimaryClipChanged();
    }

    @Override
    public void copyUrlToClipboard(GURL url) {
        ClipData clip =
                new ClipData(
                        "url",
                        new String[] {URL_MIME_TYPE, PLAIN_TEXT_MIME_TYPE},
                        new ClipData.Item(url.getSpec()));
        if (setPrimaryClipNoException(clip)) {
            showToastIfNeeded(R.string.link_copied);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (!hasFocus || Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return;
        }
        onPrimaryClipTimestampInvalidated();
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private void onPrimaryClipTimestampInvalidated() {
        ClipDescription clipDescription = mClipboardManager.getPrimaryClipDescription();
        if (clipDescription == null) return;

        long timestamp = clipDescription.getTimestamp();
        notifyPrimaryClipTimestampInvalidated(timestamp);
    }

    @Override
    public long getLastModifiedTimeMs() {
        return getLastModifiedTimeToJavaTime();
    }

    /**
     * Grant permission to access a specific Uri to other packages. For sharing images through the
     * system’s clipboard, Outside of Android O permissions are already managed properly by the
     * system. But on Android O, sharing images/files needs to grant permission to each app/packages
     * individually. Note: Don't forget to revoke the permission once the clipboard is updated.
     */
    @SuppressWarnings("QueryPermissionsNeeded")
    private void grantUriPermission(@NonNull Uri uri) {
        if ((Build.VERSION.SDK_INT != Build.VERSION_CODES.O
                        && Build.VERSION.SDK_INT != Build.VERSION_CODES.O_MR1)
                || mImageFileProvider == null) {
            return;
        }

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

        ImageFileProvider.ClipboardFileMetadata imageMetadata =
                mImageFileProvider.getLastCopiedImageMetadata();
        // Exit early if the URI is empty or event onPrimaryClipChanges was caused by sharing
        // image.
        if (imageMetadata == null
                || imageMetadata.uri == null
                || imageMetadata.uri.equals(Uri.EMPTY)
                || imageMetadata.uri.equals(getImageUri())) {
            return;
        }

        // https://developer.android.com/reference/android/content/Context#revokeUriPermission(android.net.Uri,%20int)
        // According to the above link, it is not necessary to enumerate all of the packages like
        // what was done in |grantUriPermission|. Context#revokeUriPermission(Uri, int) will revoke
        // all permissions.
        mContext.revokeUriPermission(imageMetadata.uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        // Clear uri to avoid revoke over and over.
        mImageFileProvider.clearLastCopiedImageMetadata();
    }

    /**
     * Allows the ClipboardManager Android Service to be replaced with a mock for tests, returning
     * the original so that it can be restored.
     */
    public ClipboardManager overrideClipboardManagerForTesting(ClipboardManager manager) {
        ClipboardManager oldManager = mClipboardManager;
        mClipboardManager = manager;
        return oldManager;
    }

    @Override
    public boolean canPaste() {
        return mClipboardManager.hasPrimaryClip();
    }

    @Override
    public boolean canCopy() {
        return true;
    }

    /**
     * Check Whether the ClipDescription has stypled text.
     *
     * @param description The {@link ClipDescription} to check if it has stytled text.
     * @return True if the system clipboard contain a styled text, otherwise, false.
     */
    private boolean hasStyledText(ClipDescription description) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return description.isStyledText();
        } else {
            return hasStyledTextOnPreS();
        }
    }

    private boolean hasStyledTextOnPreS() {
        CharSequence text;
        try {
            // getPrimaryClip() has been observed to throw unexpected exceptions for some devices
            // (see crbug.com/654802 and b/31501780)
            text = mClipboardManager.getPrimaryClip().getItemAt(0).getText();
        } catch (Exception e) {
            return false;
        }

        if (text instanceof Spanned) {
            Spanned spanned = (Spanned) text;
            return hasStyleSpan(spanned);
        }

        return false;
    }

    /**
     * Conditionally show a toast to avoid duplicate notifications in Android 13+
     * https://developer.android.com/develop/ui/views/touch-and-input/copy-paste#duplicate-notifications
     */
    private void showToastIfNeeded(@StringRes int stringId) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.S_V2) return;
        Toast.makeText(mContext, stringId, Toast.LENGTH_SHORT).show();
    }

    public static void setSkipImageMimeTypeCheckForTesting(Boolean doSkip) {
        ResettersForTesting.register(() -> sSkipImageMimeTypeCheckForTesting = null);
        sSkipImageMimeTypeCheckForTesting = doSkip;
    }
}
