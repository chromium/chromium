// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.Manifest;
import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.text.TextUtils;
import android.webkit.MimeTypeMap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.ui.PhotoPickerListener;
import org.chromium.ui.R;
import org.chromium.ui.UiUtils;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * A dialog that is triggered from a file input field that allows a user to select a file based on
 * a set of accepted file types. The path of the selected file is passed to the native dialog.
 */
@JNINamespace("ui")
public class SelectFileDialog implements WindowAndroid.IntentCallback, PhotoPickerListener {
    private static final String TAG = "SelectFileDialog";
    private static final String IMAGE_TYPE = "image/";
    private static final String VIDEO_TYPE = "video/";
    private static final String AUDIO_TYPE = "audio/";
    private static final String ALL_IMAGE_TYPES = IMAGE_TYPE + "*";
    private static final String ALL_VIDEO_TYPES = VIDEO_TYPE + "*";
    private static final String ALL_AUDIO_TYPES = AUDIO_TYPE + "*";
    private static final String ANY_TYPES = "*/*";

    // Duration before temporary camera file is cleaned up, in milliseconds.
    private static final long DURATION_BEFORE_FILE_CLEAN_UP_IN_MILLIS = TimeUnit.HOURS.toMillis(1);

    // A list of some of the more popular image extensions. Not meant to be
    // exhaustive, but should cover the vast majority of image types.
    private static final String[] POPULAR_IMAGE_EXTENSIONS = new String[] {".apng", ".bmp", ".gif",
            ".jpeg", ".jpg", ".pdf", ".png", ".tif", ".tiff", ".xcf", ".webp"};

    // A list of some of the more popular video extensions. Not meant to be
    // exhaustive, but should cover the vast majority of video types.
    private static final String[] POPULAR_VIDEO_EXTENSIONS = new String[] {".asf", ".avhcd", ".avi",
            ".divx", ".flv", ".mov", ".mp4", ".mpeg", ".mpg", ".swf", ".wmv", ".webm", ".mkv"};

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
     * If set, overrides the WindowAndroid passed in {@link selectFile()}.
     */
    @SuppressLint("StaticFieldLeak")
    private static WindowAndroid sOverrideWindowAndroid;

    private final long mNativeSelectFileDialog;
    private List<String> mFileTypes;
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

    SelectFileDialog(long nativeSelectFileDialog) {
        mNativeSelectFileDialog = nativeSelectFileDialog;
    }

    /**
     * Overrides the WindowAndroid passed in {@link selectFile()}.
     */
    @VisibleForTesting
    public static void setWindowAndroidForTests(WindowAndroid window) {
        sOverrideWindowAndroid = window;
    }

    /**
     * Overrides the list of accepted file types for testing purposes.
     */
    @VisibleForTesting
    public void setFileTypesForTests(List<String> fileTypes) {
        mFileTypes = fileTypes;
    }

