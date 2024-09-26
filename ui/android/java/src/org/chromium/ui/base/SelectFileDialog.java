// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.webkit.MimeTypeMap;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileProviderUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.ui.R;
import org.chromium.ui.UiUtils;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * A dialog that is triggered from a file input field that allows a user to select a file based on
 * a set of accepted file types. The path of the selected file is passed to the native dialog.
 */
@JNINamespace("ui")
public class SelectFileDialog implements WindowAndroid.IntentCallback, PhotoPickerListener {
    private static final String TAG = "SelectFileDialog";
    private static final String IMAGE_TYPE = "image";
    private static final String VIDEO_TYPE = "video";
    private static final String AUDIO_TYPE = "audio";
    private static final String ALL_TYPES = "*/*";
    private static final String GENERIC_TYPE = "application/octet-stream";

    // Duration before temporary camera file is cleaned up, in milliseconds.
    private static final long DURATION_BEFORE_FILE_CLEAN_UP_IN_MILLIS = TimeUnit.HOURS.toMillis(1);

    // A list of some of the more popular image extensions. Not meant to be
    // exhaustive, but should cover the vast majority of image types.
    private static final String[] POPULAR_IMAGE_EXTENSIONS =
            new String[] {
                ".apng", ".bmp", ".gif", ".jpeg", ".jpg", ".png", ".tif", ".tiff", ".xcf", ".webp"
            };

    // A list of some of the more popular video extensions. Not meant to be
    // exhaustive, but should cover the vast majority of video types.
    private static final String[] POPULAR_VIDEO_EXTENSIONS =
            new String[] {
                ".asf", ".avhcd", ".avi", ".divx", ".flv", ".mov", ".mp4", ".mpeg", ".mpg", ".swf",
                ".wmv", ".webm", ".mkv"
            };

    /**
     * The SELECT_FILE_DIALOG_SCOPE_* enumerations are used to measure the sort of content that
     * developers are requesting to be shown in the select file dialog. Values must be kept in sync
     * with their definition in //tools/metrics/histograms/histograms.xml, and both the numbering
     * and meaning of the values must remain constant as they're recorded by UMA.
     *
     * Values are package visible because they're tested in the SelectFileDialogTest junit test.
     */
    static final int SELECT_FILE_DIALOG_SCOPE_GENERIC = 0;

    static final int SELECT_FILE_DIALOG_SCOPE_IMAGES = 1;
    static final int SELECT_FILE_DIALOG_SCOPE_VIDEOS = 2;
    static final int SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS = 3;
    static final int SELECT_FILE_DIALOG_SCOPE_COUNT =
            SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS + 1;

    /**
     * The Android Media Picker enumerations, used to measure which type of picker is shown to the
     * user. Values must be kept in sync with their definition in
     * //tools/metrics/histograms/histograms.xml, and both the numbering and meaning of the values
     * must remain constant as they're recorded by UMA.
     */
    static final int SHOWING_CHROME_PICKER = 0;

    static final int SHOWING_ANDROID_PICKER_DIRECT = 1;
    static final int SHOWING_SUPPRESSED = 2;
    static final int SHOWING_ANDROID_PICKER_INDIRECT = 3;
    static final int SHOWING_ENUM_COUNT = SHOWING_ANDROID_PICKER_INDIRECT + 1;

    /**
     * The FileSelectedUploadMethod tracks how media files are uploaded, split into the MediaPicker
     * and an external source (such as the Android Files app). These values are persisted to logs.
     * Entries should not be renumbered and numeric values should never be reused.
     */
    @IntDef({
        FileSelectedUploadMethod.MEDIA_PICKER_IMAGE,
        FileSelectedUploadMethod.MEDIA_PICKER_VIDEO,
        FileSelectedUploadMethod.MEDIA_PICKER_OTHER,
        FileSelectedUploadMethod.MEDIA_PICKER_UNKNOWN_TYPE,
        FileSelectedUploadMethod.EXTERNAL_PICKER_IMAGE,
        FileSelectedUploadMethod.EXTERNAL_PICKER_VIDEO,
        FileSelectedUploadMethod.EXTERNAL_PICKER_OTHER,
        FileSelectedUploadMethod.EXTERNAL_PICKER_UNKNOWN_TYPE,
        FileSelectedUploadMethod.COUNT,
    })
    protected @interface FileSelectedUploadMethod {
        // An image was picked using the Media Picker.
        int MEDIA_PICKER_IMAGE = 0;
        // A video was picked using the Media Picker.
        int MEDIA_PICKER_VIDEO = 1;
        // Something other than a video/photo was picked using the Media Picker.
        int MEDIA_PICKER_OTHER = 2;
        // Unable to determine type of file picked using the Media Picker.
        int MEDIA_PICKER_UNKNOWN_TYPE = 3;
        // An image was picked using an external source.
        int EXTERNAL_PICKER_IMAGE = 4;
        // A video was picked using an external source.
        int EXTERNAL_PICKER_VIDEO = 5;
        // Something other than a video/photo was picked using an external source.
        int EXTERNAL_PICKER_OTHER = 6;
        // Unable to determine type of file picked using an external source.
        int EXTERNAL_PICKER_UNKNOWN_TYPE = 7;

        // Keeps track of the number of options above. Must be the highest number.
        int COUNT = 8;
    }

    /**
     * The FileSelectAction tracks how many media files were uploaded, using either the MediaPicker
     * or an external source (such as the Android picker). These values are persisted to logs.
     * Entries should not be renumbered and numeric values should never be reused.
     */
    @IntDef({
        FileSelectedAction.MEDIA_PICKER_IMAGE_BY_MIME_TYPE,
        FileSelectedAction.MEDIA_PICKER_VIDEO_BY_MIME_TYPE,
        FileSelectedAction.MEDIA_PICKER_OTHER_BY_MIME_TYPE,
        FileSelectedAction.MEDIA_PICKER_IMAGE_BY_EXTENSION,
        FileSelectedAction.MEDIA_PICKER_VIDEO_BY_EXTENSION,
        FileSelectedAction.MEDIA_PICKER_OTHER_BY_EXTENSION,
        FileSelectedAction.MEDIA_PICKER_UNKNOWN_TYPE,
        FileSelectedAction.EXTERNAL_PICKER_IMAGE_BY_MIME_TYPE,
        FileSelectedAction.EXTERNAL_PICKER_VIDEO_BY_MIME_TYPE,
        FileSelectedAction.EXTERNAL_PICKER_OTHER_BY_MIME_TYPE,
        FileSelectedAction.EXTERNAL_PICKER_IMAGE_BY_EXTENSION,
        FileSelectedAction.EXTERNAL_PICKER_VIDEO_BY_EXTENSION,
        FileSelectedAction.EXTERNAL_PICKER_OTHER_BY_EXTENSION,
        FileSelectedAction.EXTERNAL_PICKER_UNKNOWN_TYPE,
        FileSelectedAction.COUNT,
    })
    protected @interface FileSelectedAction {
        // MediaPicker was used to pick a photo, as determined by its MIME type.
        int MEDIA_PICKER_IMAGE_BY_MIME_TYPE = 0;

        // MediaPicker was used to pick a video, as determined by its MIME type.
        int MEDIA_PICKER_VIDEO_BY_MIME_TYPE = 1;

        // MediaPicker was used to pick a file, but the ContentResolver returned a MIME type that
        // corresponds to neither an image, nor a video. This is not expected to happen, unless more
        // formats are added to the MediaPicker.
        int MEDIA_PICKER_OTHER_BY_MIME_TYPE = 2;

        // MediaPicker was used to pick a photo, as determined by its file extension. This is
        // primarily for images fresh off of the camera (where the ContentResolver doesn't know
        // know the MIME type). It may also catch corner cases where the user picked an existing
        // photo in the MediaPicker but the ContentResolver didn't know its MIME type. It is
        // unlikely, though, because those URIs don't normally contain the extension (except when
        // camera is the source), so these would more likely show up as MEDIA_PICKER_UNKNOWN_TYPE.
        int MEDIA_PICKER_IMAGE_BY_EXTENSION = 3;

        // Same comment applies as for MEDIA_PICKER_VIDEO_BY_EXTENSION, except in this case for a
        // video. Please note though that, in the emulator, the ContentResolver *is* able to lookup
        // the MIME types for videos (where it fails to do so for photos), so videos may be counted
        // as MEDIA_PICKER_VIDEO_BY_MIME_TYPE instead.
        int MEDIA_PICKER_VIDEO_BY_EXTENSION = 4;