    /**
     * Creates and starts an intent based on the passed fileTypes and capture value.
     * @param fileTypes MIME types requested (i.e. "image/*")
     * @param capture The capture value as described in http://www.w3.org/TR/html-media-capture/
     * @param multiple Whether it should be possible to select multiple files.
     * @param window The WindowAndroid that can show intents
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    @CalledByNative
    private void selectFile(
            String[] fileTypes, boolean capture, boolean multiple, WindowAndroid window) {
        mFileTypes = new ArrayList<String>(Arrays.asList(fileTypes));
        mCapture = capture;
        mAllowMultiple = multiple;
        mWindowAndroid = (sOverrideWindowAndroid == null) ? window : sOverrideWindowAndroid;

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
            if (!window.hasPermission(storagePermission)) missingPermissions.add(storagePermission);
        } else {
            if (((mSupportsImageCapture && shouldShowImageTypes())
                        || (mSupportsVideoCapture && shouldShowVideoTypes()))
                    && !window.hasPermission(Manifest.permission.CAMERA)) {
                missingPermissions.add(Manifest.permission.CAMERA);
            }
            if (mSupportsAudioCapture && shouldShowAudioTypes()
                    && !window.hasPermission(Manifest.permission.RECORD_AUDIO)) {
                missingPermissions.add(Manifest.permission.RECORD_AUDIO);
            }
        }

        if (missingPermissions.isEmpty()) {
            launchSelectFileIntent();
        } else {
            String[] requestPermissions =
                    missingPermissions.toArray(new String[missingPermissions.size()]);
            window.requestPermissions(requestPermissions, (permissions, grantResults) -> {
                for (int i = 0; i < grantResults.length; i++) {
                    if (grantResults[i] == PackageManager.PERMISSION_DENIED) {
                        if (mCapture) {
                            onFileNotSelected();
                            return;
                        }

                        // TODO(finnur): Remove once we figure out the cause of crbug.com/950024.
                        if (shouldUsePhotoPicker) {
                            if (permissions.length != requestPermissions.length) {
                                throw new RuntimeException(
                                        String.format("Permissions arrays misaligned: %d != %d",
                                                permissions.length, requestPermissions.length));
                            }

                            if (!permissions[i].equals(requestPermissions[i])) {
                                throw new RuntimeException(
                                        String.format("Permissions arrays don't match: %s != %s",
                                                permissions[i], requestPermissions[i]));
                            }
                        }

                        if (shouldUsePhotoPicker && permissions[i].equals(storagePermission)) {
                            onFileNotSelected();
                            return;
                        }
                    }
                }
                launchSelectFileIntent();
            });
        }
    }

    /**
     * Called to launch an intent to allow user to select files.
     */
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

    /**
     * Called to launch an intent to allow user to select files. If |camera| is null,
     * the select file dialog shouldn't include any files from the camera. Otherwise, user
     * is allowed to choose files from the camera.
     * @param camera Intent for selecting files from camera.
     */
    private void launchSelectFileWithCameraIntent(Intent camera) {
        RecordHistogram.recordEnumeratedHistogram("Android.SelectFileDialogScope",
                determineSelectFileDialogScope(), SELECT_FILE_DIALOG_SCOPE_COUNT);

        boolean hasCameraPermission = mWindowAndroid.hasPermission(Manifest.permission.CAMERA);
        Intent camcorder = null;
        if (mSupportsVideoCapture && hasCameraPermission) {
            camcorder = new Intent(MediaStore.ACTION_VIDEO_CAPTURE);
        }

        boolean hasAudioPermission =
                mWindowAndroid.hasPermission(Manifest.permission.RECORD_AUDIO);
        Intent soundRecorder = null;
        if (mSupportsAudioCapture && hasAudioPermission) {
            soundRecorder = new Intent(MediaStore.Audio.Media.RECORD_SOUND_ACTION);
        }

        // Quick check - if the |capture| parameter is set and |fileTypes| has the appropriate MIME
        // type, we should just launch the appropriate intent. Otherwise build up a chooser based
        // on the accept type and then display that to the user.
        if (captureImage() && camera != null) {
            if (mWindowAndroid.showIntent(camera, this, R.string.low_memory_error)) return;
        } else if (captureVideo() && camcorder != null) {
            if (mWindowAndroid.showIntent(camcorder, this, R.string.low_memory_error)) return;
        } else if (captureAudio() && soundRecorder != null) {
            if (mWindowAndroid.showIntent(soundRecorder, this, R.string.low_memory_error)) return;
        }

        Activity activity = mWindowAndroid.getActivity().get();

        // Use the new photo picker, if available.
        List<String> imageMimeTypes = convertToSupportedPhotoPickerTypes(mFileTypes);
        if (shouldUsePhotoPicker()
                && UiUtils.showPhotoPicker(activity, this, mAllowMultiple, imageMimeTypes)) {
            return;
        }

        Intent getContentIntent = new Intent(Intent.ACTION_GET_CONTENT);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2 && mAllowMultiple) {
            getContentIntent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }

        ArrayList<Intent> extraIntents = new ArrayList<Intent>();
        if (!noSpecificType()) {
            // Create a chooser based on the accept type that was specified in the webpage. Note
            // that if the web page specified multiple accept types, we will have built a generic
            // chooser above.
            if (shouldShowImageTypes()) {
                if (camera != null) extraIntents.add(camera);
                getContentIntent.setType(ALL_IMAGE_TYPES);
            } else if (shouldShowVideoTypes()) {
                if (camcorder != null) extraIntents.add(camcorder);
                getContentIntent.setType(ALL_VIDEO_TYPES);
            } else if (shouldShowAudioTypes()) {
                if (soundRecorder != null) extraIntents.add(soundRecorder);
                getContentIntent.setType(ALL_AUDIO_TYPES);
            }

            // If any types are specified, then only accept openable files, as coercing
            // virtual files may yield to a MIME type different than expected.
            getContentIntent.addCategory(Intent.CATEGORY_OPENABLE);
        }

        if (extraIntents.isEmpty()) {
            // We couldn't resolve an accept type, so fallback to a generic chooser.
            getContentIntent.setType(ANY_TYPES);
            if (camera != null) extraIntents.add(camera);
            if (camcorder != null) extraIntents.add(camcorder);
            if (soundRecorder != null) extraIntents.add(soundRecorder);
        }

        Intent chooser = new Intent(Intent.ACTION_CHOOSER);
        if (!extraIntents.isEmpty()) {
            chooser.putExtra(Intent.EXTRA_INITIAL_INTENTS,
                    extraIntents.toArray(new Intent[] { }));
        }
        chooser.putExtra(Intent.EXTRA_INTENT, getContentIntent);