        // MediaPicker was used to pick something other than a video/photo, as determined by the
        // file extension. This is not expected to happen, unless more formats are added to the
        // MediaPicker.
        int MEDIA_PICKER_OTHER_BY_EXTENSION = 5;

        // MediaPicker was used, but neither the MIME type nor the extension provided clues as to
        // what type of file it was (or the URI was null).
        int MEDIA_PICKER_UNKNOWN_TYPE = 6;

        // An external source (Android intent) was used to pick a photo, as determined by its MIME
        // type.
        int EXTERNAL_PICKER_IMAGE_BY_MIME_TYPE = 7;

        // An external source (Android intent) was used to pick a video, as determined by its MIME
        // type.
        int EXTERNAL_PICKER_VIDEO_BY_MIME_TYPE = 8;

        // An external source (Android intent) was used to pick something other than a video/photo,
        // as determined by the MIME type.
        int EXTERNAL_PICKER_OTHER_BY_MIME_TYPE = 9;

        // An external source (Android intent) was used to pick a photo, as determined by its file
        // extension.
        int EXTERNAL_PICKER_IMAGE_BY_EXTENSION = 10;

        // An external source (Android intent) was used to pick a video, as determined by its file
        // extension.
        int EXTERNAL_PICKER_VIDEO_BY_EXTENSION = 11;

        // An external source (Android intent) was used to pick something other than a video/photo,
        // as determined by its file extension.
        int EXTERNAL_PICKER_OTHER_BY_EXTENSION = 12;

        // An external source (Android intent) was used, but neither the MIME type nor the file
        // extension provided clues as to what type of file it was (or the URI was null).
        int EXTERNAL_PICKER_UNKNOWN_TYPE = 13;

        // Keeps track of the number of options above. Must be the highest number.
        int COUNT = 14;
    }

    /** If set, overrides the WindowAndroid passed in {@link selectFile()}. */
    @SuppressLint("StaticFieldLeak")
    private static WindowAndroid sWindowAndroidForTesting;

    private long mNativeSelectFileDialog;
    private String mIntentAction;
    // File types may contain both file extensions and MIME types.
    private List<String> mFileTypes;
    // Converted from `mFileTypes`, only contains deduped MIME types.
    private List<String> mMimeTypes;
    private boolean mCapture;
    private boolean mAllowMultiple;
    private Uri mCameraOutputUri;
    private WindowAndroid mWindowAndroid;

    /** Whether an Activity is available on the system to support capturing images (i.e. Camera). */
    private boolean mSupportsImageCapture;

    /**
     * Whether an Activity is available to capture video (i.e. Camera with video recording
     * capabilities).
     */
    private boolean mSupportsVideoCapture;

    /** Whether an Activity is available to capture audio. */
    private boolean mSupportsAudioCapture;

    /**
     * Keeps track of whether the MediaPicker was used to upload files. The can be true while the
     * MediaPicker is showing, and flip to false if the user opts to use the 'Browse' escape hatch,
     * to use the stock Android picker.
     */
    private boolean mMediaPickerWasUsed;

    /** A delegate for the photo picker. */
    private static PhotoPickerDelegate sPhotoPickerDelegate;

    /** The active photo picker, or null if none is active. */
    private static PhotoPicker sPhotoPicker;

    /**
     * Allows setting a delegate to override the default Android stock photo picker.
     * @param delegate A {@link PhotoPickerDelegate} instance.
     */
    public static void setPhotoPickerDelegate(PhotoPickerDelegate delegate) {
        sPhotoPickerDelegate = delegate;
    }

    @VisibleForTesting
    SelectFileDialog(long nativeSelectFileDialog) {
        mNativeSelectFileDialog = nativeSelectFileDialog;
    }

    /** Overrides the WindowAndroid passed in {@link selectFile()}. */
    public static void setWindowAndroidForTests(WindowAndroid window) {
        sWindowAndroidForTesting = window;
        ResettersForTesting.register(() -> sWindowAndroidForTesting = null);
    }

    /** Overrides the list of accepted file types for testing purposes. */
    public void setFileTypesForTests(List<String> fileTypes) {
        List<String> oldValue = mFileTypes;
        mFileTypes = fileTypes;
        ResettersForTesting.register(() -> mFileTypes = oldValue);
        mMimeTypes = convertToSupportedMimeTypes(mFileTypes);
    }

    /**
     * Creates and starts an intent based on the passed fileTypes and capture value.
     *
     * @param intentAction Intent action such as ACTION_GET_CONTENT.
     * @param fileTypes MIME types requested (i.e. "image/*")
     * @param capture The capture value as described in http://www.w3.org/TR/html-media-capture/
     * @param multiple Whether it should be possible to select multiple files.
     * @param window The WindowAndroid that can show intents
     */
    @CalledByNative
    protected void selectFile(
            String intentAction,
            String[] fileTypes,
            boolean capture,
            boolean multiple,
            WindowAndroid window) {
        mIntentAction =
                UiAndroidFeatureMap.isEnabled(UiAndroidFeatures.SELECT_FILE_OPEN_DOCUMENT)
                        ? intentAction
                        : Intent.ACTION_GET_CONTENT;
        mFileTypes = new ArrayList<String>(Arrays.asList(fileTypes));
        mMimeTypes = convertToSupportedMimeTypes(mFileTypes);
        mCapture = capture;
        mAllowMultiple = multiple;
        mWindowAndroid = (sWindowAndroidForTesting == null) ? window : sWindowAndroidForTesting;

        // No mime types or extra choosers needed for open-directory.
        if (Intent.ACTION_OPEN_DOCUMENT_TREE.equals(mIntentAction)) {
            Intent intent = new Intent(mIntentAction);
            if (!mWindowAndroid.showIntent(intent, this, R.string.low_memory_error)) {
                onFileNotSelected();
            }
            return;
        }

        mSupportsImageCapture =
                mWindowAndroid.canResolveActivity(new Intent(MediaStore.ACTION_IMAGE_CAPTURE));
        mSupportsVideoCapture =
                mWindowAndroid.canResolveActivity(new Intent(MediaStore.ACTION_VIDEO_CAPTURE));
        mSupportsAudioCapture =
                mWindowAndroid.canResolveActivity(
                        new Intent(MediaStore.Audio.Media.RECORD_SOUND_ACTION));

        List<String> missingPermissions = new ArrayList<>();
        String storagePermission = Manifest.permission.READ_EXTERNAL_STORAGE;
        boolean shouldUsePhotoPicker = shouldUsePhotoPicker();
        if (shouldUsePhotoPicker) {
            // The permission scenario for accessing media has evolved a bit over the years:
            // Early on, READ_EXTERNAL_STORAGE was required to access media, but that permission was
            // later deprecated. In its place (starting with Android T) READ_MEDIA_IMAGES and
            // READ_MEDIA_VIDEO were required. To make matters more interesting, a native Android
            // Media Picker was also introduced at the same time, but it functions without requiring
            // Chrome to request any permission.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                if (!preferAndroidMediaPicker()) {
                    if (!window.hasPermission(Manifest.permission.READ_MEDIA_IMAGES)
                            && shouldShowImageTypes()) {
                        missingPermissions.add(Manifest.permission.READ_MEDIA_IMAGES);
                    }
                    if (!window.hasPermission(Manifest.permission.READ_MEDIA_VIDEO)
                            && shouldShowVideoTypes()) {
                        missingPermissions.add(Manifest.permission.READ_MEDIA_VIDEO);
                    }
                }
            } else {
                if (!window.hasPermission(storagePermission)) {
                    missingPermissions.add(storagePermission);
                }
            }
        } else {
            if (((mSupportsImageCapture && shouldShowImageTypes())
                            || (mSupportsVideoCapture && shouldShowVideoTypes()))
                    && !window.hasPermission(Manifest.permission.CAMERA)) {
                missingPermissions.add(Manifest.permission.CAMERA);
            }
            if (mSupportsAudioCapture
                    && shouldShowAudioTypes()
                    && !window.hasPermission(Manifest.permission.RECORD_AUDIO)) {
                missingPermissions.add(Manifest.permission.RECORD_AUDIO);
            }
        }

        if (missingPermissions.isEmpty()) {
            launchSelectFileIntent();
        } else {
            String[] requestPermissions =
                    missingPermissions.toArray(new String[missingPermissions.size()]);
            window.requestPermissions(
                    requestPermissions,
                    (permissions, grantResults) -> {
                        for (int i = 0; i < grantResults.length; i++) {
                            if (grantResults[i] == PackageManager.PERMISSION_DENIED) {
                                if (mCapture) {
                                    onFileNotSelected();
                                    return;
                                }

                                // TODO(finnur): Remove once we figure out the cause of
                                // crbug.com/950024.
                                if (shouldUsePhotoPicker) {
                                    if (permissions.length != requestPermissions.length) {
                                        throw new RuntimeException(
                                                String.format(
                                                        "Permissions arrays misaligned: %d != %d",
                                                        permissions.length,
                                                        requestPermissions.length));
                                    }

                                    if (!permissions[i].equals(requestPermissions[i])) {
                                        throw new RuntimeException(
                                                String.format(
                                                        "Permissions arrays don't match: %s != %s",
                                                        permissions[i], requestPermissions[i]));
                                    }
                                }

                                if (shouldUsePhotoPicker) {
                                    if (permissions[i].equals(storagePermission)
                                            || permissions[i].equals(
                                                    Manifest.permission.READ_MEDIA_IMAGES)
                                            || permissions[i].equals(
                                                    Manifest.permission.READ_MEDIA_VIDEO)) {
                                        WindowAndroid.showError(R.string.permission_denied_error);
                                        onFileNotSelected();
                                        return;
                                    }
                                }
                            }
                        }
                        launchSelectFileIntent();
                    });
        }
    }

    /** Called to launch an intent to allow user to select files. */
    private void launchSelectFileIntent() {
        boolean hasCameraPermission = mWindowAndroid.hasPermission(Manifest.permission.CAMERA);
        if (mSupportsImageCapture && hasCameraPermission) {
            // GetCameraIntentTask will call LaunchSelectFileWithCameraIntent later.
            new GetCameraIntentTask(false, mWindowAndroid, this)
                    .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } else {
            launchSelectFileWithCameraIntent(null);
        }
    }

    /** Returns an Image capture Intent with the right flags and extra data. */
    private Intent getImageCaptureIntent() {
        Intent camera = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        camera.setFlags(
                Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        camera.putExtra(MediaStore.EXTRA_OUTPUT, mCameraOutputUri);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            // ClipData.newUri may access the disk (for reading mime types).
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                camera.setClipData(
                        ClipData.newUri(
                                ContextUtils.getApplicationContext().getContentResolver(),
                                UiUtils.IMAGE_FILE_PATH,
                                mCameraOutputUri));
            }
        }
        return camera;
    }

    /**
     * Returns a Video capture Intent. Can return null if video capture is not supported or the
     * camera permission has not been granted.
     */
    @Nullable
    private Intent getVideoCaptureIntent() {
        boolean hasCameraPermission = mWindowAndroid.hasPermission(Manifest.permission.CAMERA);
        if (mSupportsVideoCapture && hasCameraPermission) {
            return new Intent(MediaStore.ACTION_VIDEO_CAPTURE);
        }
        return null;
    }

    /**
     * Returns a SoundRecorder Intent. Can return null if sound capture is not supported or the
     * sound permission has not been granted.
     */
    @Nullable
    private Intent getSoundRecorderIntent() {
        boolean hasAudioPermission = mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO);
        if (mSupportsAudioCapture && hasAudioPermission) {
            return new Intent(MediaStore.Audio.Media.RECORD_SOUND_ACTION);
        }
        return null;
    }

    /**
     * Called to launch an intent to allow user to select files. If |camera| is null,
     * the select file dialog shouldn't include any files from the camera. Otherwise, user
     * is allowed to choose files from the camera.
     * @param camera Intent for selecting files from camera.
     */
    private void launchSelectFileWithCameraIntent(Intent camera) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.SelectFileDialogScope",
                determineSelectFileDialogScope(),
                SELECT_FILE_DIALOG_SCOPE_COUNT);

        Intent videoCapture = getVideoCaptureIntent();
        Intent soundRecorder = getSoundRecorderIntent();

        // Quick check - if the |capture| parameter is set and |fileTypes| has the appropriate MIME
        // type, we should just launch the appropriate intent. Otherwise build up a chooser based
        // on the accept type and then display that to the user.
        if (captureImage() && camera != null) {
            if (mWindowAndroid.showIntent(camera, this, R.string.low_memory_error)) return;
        } else if (captureVideo() && videoCapture != null) {
            if (mWindowAndroid.showIntent(videoCapture, this, R.string.low_memory_error)) return;
        } else if (captureAudio() && soundRecorder != null) {
            if (mWindowAndroid.showIntent(soundRecorder, this, R.string.low_memory_error)) return;
        }

        // Use the new photo picker, if available.
        if (shouldUsePhotoPicker()
                && showPhotoPicker(
                        mWindowAndroid,
                        /* intentCallback= */ this,
                        /* listener= */ this,
                        mAllowMultiple,
                        mMimeTypes)) {
            mMediaPickerWasUsed = true;
            return;
        } else {
            mMediaPickerWasUsed = false;
            if (!shouldUsePhotoPicker()) {
                logMediaPickerShown(SHOWING_SUPPRESSED);
            }
        }

        showExternalPicker(camera, videoCapture, soundRecorder);
    }

    /**
     * Launches a chooser intent to get files from an external source. If launching the Intent is
     * not successful, the onFileNotSelected is called to end file upload.
     * @param camera A camera capture intent to supply as extra Intent data.
     * @param camcorder A camcorder intent to supply as extra Intent data.
     * @param soundRecorder A soundRecorder intent to supply as extra Intent data.
     */
    private void showExternalPicker(Intent camera, Intent camcorder, Intent soundRecorder) {
        if (UiAndroidFeatureMap.isEnabled(UiAndroidFeatures.DEPRECATED_EXTERNAL_PICKER_FUNCTION)) {
            showExternalPickerDeprecated(camera, camcorder, soundRecorder);
            return;
        }

        Intent getContentIntent = new Intent(mIntentAction);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2 && mAllowMultiple) {
            getContentIntent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }

        // Set to all types, and restrict further by MIME-type below.
        getContentIntent.setType(ALL_TYPES);

        if (mMimeTypes.size() > 0) {
            // If some of the extensions are generic, just let user selectall files.
            List<String> types =
                    mMimeTypes.contains(GENERIC_TYPE)
                            ? new ArrayList<>()
                            : new ArrayList<>(mMimeTypes);
            // Calls to ACTION_GET_CONTENT can result in the MediaPicker hijacking the call and
            // showing itself instead of the Files app, when only images or videos are provided.
            // This flow is not only confusing for the user (a MediaPicker on top of a MediaPicker?)
            // but also breaks our cloud media integration, which is currently provided via the
            // Files app. We therefore add a non-existent MIME-type to the mix, which the Files app
            // will ignore, but ensures the MediaPicker wont hijack the call.
            if (shouldShowImageTypes() || shouldShowVideoTypes()) {
                types.add("type/nonexistent");
            }

            if (!types.isEmpty()) {
                getContentIntent.putExtra(Intent.EXTRA_MIME_TYPES, types.toArray(new String[0]));
            }
        }

        ArrayList<Intent> extraIntents = new ArrayList<Intent>();
        if (shouldShowImageTypes() && camera != null) extraIntents.add(camera);
        if (shouldShowVideoTypes() && camcorder != null) extraIntents.add(camcorder);
        if (shouldShowAudioTypes() && soundRecorder != null) extraIntents.add(soundRecorder);

        // Only accept openable files, as coercing virtual files may yield to a MIME type different
        // than expected.
        getContentIntent.addCategory(Intent.CATEGORY_OPENABLE);

        Intent chooser = new Intent(Intent.ACTION_CHOOSER);
        if (!extraIntents.isEmpty()) {
            chooser.putExtra(Intent.EXTRA_INITIAL_INTENTS, extraIntents.toArray(new Intent[] {}));
        }
        chooser.putExtra(Intent.EXTRA_INTENT, getContentIntent);

        if (!mWindowAndroid.showIntent(chooser, this, R.string.low_memory_error)) {
            onFileNotSelected();
        }
    }

    /**
     * The deprecated way of launching a chooser intent to get files from an external source (use
     * showExternalPicker instead). If launching the Intent is not successful, the onFileNotSelected
     * is called to end file upload.
     *
     * @param camera A camera capture intent to supply as extra Intent data.
     * @param camcorder A camcorder intent to supply as extra Intent data.
     * @param soundRecorder A soundRecorder intent to supply as extra Intent data.
     */
    private void showExternalPickerDeprecated(
            Intent camera, Intent camcorder, Intent soundRecorder) {
        Intent getContentIntent = new Intent(mIntentAction);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2 && mAllowMultiple) {
            getContentIntent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }

        // Set to all types, but potentially restricted further by MIME-type below.
        getContentIntent.setType(ALL_TYPES);

        ArrayList<Intent> extraIntents = new ArrayList<Intent>();
        if (acceptsSingleType()) {
            // Attention: We should change the variable below to `mMimeTypes`. Using of `mFileTypes`
            // is discouraged because it may include both file and MIME types. We keep the current
            // variable just in case something goes wrong with the newly introduced `mMimeTypes` so
            // that we can switch back to the old implementation. Remove this method once the new
            // implementation is proven to work properly.
            List<String> types = new ArrayList<>(mFileTypes);
            // Calls to ACTION_GET_CONTENT can result in the MediaPicker hijacking the call and
            // showing itself instead of the Files app, when only images or videos are provided.
            // This flow is not only confusing for the user (a MediaPicker on top of a MediaPicker?)
            // but also breaks our cloud media integration, which is currently provided via the
            // Files app. We therefore add a non-existant MIME-type to the mix, which the Files app
            // will ignore, but ensures the MediaPicker wont hijack the call.
            String noOpMimeType = "type/nonexistent";

            // If one and only one category of accept type was specified (image, video, etc..),
            // then update the intent to specifically target that request.
            if (shouldShowImageTypes()) {
                if (camera != null) extraIntents.add(camera);
                types.add(noOpMimeType);
                getContentIntent.putExtra(Intent.EXTRA_MIME_TYPES, types.toArray(new String[0]));
            } else if (shouldShowVideoTypes()) {
                if (camcorder != null) extraIntents.add(camcorder);
                types.add(noOpMimeType);
                getContentIntent.putExtra(Intent.EXTRA_MIME_TYPES, types.toArray(new String[0]));
            } else if (shouldShowAudioTypes()) {
                if (soundRecorder != null) extraIntents.add(soundRecorder);
                getContentIntent.putExtra(Intent.EXTRA_MIME_TYPES, types.toArray(new String[0]));
            }

            // If any types are specified, then only accept openable files, as coercing
            // virtual files may yield to a MIME type different than expected.
            getContentIntent.addCategory(Intent.CATEGORY_OPENABLE);
        }

        Bundle extras = getContentIntent.getExtras();
        if (extras == null || extras.get(Intent.EXTRA_MIME_TYPES) == null) {
            // We couldn't resolve a single accept type, so fallback to a generic chooser.
            if (camera != null) extraIntents.add(camera);
            if (camcorder != null) extraIntents.add(camcorder);
            if (soundRecorder != null) extraIntents.add(soundRecorder);
        }

        Intent chooser = new Intent(Intent.ACTION_CHOOSER);
        if (!extraIntents.isEmpty()) {
            chooser.putExtra(Intent.EXTRA_INITIAL_INTENTS, extraIntents.toArray(new Intent[] {}));
        }
        chooser.putExtra(Intent.EXTRA_INTENT, getContentIntent);

        if (!mWindowAndroid.showIntent(chooser, this, R.string.low_memory_error)) {
            onFileNotSelected();
        }
    }

    /**
     * Determines whether the photo picker should be used for this select file request. To be
     * applicable for the photo picker, the following must be true:
     * 1.) Only media types were requested in the file request
     * 2.) The file request did not explicitly ask to capture camera directly.
     * 3.) The photo picker is supported by the embedder (i.e. Chrome).
     * 4.) There is a valid Android Activity associated with the file request.
     */
    private boolean shouldUsePhotoPicker() {
        return !captureImage()
                && isSupportedPhotoPickerTypes(mMimeTypes)
                && shouldShowPhotoPicker()
                && mWindowAndroid.getActivity().get() != null;
    }

    /**
     * Returns whether a list of MIME types are supported by photo picker.
     *
     * @param mimeTypes the list of MIME types to check.
     * @return true if all MIME types are supported by photo picker, or false otherwise.
     */
    @VisibleForTesting
    public static boolean isSupportedPhotoPickerTypes(List<String> mimeTypes) {
        if (mimeTypes.size() == 0) return false;
        for (String type : mimeTypes) {
            if (!type.startsWith("image/")) {
                if (!photoPickerSupportsVideo() || !type.startsWith("video/")) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Converts a list of extensions and MIME types to a list of de-duped MIME types. If the input
     * list contains a unsupported extension, "application/octet-stream" is returned as the MIME
     * tye.
     *
     * @param fileTypes the list of filetypes (extensions and Mime types) to convert.
     * @return A de-duped list of supported types only.
     */
    @VisibleForTesting
    public static List<String> convertToSupportedMimeTypes(List<String> fileTypes) {
        List<String> mimeTypes = new ArrayList<>();
        if (fileTypes.size() == 0) return mimeTypes;
        for (String type : fileTypes) {
            String mimeType = ensureMimeType(type);
            if (!mimeType.isEmpty() && !mimeTypes.contains(mimeType)) {
                mimeTypes.add(mimeType);
            }
        }
        return mimeTypes;
    }

    /**
     * Convert |type| to MIME type (known types only).
     *
     * @param type The type to convert. Can be either a MIME type or an extension (should include
     *     the leading dot). If an extension is passed in, it is converted to the corresponding MIME
     *     type (via {@link MimeTypeMap}), or "application/octet-stream" if the MIME type is not
     *     known.
     * @return The MIME type, if known, or "application/octet-stream" otherwise (or blank if
     *     extension is blank).
     */
    @VisibleForTesting
    public static String ensureMimeType(String type) {
        if (type.length() == 0) return "";

        if (isMimeType(type)) {
            return type;
        }

        String extension = MimeTypeMap.getFileExtensionFromUrl(type);
        if (extension.isEmpty()) return "";

        String mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
        if (mimeType != null) return mimeType;
        return GENERIC_TYPE;
    }

    @Override
    public void onPhotoPickerUserAction(@PhotoPickerAction int action, Uri[] photos) {
        switch (action) {
            case PhotoPickerAction.CANCEL:
                onFileNotSelected();
                break;

            case PhotoPickerAction.PHOTOS_SELECTED:
                if (photos.length == 0) {
                    onFileNotSelected();
                    return;
                }

                GetDisplayNameTask task =
                        new GetDisplayNameTask(
                                ContextUtils.getApplicationContext(), photos.length > 1, photos);
                task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                break;

            case PhotoPickerAction.LAUNCH_GALLERY:
                mMediaPickerWasUsed = false;
                showExternalPicker(
                        /* camera= */ null, /* camcorder= */ null, /* soundRecorder= */ null);
                break;

            case PhotoPickerAction.LAUNCH_CAMERA:
                if (!mWindowAndroid.hasPermission(Manifest.permission.CAMERA)) {
                    mWindowAndroid.requestPermissions(
                            new String[] {Manifest.permission.CAMERA},
                            (permissions, grantResults) -> {
                                if (grantResults.length == 0
                                        || grantResults[0] == PackageManager.PERMISSION_DENIED) {
                                    return;
                                }
                                assert grantResults.length == 1;
                                new GetCameraIntentTask(true, mWindowAndroid, this)
                                        .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                            });
                } else {
                    new GetCameraIntentTask(true, mWindowAndroid, this)
                            .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                }
                break;
        }
    }

    @Override
    public void onPhotoPickerDismissed() {
        assert sPhotoPicker != null;
        sPhotoPicker = null;
    }

    private class GetCameraIntentTask extends AsyncTask<Uri> {
        private Boolean mDirectToCamera;
        private WindowAndroid mWindow;
        private WindowAndroid.IntentCallback mCallback;

        public GetCameraIntentTask(
                Boolean directToCamera,
                WindowAndroid window,
                WindowAndroid.IntentCallback callback) {
            mDirectToCamera = directToCamera;
            mWindow = window;
            mCallback = callback;
        }

        @Override
        public Uri doInBackground() {
            try {
                Context context = ContextUtils.getApplicationContext();
                return FileProviderUtils.getContentUriFromFile(getFileForImageCapture(context));
            } catch (IOException e) {
                Log.e(TAG, "Cannot retrieve content uri from file", e);
                return null;
            }
        }

        @Override
        protected void onPostExecute(Uri result) {
            mCameraOutputUri = result;
            if (mCameraOutputUri == null) {
                if (captureImage() || mDirectToCamera) {
                    onFileNotSelected();
                } else {
                    launchSelectFileWithCameraIntent(null);
                }
                return;
            }

            if (mDirectToCamera) {
                // Android doesn't support launching an intent flexible enough to let the user
                // decide whether to record photos _or_ videos so, when both types are requested,
                // we choose one over the other. We currently default to photos, to maintain past
                // behavior, but should perhaps consider showing the user a chooser instead.
                Intent intent =
                        acceptsOnlyType(VIDEO_TYPE)
                                ? getVideoCaptureIntent()
                                : getImageCaptureIntent();
                mWindow.showIntent(intent, mCallback, R.string.low_memory_error);
            } else {
                launchSelectFileWithCameraIntent(getImageCaptureIntent());
            }
        }
    }

    /**
     * Get a file for the image capture operation. For devices with JB MR2 or
     * latter android versions, the file is put under IMAGE_FILE_PATH directory.
     * For ICS devices, the file is put under CAPTURE_IMAGE_DIRECTORY.
     *
     * @param context The application context.
     * @return file path for the captured image to be stored.
     */
    private File getFileForImageCapture(Context context) throws IOException {
        assert !ThreadUtils.runningOnUiThread();
        File photoFile =
                File.createTempFile(
                        String.valueOf(System.currentTimeMillis()),
                        ".jpg",
                        UiUtils.getDirectoryForImageCapture(context));
        return photoFile;
    }

    // TODO(crbug.com/41484704): Merge the Chrome and WebView implementations
    // of isPathUnderAppDir into one.
    private static boolean isPathUnderAppDir(String path, Context context) {
        File file = new File(path);
        File dataDir = ContextCompat.getDataDir(context);
        try {
            String pathCanonical = file.getCanonicalPath();
            String dataDirCanonical = dataDir.getCanonicalPath();
            return pathCanonical.startsWith(dataDirCanonical);
        } catch (Exception e) {
            return false;
        }
    }

    @RequiresApi(Build.VERSION_CODES.O)
    public static boolean isContentUriUnderAppDir(Uri uri, Context context) {
        assert !ThreadUtils.runningOnUiThread();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return false;
        }
        try {
            ParcelFileDescriptor pfd = context.getContentResolver().openFileDescriptor(uri, "r");
            int fd = pfd.getFd();
            // Use the file descriptor to find out the read file path thru symbolic link.
            Path fdPath = Paths.get("/proc/self/fd/" + fd);
            Path filePath = Files.readSymbolicLink(fdPath);
            return isPathUnderAppDir(filePath.toString(), context);
        } catch (Exception e) {
            return false;
        }
    }

    // WindowAndroid.IntentCallback:

    /**
     * Callback method to handle the intent results and pass on the path to the native
     * SelectFileDialog.
     *
     * @param resultCode The result code whether the intent returned successfully.
     * @param results The results of the requested intent.
     */
    @Override
    public void onIntentCompleted(int resultCode, Intent results) {
        if (sPhotoPicker != null) {
            sPhotoPicker.onExternalIntentCompleted();
        }

        if (resultCode != Activity.RESULT_OK) {
            onFileNotSelected();
            return;
        }

        if (results == null
                || (results.getData() == null
                        && (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2
                                || results.getClipData() == null))) {
            // If we have a successful return but no data, then assume this is the camera returning
            // the photo that we requested.
            // If the uri is a file, we need to convert it to the absolute path or otherwise
            // android cannot handle it correctly on some earlier versions.
            // http://crbug.com/423338.
            String path =
                    ContentResolver.SCHEME_FILE.equals(mCameraOutputUri.getScheme())
                            ? mCameraOutputUri.getPath()
                            : mCameraOutputUri.toString();

            if (!isPathUnderAppDir(
                    mCameraOutputUri.getSchemeSpecificPart(),
                    mWindowAndroid.getApplicationContext())) {
                onFileSelected(
                        mNativeSelectFileDialog, path, mCameraOutputUri.getLastPathSegment());
                // Broadcast to the media scanner that there's a new photo on the device so it will
                // show up right away in the gallery (rather than waiting until the next time the
                // media scanner runs).
                mWindowAndroid.sendBroadcast(
                        new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, mCameraOutputUri));
            } else {
                onFileNotSelected();
            }
            return;
        }

        // Path for when EXTRA_ALLOW_MULTIPLE Intent extra has been defined. Each of the selected
        // files will be shared as an entry on the Intent's ClipData. This functionality is only
        // available in Android JellyBean MR2 and higher.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2
                && results.getData() == null
                && results.getClipData() != null) {
            ClipData clipData = results.getClipData();

            int itemCount = clipData.getItemCount();
            if (itemCount == 0) {
                onFileNotSelected();
                return;
            }

            Uri[] filePathArray = new Uri[itemCount];
            for (int i = 0; i < itemCount; ++i) {
                filePathArray[i] = clipData.getItemAt(i).getUri();
                // Check if the caller has permission to access the uri if it is a content uri.
                if (ContentResolver.SCHEME_CONTENT.equals(filePathArray[i].getScheme())
                        && !doesCallerHavePermissionForUri(filePathArray[i])) {
                    onFileNotSelected();
                    return;
                }
            }
            GetDisplayNameTask task =
                    new GetDisplayNameTask(
                            ContextUtils.getApplicationContext(), true, filePathArray);
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            return;
        }

        if (ContentResolver.SCHEME_FILE.equals(results.getData().getScheme())) {
            String filePath = results.getData().getPath();
            if (!TextUtils.isEmpty(filePath)) {
                FilePathSelectedTask task =
                        new FilePathSelectedTask(
                                ContextUtils.getApplicationContext(), filePath, mWindowAndroid);
                task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                return;
            }
        }

        if (ContentResolver.SCHEME_CONTENT.equals(results.getScheme())) {
            Uri uri = results.getData();
            // Check if the caller has permission to access the uri.
            if (!doesCallerHavePermissionForUri(uri)) {
                onFileNotSelected();
                return;
            }
            if (UiAndroidFeatureMap.isEnabled(UiAndroidFeatures.SELECT_FILE_OPEN_DOCUMENT)) {
                ContentResolver cr = ContextUtils.getApplicationContext().getContentResolver();
                try {
                    cr.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                } catch (SecurityException e) {
                    Log.w(TAG, "No persisted read permission for " + uri);
                }
                try {
                    cr.takePersistableUriPermission(uri, Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                } catch (SecurityException e) {
                    Log.w(TAG, "No persisted write permission for " + uri);
                }
            }
            GetDisplayNameTask task =
                    new GetDisplayNameTask(
                            ContextUtils.getApplicationContext(), false, new Uri[] {uri});
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            return;
        }

        onFileNotSelected();
        WindowAndroid.showError(R.string.opening_file_error);
    }

    private void onFileNotSelected() {
        onFileNotSelected(mNativeSelectFileDialog);
    }

    // Determines the scope of the requested select file dialog for use in a UMA histogram. Right
    // now we want to distinguish between generic, photo and visual media pickers.
    @VisibleForTesting
    int determineSelectFileDialogScope() {
        if (mMimeTypes.size() == 0) return SELECT_FILE_DIALOG_SCOPE_GENERIC;

        // Capture the MIME types:
        int acceptsImages = countAcceptTypesFor(IMAGE_TYPE);
        int acceptsVideos = countAcceptTypesFor(VIDEO_TYPE);

        // Capture the most common image and video extensions:
        // TODO(b/365299139): This code below is probably wrong because mFileTypes may
        // contain MIME types instead of file extensions. Need to figure out the
        // right logic to count different types.
        if (mMimeTypes.size() > acceptsImages + acceptsVideos) {
            for (String left : mFileTypes) {
                boolean found = false;
                for (String right : POPULAR_IMAGE_EXTENSIONS) {
                    if (left.equalsIgnoreCase(right)) {
                        found = true;
                        acceptsImages++;
                        break;
                    }
                }

                if (found) continue;

                for (String right : POPULAR_VIDEO_EXTENSIONS) {
                    if (left.equalsIgnoreCase(right)) {
                        acceptsVideos++;
                        break;
                    }
                }
            }
        }

        int acceptsOthers = mFileTypes.size() - acceptsImages - acceptsVideos;

        if (acceptsOthers > 0) return SELECT_FILE_DIALOG_SCOPE_GENERIC;
        if (acceptsVideos > 0) {
            return (acceptsImages == 0)
                    ? SELECT_FILE_DIALOG_SCOPE_VIDEOS
                    : SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS;
        }
        return SELECT_FILE_DIALOG_SCOPE_IMAGES;
    }

    /**
     * Whether any of the types in `mMimeTypes` accepts the given type. If `mMimeTypes` contains
     * ALL_TYPES or is empty every type is accepted so always return true.
     *
     * @param superType The superType to look for, such as 'image' or 'video'. Note: This is
     *     string-matched on the prefix, so using generics as 'image/*' or '*' will not work.
     */
    private boolean acceptsType(String superType) {
        if (mMimeTypes.isEmpty() || mMimeTypes.contains(ALL_TYPES)) return true;
        return countAcceptTypesFor(superType) > 0;
    }

    /**
     * Whether all types in `mMimeTypes` accepts the given type.
     *
     * @param superType The superType to look for, such as 'image' or 'video'. Note: This is
     *     string-matched on the prefix, so using generics as 'image/*' or '*' will not work.
     */
    private boolean acceptsOnlyType(String superType) {
        return countAcceptTypesFor(superType) == mMimeTypes.size();
    }

    /**
     * Checks whether the list of accepted types effectively describes only a single type, which
     * might be wildcard. For example:
     *
     * <p>[image/jpeg] -> true: Only one type is specified. [image/jpeg, image/gif] -> false:
     * Contains two distinct types. [image/*, image/gif] -> true: image/gif already part of image/*.
     */
    @VisibleForTesting
    boolean acceptsSingleType() {
        // We use a single Intent to decide the type of the file chooser we display to the user,
        // which means we can only give it a single type. If there are multiple accept types
        // specified, we will fallback to a generic chooser (unless a capture parameter has been
        // specified, in which case we'll try to satisfy that first.
        if (mMimeTypes.size() == 1) return !mMimeTypes.contains(ALL_TYPES);
        // Also return true when a generic subtype "type/*" and one or more specific subtypes
        // "type/subtype" are listed but all still have the same supertype.
        // Ie. treat ["image/png", "image/*"] as if it said just ["image/*"].
        String superTypeFound = null;
        boolean foundGenericSubtype = false;
        for (String fileType : mMimeTypes) {
            int slash = fileType.indexOf('/');
            if (slash == -1) return false;
            String superType = fileType.substring(0, slash);
            boolean genericSubtype = fileType.substring(slash + 1).equals("*");
            if (superTypeFound == null) {
                superTypeFound = superType;
            } else if (!superTypeFound.equals(superType)) {
                // More than one type.
                return false;
            }
            if (genericSubtype) foundGenericSubtype = true;
        }
        return foundGenericSubtype;
    }

    @VisibleForTesting
    boolean shouldShowImageTypes() {
        return acceptsType(IMAGE_TYPE);
    }

    @VisibleForTesting
    boolean shouldShowVideoTypes() {
        return acceptsType(VIDEO_TYPE);
    }

    @VisibleForTesting
    boolean shouldShowAudioTypes() {
        return acceptsType(AUDIO_TYPE);
    }

    /**
     * Whether the HTML input field specified the 'capture' attribute and specifically requested
     * image capture.
     *
     * See https://www.w3.org/TR/html-media-capture/ for further description.
     */
    private boolean captureImage() {
        return mCapture && acceptsOnlyType(IMAGE_TYPE);
    }

    /**
     * Whether the HTML input field specified the 'capture' attribute and specifically requested
     * video capture.
     */
    private boolean captureVideo() {
        return mCapture && acceptsOnlyType(VIDEO_TYPE);
    }

    /**
     * Whether the HTML input field specified the 'capture' attribute and specifically requested
     * audio capture.
     */
    private boolean captureAudio() {
        return mCapture && acceptsOnlyType(AUDIO_TYPE);
    }

    private int countAcceptTypesFor(String superType) {
        assert superType.indexOf('/') == -1;
        int count = 0;
        for (String type : mMimeTypes) {
            if (type.startsWith(superType)) {
                count++;
            }
        }
        return count;
    }

    final class FilePathSelectedTask extends AsyncTask<Boolean> {
        final Context mContext;
        final String mFilePath;
        final WindowAndroid mWindow;

        public FilePathSelectedTask(Context context, String filePath, WindowAndroid window) {
            mContext = context;
            mFilePath = filePath;
            mWindow = window;
        }

        @Override
        public Boolean doInBackground() {
            // Don't allow invalid file path or files under app dir to be uploaded.
            return !isPathUnderAppDir(mFilePath, mContext)
                    && !FileUtils.getAbsoluteFilePath(mFilePath).isEmpty();
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (result) {
                onFileSelected(mNativeSelectFileDialog, mFilePath, "");
                WindowAndroid.showError(R.string.opening_file_error);
            } else {
                onFileNotSelected();
            }
        }
    }

    class GetDisplayNameTask extends AsyncTask<String[]> {
        String[] mFilePaths;
        final Context mContext;
        final boolean mIsMultiple;
        final Uri[] mUris;

        public GetDisplayNameTask(Context context, boolean isMultiple, Uri[] uris) {
            mContext = context;
            mIsMultiple = isMultiple;
            mUris = uris;
        }

        @Override
        @SuppressLint("NewApi")
        public String[] doInBackground() {
            mFilePaths = new String[mUris.length];
            String[] displayNames = new String[mUris.length];
            try {
                for (int i = 0; i < mUris.length; i++) {
                    // The selected files must be returned as a list of absolute paths. A MIUI 8.5
                    // device was observed to return a file:// URI instead, so convert if necessary.
                    // See https://crbug.com/752834 for context.
                    if (ContentResolver.SCHEME_FILE.equals(mUris[i].getScheme())) {
                        if (isPathUnderAppDir(mUris[i].getSchemeSpecificPart(), mContext)) {
                            return null;
                        }
                        mFilePaths[i] = mUris[i].getSchemeSpecificPart();
                    } else {
                        if (ContentResolver.SCHEME_CONTENT.equals(mUris[i].getScheme())
                                && isContentUriUnderAppDir(mUris[i], mContext)) {
                            return null;
                        }
                        mFilePaths[i] = mUris[i].toString();
                    }

                    displayNames[i] =
                            ContentUriUtils.getDisplayName(
                                    mUris[i], mContext, MediaStore.MediaColumns.DISPLAY_NAME);
                }
            } catch (SecurityException e) {
                // Some third party apps will present themselves as being able
                // to handle the ACTION_GET_CONTENT intent but then declare themselves
                // as exported=false (or more often omit the exported keyword in
                // the manifest which defaults to false after JB).
                // In those cases trying to access the contents raises a security exception
                // which we should not crash on. See crbug.com/382367 for details.
                Log.w(TAG, "Unable to extract results from the content provider");
                return null;
            }

            return displayNames;
        }

        @Override
        protected void onPostExecute(String[] result) {
            if (result == null) {
                onFileNotSelected();
                return;
            }
            if (mIsMultiple) {
                onMultipleFilesSelected(mNativeSelectFileDialog, mFilePaths, result);
            } else {
                onFileSelected(mNativeSelectFileDialog, mFilePaths[0], result[0]);
            }
        }
    }

    final class RecordUploadMetricsTask extends AsyncTask<Boolean> {
        final String[] mFilesSelected;
        final boolean mMediaPickerWasUsed;
        final ContentResolver mContentResolver;

        public RecordUploadMetricsTask(
                ContentResolver contentResolver,
                String[] filesSelected,
                boolean mediaPickerWasUsed) {
            mContentResolver = contentResolver;
            mFilesSelected = filesSelected;
            mMediaPickerWasUsed = mediaPickerWasUsed;
        }

        @Override
        public Boolean doInBackground() {
            for (String path : mFilesSelected) {
                // The |path| variable will now contain a content URI such as:
                //   content://media/external/file/1234
                //   content://com.android.providers.media.documents/document/image%3A1234
                //   content://org.chromium.chrome.FileProvider/images/1234.jpg
                // The first is an example URI from Chrome's MediaPicker, the second from the stock
                // Android picker and the third is from the camera (when taking a photo). All
                // obtained using the Emulator.
                logFileSelectedAction(
                        getMediaType(Uri.parse(path), mMediaPickerWasUsed, mContentResolver));
            }
            return true;
        }

        @Override
        protected void onPostExecute(Boolean result) {}
    }

    protected RecordUploadMetricsTask getUploadMetricTaskForTesting(
            ContentResolver contentResolver, String[] filesSelected, boolean mediaPickerWasUsed) {
        return new RecordUploadMetricsTask(contentResolver, filesSelected, mediaPickerWasUsed);
    }

    /** Clears all captured camera files. */
    public static void clearCapturedCameraFiles() {
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    try {
                        File path =
                                UiUtils.getDirectoryForImageCapture(
                                        ContextUtils.getApplicationContext());
                        if (!path.isDirectory()) return;
                        File[] files = path.listFiles();
                        if (files == null) return;
                        long now = System.currentTimeMillis();
                        for (File file : files) {
                            if (now - file.lastModified()
                                    > DURATION_BEFORE_FILE_CLEAN_UP_IN_MILLIS) {
                                if (!file.delete()) Log.e(TAG, "Failed to delete: " + file);
                            }
                        }
                    } catch (IOException e) {
                        Log.w(TAG, "Failed to delete captured camera files.", e);
                    }
                });
    }

    protected void onFileSelected(
            long nativeSelectFileDialogImpl, String filePath, String displayName) {
        recordImageCountHistograms(new String[] {filePath});
        if (nativeSelectFileDialogImpl != 0) {
            SelectFileDialogJni.get()
                    .onFileSelected(
                            nativeSelectFileDialogImpl,
                            SelectFileDialog.this,
                            filePath,
                            displayName);
        }
    }

    protected void onMultipleFilesSelected(
            long nativeSelectFileDialogImpl, String[] filePathArray, String[] displayNameArray) {
        recordImageCountHistograms(filePathArray);
        if (nativeSelectFileDialogImpl != 0) {
            SelectFileDialogJni.get()
                    .onMultipleFilesSelected(
                            nativeSelectFileDialogImpl,
                            SelectFileDialog.this,
                            filePathArray,
                            displayNameArray);
        }
    }

    protected void onFileNotSelected(long nativeSelectFileDialogImpl) {
        recordImageCountHistograms(new String[] {});
        if (nativeSelectFileDialogImpl != 0) {
            SelectFileDialogJni.get()
                    .onFileNotSelected(nativeSelectFileDialogImpl, SelectFileDialog.this);
        }
    }

    private void recordImageCountHistograms(String[] filesSelected) {
        if (isSupportedPhotoPickerTypes(mMimeTypes)) {
            // Record the total number of images selected via the Chrome Media Picker.
            RecordHistogram.recordCount100Histogram(
                    "Android.SelectFileDialogImgCount", filesSelected.length);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            new RecordUploadMetricsTask(
                            ContextUtils.getApplicationContext().getContentResolver(),
                            filesSelected,
                            mMediaPickerWasUsed)
                    .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }

    // This function returns the method used to upload, by looking it up from
    // mediaType details.
    private static int getUploadMethod(@FileSelectedAction int mediaType) {
        switch (mediaType) {
            case FileSelectedAction.MEDIA_PICKER_IMAGE_BY_MIME_TYPE:
            case FileSelectedAction.MEDIA_PICKER_IMAGE_BY_EXTENSION:
                return FileSelectedUploadMethod.MEDIA_PICKER_IMAGE;
            case FileSelectedAction.MEDIA_PICKER_VIDEO_BY_MIME_TYPE:
            case FileSelectedAction.MEDIA_PICKER_VIDEO_BY_EXTENSION:
                return FileSelectedUploadMethod.MEDIA_PICKER_VIDEO;
            case FileSelectedAction.MEDIA_PICKER_OTHER_BY_MIME_TYPE:
            case FileSelectedAction.MEDIA_PICKER_OTHER_BY_EXTENSION:
                return FileSelectedUploadMethod.MEDIA_PICKER_OTHER;
            case FileSelectedAction.MEDIA_PICKER_UNKNOWN_TYPE:
                return FileSelectedUploadMethod.MEDIA_PICKER_UNKNOWN_TYPE;
            case FileSelectedAction.EXTERNAL_PICKER_IMAGE_BY_MIME_TYPE:
            case FileSelectedAction.EXTERNAL_PICKER_IMAGE_BY_EXTENSION:
                return FileSelectedUploadMethod.EXTERNAL_PICKER_IMAGE;
            case FileSelectedAction.EXTERNAL_PICKER_VIDEO_BY_MIME_TYPE:
            case FileSelectedAction.EXTERNAL_PICKER_VIDEO_BY_EXTENSION:
                return FileSelectedUploadMethod.EXTERNAL_PICKER_VIDEO;
            case FileSelectedAction.EXTERNAL_PICKER_OTHER_BY_MIME_TYPE:
            case FileSelectedAction.EXTERNAL_PICKER_OTHER_BY_EXTENSION:
                return FileSelectedUploadMethod.EXTERNAL_PICKER_OTHER;
            case FileSelectedAction.EXTERNAL_PICKER_UNKNOWN_TYPE:
                return FileSelectedUploadMethod.EXTERNAL_PICKER_UNKNOWN_TYPE;
            default:
                assert false;
                return FileSelectedUploadMethod.MEDIA_PICKER_UNKNOWN_TYPE;
        }
    }

    private int getMediaType(
            Uri mediaUri, boolean mediaPickerWasUsed, ContentResolver contentResolver) {
        if (mediaUri == null) {
            return mediaPickerWasUsed
                    ? FileSelectedAction.MEDIA_PICKER_UNKNOWN_TYPE
                    : FileSelectedAction.EXTERNAL_PICKER_UNKNOWN_TYPE;
        }

        // Note: ContentResolver also allows MEDIA_TYPE to be queried instead, but that is less
        // reliable than the MIME type (frequently returns MEDIA_TYPE_IMAGE when selecting videos in
        // Android picker in the emulator).
        String[] filePathColumn = {
            MediaStore.Files.FileColumns.MIME_TYPE,
        };

        Cursor cursor = null;
        try {
            cursor = contentResolver.query(mediaUri, filePathColumn, null, null, null);
        } catch (Exception e) {
            // The OS may fail at some point during this, as seen in crbug.com/1395702.
            Log.w(TAG, "Failed to use ContentResolver", e);
            return mediaPickerWasUsed
                    ? FileSelectedAction.MEDIA_PICKER_UNKNOWN_TYPE
                    : FileSelectedAction.EXTERNAL_PICKER_UNKNOWN_TYPE;
        }

        if (cursor != null) {
            Integer mediaType = null;
            if (cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(filePathColumn[0]);
                if (column != -1) {
                    String mimeType = cursor.getString(column);
                    if (mimeType != null) {
                        if (mimeType.startsWith("image/")) {
                            mediaType =
                                    mediaPickerWasUsed
                                            ? FileSelectedAction.MEDIA_PICKER_IMAGE_BY_MIME_TYPE
                                            : FileSelectedAction.EXTERNAL_PICKER_IMAGE_BY_MIME_TYPE;
                        } else if (mimeType.startsWith("video/")) {
                            mediaType =
                                    mediaPickerWasUsed
                                            ? FileSelectedAction.MEDIA_PICKER_VIDEO_BY_MIME_TYPE
                                            : FileSelectedAction.EXTERNAL_PICKER_VIDEO_BY_MIME_TYPE;
                        } else {
                            mediaType =
                                    mediaPickerWasUsed
                                            ? FileSelectedAction.MEDIA_PICKER_OTHER_BY_MIME_TYPE
                                            : FileSelectedAction.EXTERNAL_PICKER_OTHER_BY_MIME_TYPE;
                        }
                    }
                }
            }
            cursor.close();
            if (mediaType != null) {
                return mediaType;
            }
        }

        // Unable to look up the MIME type, most likely because this URI is for media captured by
        // the camera. Use the extension if provided.
        int index = mediaUri.getPath().lastIndexOf(".");
        if (index > -1) {
            String extension = mediaUri.getPath().substring(index);
            for (String right : POPULAR_IMAGE_EXTENSIONS) {
                if (extension.equalsIgnoreCase(right)) {
                    return mediaPickerWasUsed
                            ? FileSelectedAction.MEDIA_PICKER_IMAGE_BY_EXTENSION
                            : FileSelectedAction.EXTERNAL_PICKER_IMAGE_BY_EXTENSION;
                }
            }
            for (String right : POPULAR_VIDEO_EXTENSIONS) {
                if (extension.equalsIgnoreCase(right)) {
                    return mediaPickerWasUsed
                            ? FileSelectedAction.MEDIA_PICKER_VIDEO_BY_EXTENSION
                            : FileSelectedAction.EXTERNAL_PICKER_VIDEO_BY_EXTENSION;
                }
            }

            return mediaPickerWasUsed
                    ? FileSelectedAction.MEDIA_PICKER_OTHER_BY_EXTENSION
                    : FileSelectedAction.EXTERNAL_PICKER_OTHER_BY_EXTENSION;
        }

        return mediaPickerWasUsed
                ? FileSelectedAction.MEDIA_PICKER_UNKNOWN_TYPE
                : FileSelectedAction.EXTERNAL_PICKER_UNKNOWN_TYPE;
    }

    private static void logFileSelectedAction(@FileSelectedAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.SelectFileDialogContentSelected", action, FileSelectedAction.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.SelectFileDialogUploadMethods",
                getUploadMethod(action),
                FileSelectedUploadMethod.COUNT);
    }

    private static boolean shouldShowPhotoPicker() {
        return sPhotoPickerDelegate != null;
    }

    private static boolean photoPickerSupportsVideo() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return false;
        return shouldShowPhotoPicker();
    }

    private static boolean preferAndroidMediaPickerViaGetContent() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                && sPhotoPickerDelegate != null
                && sPhotoPickerDelegate.launchViaActionGetContent();
    }

    private static boolean preferAndroidMediaPickerViaPickImage() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                && sPhotoPickerDelegate != null
                && sPhotoPickerDelegate.launchViaActionPickImages();
    }

    private static boolean preferAndroidMediaPickerViaPickImagePlus() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                && sPhotoPickerDelegate != null
                && sPhotoPickerDelegate.launchViaActionPickImagesPlus();
    }

    private static boolean preferAndroidMediaPicker() {
        return preferAndroidMediaPickerViaGetContent()
                || preferAndroidMediaPickerViaPickImage()
                || preferAndroidMediaPickerViaPickImagePlus();
    }

    /**
     * The Android media picker currently doesn't fully support multiple mime types. It can only do
     * a single specific mime type, all images, all videos, or all media (images/videos).
     */
    private static String singleMimeTypeForAndroidPicker(List<String> mimeTypes) {
        if (mimeTypes.size() == 1) {
            return mimeTypes.get(0);
        }

        boolean showImages = false;
        boolean showVideos = false;
        for (String mimeType : mimeTypes) {
            String type = mimeType.toLowerCase(Locale.ROOT);
            if (type.startsWith(IMAGE_TYPE)) {
                showImages = true;
            } else if (type.startsWith(VIDEO_TYPE)) {
                showVideos = true;
            }

            if (showImages && showVideos) break;
        }

        if (showImages && showVideos) {
            return ALL_TYPES;
        } else if (showVideos) {
            return VIDEO_TYPE + "/*";
        } else if (showImages) {
            return IMAGE_TYPE + "/*";
        } else {
            return "";
        }
    }

    private static void logMediaPickerShown(int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.MediaPickerShown", value, SHOWING_ENUM_COUNT);
    }

    private static String resolvePackageNameFromIntent(Intent intent) {
        String packageName = "";
        ResolveInfo resolveInfo = PackageManagerUtils.resolveActivity(intent, 0);
        if (resolveInfo != null
                && resolveInfo.activityInfo != null
                && resolveInfo.activityInfo.applicationInfo != null
                && resolveInfo.activityInfo.applicationInfo.packageName != null) {
            packageName = resolveInfo.activityInfo.applicationInfo.packageName;
        }
        return packageName;
    }

    private static boolean showPhotoPicker(
            WindowAndroid windowAndroid,
            WindowAndroid.IntentCallback intentCallback,
            PhotoPickerListener listener,
            boolean allowMultiple,
            List<String> mimeTypes) {
        if (preferAndroidMediaPickerViaGetContent()) {
            return showAndroidMediaPickerIndirect(
                    windowAndroid, intentCallback, allowMultiple, mimeTypes);
        } else if (preferAndroidMediaPickerViaPickImage()
                || preferAndroidMediaPickerViaPickImagePlus()) {
            return showAndroidMediaPickerDirect(
                    windowAndroid, intentCallback, allowMultiple, mimeTypes);
        } else {
            return showChromeMediaPicker(windowAndroid, listener, allowMultiple, mimeTypes);
        }
    }

    private static boolean showAndroidMediaPickerDirect(
            WindowAndroid windowAndroid,
            WindowAndroid.IntentCallback intentCallback,
            boolean allowMultiple,
            List<String> mimeTypes) {
        // This default value is kept for backwards compatibility, but it is effectively never used,
        // because the Android Media Picker is limited to T and newer (in preferAndroidMediaPicker).
        int maxImagesForUpload = 50;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            maxImagesForUpload = MediaStore.getPickImagesMaxLimit();
        }

        Intent intent = new Intent(MediaStore.ACTION_PICK_IMAGES);
        if (allowMultiple) {
            intent.putExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, maxImagesForUpload);
        }

        // This flag is currently a no-op for the Android Media Picker, but we are hoping it is
        // something the team will consider adding in the future (we have to add it now for
        // scheduling purposes).
        intent.putExtra("forceShowBrowse", true);

        // Note: The showAndroidMediaPickerDirect is not only used for the Direct and DirectPlus
        // flavors, but also as a fallback for Indirect. Only the Direct flavor should use the
        // deprecated MIME-type code-path.
        if (!preferAndroidMediaPickerViaPickImage()) {
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes.toArray(new String[0]));
        } else {
            String mimeType = singleMimeTypeForAndroidPicker(mimeTypes);
            if (mimeType.isEmpty()) {
                return false;
            }
            intent.setType(mimeType);
        }

        if (!windowAndroid.showIntent(
                intent, intentCallback, /* errorId= */ R.string.opening_android_media_picker)) {
            return false;
        }

        logMediaPickerShown(SHOWING_ANDROID_PICKER_DIRECT);
        return true;
    }

    private static boolean showAndroidMediaPickerIndirect(
            WindowAndroid windowAndroid,
            WindowAndroid.IntentCallback intentCallback,
            boolean allowMultiple,
            List<String> mimeTypes) {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        if (allowMultiple) {
            // Note that the ACTION_GET_CONTENT intent does not support a parameter to set a max
            // limit of photos (ACTION_PICK_IMAGES support is via MediaStore.EXTRA_PICK_IMAGES_MAX).
            // There is therefore no enforced max limit and all we need to do is set the 'allow
            // multiple' flag.
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes.toArray(new String[0]));

        // When relying on the indirect way of launching the Android Media Picker, we want to be
        // sure that the Android Media Picker is the one handling the request and not something
        // random.
        String packageNameForGetContent = resolvePackageNameFromIntent(intent);
        if (!"com.google.android.providers.media.module".equals(packageNameForGetContent)) {
            return showAndroidMediaPickerDirect(
                    windowAndroid, intentCallback, allowMultiple, mimeTypes);
        }

        if (!windowAndroid.showIntent(
                intent, intentCallback, /* errorId= */ R.string.opening_android_media_picker)) {
            return false;
        }

        logMediaPickerShown(SHOWING_ANDROID_PICKER_INDIRECT);
        return true;
    }

    private static boolean showChromeMediaPicker(
            WindowAndroid windowAndroid,
            PhotoPickerListener listener,
            boolean allowMultiple,
            List<String> mimeTypes) {
        if (sPhotoPickerDelegate == null) return false;
        assert sPhotoPicker == null;
        sPhotoPicker =
                sPhotoPickerDelegate.showPhotoPicker(
                        windowAndroid, listener, allowMultiple, mimeTypes);
        logMediaPickerShown(SHOWING_CHROME_PICKER);
        return true;
    }

    // Returns whether a string is a MIME type.
    private static boolean isMimeType(String type) {
        if (TextUtils.isEmpty(type) && type.length() < 3) {
            return false;
        }
        int index = type.indexOf('/');
        return index > 0 && index < type.length() - 1;
    }

    /**
     * Returns whether the current caller of the activity can access a content Uri. This method can
     * only be called inside onNewIntent() or onActivityResult() and may throw an exception if
     * called in other methods.
     *
     * @param uri Uri for permission check.
     * @return Whether the caller has permission to access uri.
     */
    @SuppressLint("NewApi")
    public boolean doesCallerHavePermissionForUri(Uri uri) {
        assert ThreadUtils.runningOnUiThread();

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.VANILLA_ICE_CREAM
                || !UiAndroidFeatureMap.isEnabled(
                        UiAndroidFeatures.CHECK_INTENT_CALLER_PERMISSION)) {
            return true;
        }
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) {
            return false;
        }
        try {
            return activity.getCurrentCaller()
                            .checkContentUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    == PackageManager.PERMISSION_GRANTED;
        } catch (Exception e) {
            Log.w(TAG, "Failed to check caller's permission.", e);
        }
        return false;
    }

    @CalledByNative
    private void nativeDestroyed() {
        mNativeSelectFileDialog = 0;
    }

    @VisibleForTesting
    @CalledByNative
    static SelectFileDialog create(long nativeSelectFileDialog) {
        return new SelectFileDialog(nativeSelectFileDialog);
    }

    @NativeMethods
    interface Natives {
        void onFileSelected(
                long nativeSelectFileDialogImpl,
                SelectFileDialog caller,
                String filePath,
                String displayName);

        void onMultipleFilesSelected(
                long nativeSelectFileDialogImpl,
                SelectFileDialog caller,
                String[] filePathArray,
                String[] displayNameArray);

        void onFileNotSelected(long nativeSelectFileDialogImpl, SelectFileDialog caller);
    }
}