        if (!mWindowAndroid.showIntent(chooser, this, R.string.low_memory_error)) {
            onFileNotSelected();
        }
    }

    /**
     * Determines whether the photo picker should be used for this select file request.  To be
     * applicable for the photo picker, the following must be true:
     *   1.) Only image types were requested in the file request
     *   2.) The file request did not explicitly ask to capture camera directly.
     *   3.) The photo picker is supported by the embedder (i.e. Chrome).
     *   4.) There is a valid Android Activity associated with the file request.
     */
    private boolean shouldUsePhotoPicker() {
        List<String> mediaMimeTypes = convertToSupportedPhotoPickerTypes(mFileTypes);
        return !captureImage() && mediaMimeTypes != null && UiUtils.shouldShowPhotoPicker()
                && mWindowAndroid.getActivity().get() != null;
    }

    /**
     * Converts a list of extensions and Mime types to a list of de-duped Mime types supported by
     * the photo picker only. If the input list contains a unsupported type, then null is returned.
     * @param fileTypes the list of filetypes (extensions and Mime types) to convert.
     * @return A de-duped list of supported types only, or null if one or more unsupported types
     *         were given as input.
     */
    @VisibleForTesting
    public static List<String> convertToSupportedPhotoPickerTypes(List<String> fileTypes) {
        if (fileTypes.size() == 0) return null;
        List<String> mimeTypes = new ArrayList<>();
        for (String type : fileTypes) {
            String mimeType = ensureMimeType(type);
            if (!mimeType.startsWith("image/")) {
                if (!UiUtils.photoPickerSupportsVideo() || !mimeType.startsWith("video/")) {
                    return null;
                }
            }
            if (!mimeTypes.contains(mimeType)) mimeTypes.add(mimeType);
        }
        return mimeTypes;
    }

    /**
     * Convert |type| to MIME type (known types only).
     * @param type The type to convert. Can be either a MIME type or an extension (should include
     *             the leading dot). If an extension is passed in, it is converted to the
     *             corresponding MIME type (via {@link MimeTypeMap}), or "application/octet-stream"
     *             if the MIME type is not known.
     * @return The MIME type, if known, or "application/octet-stream" otherwise (or blank if input
     *         is blank).
     */
    @VisibleForTesting
    public static String ensureMimeType(String type) {
        if (type.length() == 0) return "";

        String extension = MimeTypeMap.getFileExtensionFromUrl(type);
        if (extension.length() > 0) {
            String mimeType = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
            if (mimeType != null) return mimeType;
            return "application/octet-stream";
        }
        return type;
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

                GetDisplayNameTask task = new GetDisplayNameTask(
                        ContextUtils.getApplicationContext(), photos.length > 1, photos);
                task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
                break;

            case PhotoPickerAction.LAUNCH_GALLERY:
                Intent intent = new Intent();
                intent.setType("image/*");
                if (mAllowMultiple) intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                intent.setAction(Intent.ACTION_GET_CONTENT);
                mWindowAndroid.showCancelableIntent(intent, this, R.string.low_memory_error);
                break;

            case PhotoPickerAction.LAUNCH_CAMERA:
                if (!mWindowAndroid.hasPermission(Manifest.permission.CAMERA)) {
                    mWindowAndroid.requestPermissions(new String[] {Manifest.permission.CAMERA},
                            (permissions, grantResults) -> {
                                assert grantResults.length == 1;
                                if (grantResults[0] == PackageManager.PERMISSION_DENIED) {
                                    onFileNotSelected();
                                    return;
                                }
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

    private class GetCameraIntentTask extends AsyncTask<Uri> {
        private Boolean mDirectToCamera;
        private WindowAndroid mWindow;
        private WindowAndroid.IntentCallback mCallback;

        public GetCameraIntentTask(Boolean directToCamera, WindowAndroid window,
                WindowAndroid.IntentCallback callback) {
            mDirectToCamera = directToCamera;
            mWindow = window;
            mCallback = callback;
        }

        @Override
        public Uri doInBackground() {
            try {
                Context context = ContextUtils.getApplicationContext();
                return ContentUriUtils.getContentUriFromFile(getFileForImageCapture(context));
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

            Intent camera = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
            camera.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            camera.putExtra(MediaStore.EXTRA_OUTPUT, mCameraOutputUri);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                camera.setClipData(
                        ClipData.newUri(ContextUtils.getApplicationContext().getContentResolver(),
                                UiUtils.IMAGE_FILE_PATH, mCameraOutputUri));
            }
            if (mDirectToCamera) {
                mWindow.showIntent(camera, mCallback, R.string.low_memory_error);
            } else {
                launchSelectFileWithCameraIntent(camera);
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
        File photoFile = File.createTempFile(String.valueOf(System.currentTimeMillis()), ".jpg",
                UiUtils.getDirectoryForImageCapture(context));
        return photoFile;
    }

    /**
     * Callback method to handle the intent results and pass on the path to the native
     * SelectFileDialog.
     * @param window The window that has access to the application activity.
     * @param resultCode The result code whether the intent returned successfully.
     * @param results The results of the requested intent.
     */
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    @Override
    public void onIntentCompleted(WindowAndroid window, int resultCode, Intent results) {
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
            String path = ContentResolver.SCHEME_FILE.equals(mCameraOutputUri.getScheme())
                    ? mCameraOutputUri.getPath() : mCameraOutputUri.toString();
            onFileSelected(mNativeSelectFileDialog, path, mCameraOutputUri.getLastPathSegment());
            // Broadcast to the media scanner that there's a new photo on the device so it will
            // show up right away in the gallery (rather than waiting until the next time the media
            // scanner runs).
            window.sendBroadcast(new Intent(
                    Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, mCameraOutputUri));
            return;
        }

        // Path for when EXTRA_ALLOW_MULTIPLE Intent extra has been defined. Each of the selected
        // files will be shared as an entry on the Intent's ClipData. This functionality is only
        // available in Android JellyBean MR2 and higher.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2
                && results.getData() == null && results.getClipData() != null) {
            ClipData clipData = results.getClipData();

            int itemCount = clipData.getItemCount();
            if (itemCount == 0) {
                onFileNotSelected();
                return;
            }

            Uri[] filePathArray = new Uri[itemCount];
            for (int i = 0; i < itemCount; ++i) {
                filePathArray[i] = clipData.getItemAt(i).getUri();
            }
            GetDisplayNameTask task = new GetDisplayNameTask(
                    ContextUtils.getApplicationContext(), true, filePathArray);
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            return;
        }

        if (ContentResolver.SCHEME_FILE.equals(results.getData().getScheme())) {
            String filePath = results.getData().getPath();
            // Don't allow files under private data dir to be uploaded.
            if (!TextUtils.isEmpty(filePath)
                    && !filePath.startsWith(PathUtils.getDataDirectory())) {
                onFileSelected(mNativeSelectFileDialog, filePath, "");
                return;
            }
        }

        if (ContentResolver.SCHEME_CONTENT.equals(results.getScheme())) {
            GetDisplayNameTask task = new GetDisplayNameTask(
                    ContextUtils.getApplicationContext(), false, new Uri[] {results.getData()});
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            return;
        }

        onFileNotSelected();
        window.showError(R.string.opening_file_error);
    }

    private void onFileNotSelected() {
        onFileNotSelected(mNativeSelectFileDialog);
    }

    // Determines the scope of the requested select file dialog for use in a UMA histogram. Right
    // now we want to distinguish between generic, photo and visual media pickers.
    @VisibleForTesting
    int determineSelectFileDialogScope() {
        if (mFileTypes.size() == 0) return SELECT_FILE_DIALOG_SCOPE_GENERIC;

        // Capture the MIME types:
        int acceptsImages = countAcceptTypesFor(IMAGE_TYPE);
        int acceptsVideos = countAcceptTypesFor(VIDEO_TYPE);

        // Capture the most common image and video extensions:
        if (mFileTypes.size() > acceptsImages + acceptsVideos) {
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
            return (acceptsImages == 0) ? SELECT_FILE_DIALOG_SCOPE_VIDEOS
                                        : SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS;
        }
        return SELECT_FILE_DIALOG_SCOPE_IMAGES;
    }

    private boolean noSpecificType() {
        // We use a single Intent to decide the type of the file chooser we display to the user,
        // which means we can only give it a single type. If there are multiple accept types
        // specified, we will fallback to a generic chooser (unless a capture parameter has been
        // specified, in which case we'll try to satisfy that first.
        return mFileTypes.size() != 1 || mFileTypes.contains(ANY_TYPES);
    }

    private boolean shouldShowTypes(String allTypes, String specificType) {
        if (noSpecificType() || mFileTypes.contains(allTypes)) return true;
        return countAcceptTypesFor(specificType) > 0;
    }

    private boolean shouldShowImageTypes() {
        return shouldShowTypes(ALL_IMAGE_TYPES, IMAGE_TYPE);
    }

    private boolean shouldShowVideoTypes() {
        return shouldShowTypes(ALL_VIDEO_TYPES, VIDEO_TYPE);
    }

    private boolean shouldShowAudioTypes() {
        return shouldShowTypes(ALL_AUDIO_TYPES, AUDIO_TYPE);
    }

    private boolean acceptsSpecificType(String type) {
        return mFileTypes.size() == 1 && TextUtils.equals(mFileTypes.get(0), type);
    }

    /**
     * Whether the HTML input field specified the 'capture' attribute and specifically requested
     * image capture.
     *
     * See https://www.w3.org/TR/html-media-capture/ for further description.
     */
    private boolean captureImage() {
        return mCapture && acceptsSpecificType(ALL_IMAGE_TYPES);
    }

    /**
     * Whether the HTML input field specified the 'capture' attribute and specifically requested
     * video capture.
     */
    private boolean captureVideo() {
        return mCapture && acceptsSpecificType(ALL_VIDEO_TYPES);
    }

    /**
     * Whether the HTML input field specified the 'capture' attribute and specifically requested
     * audio capture.
     */
    private boolean captureAudio() {
        return mCapture && acceptsSpecificType(ALL_AUDIO_TYPES);
    }

    private int countAcceptTypesFor(String accept) {
        int count = 0;
        for (String type : mFileTypes) {
            if (type.startsWith(accept)) {
                count++;
            }
        }
        return count;
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
        public String[] doInBackground() {
            mFilePaths = new String[mUris.length];
            String[] displayNames = new String[mUris.length];
            try {
                for (int i = 0; i < mUris.length; i++) {
                    // The selected files must be returned as a list of absolute paths. A MIUI 8.5
                    // device was observed to return a file:// URI instead, so convert if necessary.
                    // See https://crbug.com/752834 for context.
                    if (ContentResolver.SCHEME_FILE.equals(mUris[i].getScheme())) {
                        mFilePaths[i] = mUris[i].getSchemeSpecificPart();
                    } else {
                        mFilePaths[i] = mUris[i].toString();
                    }
                    displayNames[i] = ContentUriUtils.getDisplayName(
                            mUris[i], mContext, MediaStore.MediaColumns.DISPLAY_NAME);
                }
            }  catch (SecurityException e) {
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

    /**
     * Clears all captured camera files.
     */
    public static void clearCapturedCameraFiles() {
        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
            try {
                File path =
                        UiUtils.getDirectoryForImageCapture(ContextUtils.getApplicationContext());
                if (!path.isDirectory()) return;
                File[] files = path.listFiles();
                if (files == null) return;
                long now = System.currentTimeMillis();
                for (File file : files) {
                    if (now - file.lastModified() > DURATION_BEFORE_FILE_CLEAN_UP_IN_MILLIS) {
                        if (!file.delete()) Log.e(TAG, "Failed to delete: " + file);
                    }
                }
            } catch (IOException e) {
                Log.w(TAG, "Failed to delete captured camera files.", e);
            }
        });
    }

    private boolean eligibleForPhotoPicker() {
        return convertToSupportedPhotoPickerTypes(mFileTypes) != null;
    }

    private void onFileSelected(
            long nativeSelectFileDialogImpl, String filePath, String displayName) {
        if (eligibleForPhotoPicker()) recordImageCountHistogram(1);
        SelectFileDialogJni.get().onFileSelected(
                nativeSelectFileDialogImpl, SelectFileDialog.this, filePath, displayName);
    }

    private void onMultipleFilesSelected(
            long nativeSelectFileDialogImpl, String[] filePathArray, String[] displayNameArray) {
        if (eligibleForPhotoPicker()) recordImageCountHistogram(filePathArray.length);
        SelectFileDialogJni.get().onMultipleFilesSelected(
                nativeSelectFileDialogImpl, SelectFileDialog.this, filePathArray, displayNameArray);
    }

    private void onFileNotSelected(long nativeSelectFileDialogImpl) {
        if (eligibleForPhotoPicker()) recordImageCountHistogram(0);
        SelectFileDialogJni.get().onFileNotSelected(
                nativeSelectFileDialogImpl, SelectFileDialog.this);
    }

    private void recordImageCountHistogram(int count) {
        RecordHistogram.recordCount100Histogram("Android.SelectFileDialogImgCount", count);
    }

    @VisibleForTesting
    @CalledByNative
    static SelectFileDialog create(long nativeSelectFileDialog) {
        return new SelectFileDialog(nativeSelectFileDialog);
    }

    @NativeMethods
    interface Natives {
        void onFileSelected(long nativeSelectFileDialogImpl, SelectFileDialog caller,
                String filePath, String displayName);
        void onMultipleFilesSelected(long nativeSelectFileDialogImpl, SelectFileDialog caller,
                String[] filePathArray, String[] displayNameArray);
        void onFileNotSelected(long nativeSelectFileDialogImpl, SelectFileDialog caller);
        void onContactsSelected(
                long nativeSelectFileDialogImpl, SelectFileDialog caller, String contacts);
    }
}
