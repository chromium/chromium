/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.cardboard.sdk;

import android.Manifest;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.vision.MultiProcessor;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;
import com.google.cardboard.sdk.qrcode.CardboardParamsUtils;
import com.google.cardboard.sdk.qrcode.QrCodeContentProcessor;
import com.google.cardboard.sdk.qrcode.QrCodeTracker;
import com.google.cardboard.sdk.qrcode.QrCodeTrackerFactory;
import com.google.cardboard.sdk.qrcode.camera.CameraSource;
import com.google.cardboard.sdk.qrcode.camera.CameraSourcePreview;

import java.io.IOException;

/**
 * Manages the QR code capture activity. It scans permanently with the camera until it finds a valid
 * QR code.
 */
public class QrCodeCaptureActivity extends AppCompatActivity
        implements QrCodeTracker.Listener, QrCodeContentProcessor.Listener {
    private static final String TAG = QrCodeCaptureActivity.class.getSimpleName();

    // Intent request code to handle updating play services if needed.
    private static final int RC_HANDLE_GMS = 9001;

    // Permission request codes
    private static final int PERMISSIONS_REQUEST_CODE = 2;

    // Min sdk version required for google play services.
    private static final int MIN_SDK_VERSION = 23;

    private CameraSource cameraSource;
    private CameraSourcePreview cameraSourcePreview;

    // Flag used to avoid saving the device parameters more than once.
    private static boolean qrCodeSaved = false;

    /** Initializes the UI and creates the detector pipeline. */
    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.qr_code_capture);

        // Adds margins to the container to account for edge to edge:
        // https://developer.android.com/develop/ui/views/layout/edge-to-edge
        View container = findViewById(R.id.container);
        ViewCompat.setOnApplyWindowInsetsListener(
                container,
                (v, windowInsets) -> {
                    Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
                    ViewGroup.MarginLayoutParams mlp =
                            (ViewGroup.MarginLayoutParams) v.getLayoutParams();
                    mlp.leftMargin = insets.left;
                    mlp.bottomMargin = insets.bottom;
                    mlp.rightMargin = insets.right;
                    v.setLayoutParams(mlp);
                    return WindowInsetsCompat.CONSUMED;
                });

        cameraSourcePreview = findViewById(R.id.preview);
    }

    /**
     * Checks for CAMERA permission.
     *
     * @return whether CAMERA permission is already granted.
     */
    private boolean isCameraEnabled() {
        return ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Checks for WRITE_EXTERNAL_STORAGE permission.
     *
     * @return whether WRITE_EXTERNAL_STORAGE permission is already granted.
     */
    private boolean isWriteExternalStoragePermissionsEnabled() {
        return ActivityCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
    }

    /** Handles the requests for activity permissions. */
    private void requestPermissions() {
        final String[] permissions =
                VERSION.SDK_INT < VERSION_CODES.Q
                        ? new String[] {
                            Manifest.permission.CAMERA, Manifest.permission.WRITE_EXTERNAL_STORAGE
                        }
                        : new String[] {Manifest.permission.CAMERA};
        ActivityCompat.requestPermissions(this, permissions, PERMISSIONS_REQUEST_CODE);
    }

    /**
     * Callback for the result from requesting permissions.
     *
     * <p>When Android SDK version is less than Q, both WRITE_EXTERNAL_STORAGE and CAMERA
     * permissions are requested. Otherwise, only CAMERA permission is requested.
     */
    @Override
    public void onRequestPermissionsResult(
            int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (VERSION.SDK_INT < VERSION_CODES.Q) {
            if (!(isCameraEnabled() && isWriteExternalStoragePermissionsEnabled())) {
                Log.i(TAG, getString(R.string.no_permissions));
                Toast.makeText(this, R.string.no_permissions, Toast.LENGTH_LONG).show();
                if (!ActivityCompat.shouldShowRequestPermissionRationale(
                                this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                        || !ActivityCompat.shouldShowRequestPermissionRationale(
                                this, Manifest.permission.CAMERA)) {
                    // Permission denied with checking "Do not ask again".
                    Log.i(TAG, "Permission denied with checking \"Do not ask again\".");
                    launchPermissionsSettings();
                }
                finish();
            }
        } else {
            if (!isCameraEnabled()) {
                Log.i(TAG, getString(R.string.no_camera_permission));
                Toast.makeText(this, R.string.no_camera_permission, Toast.LENGTH_LONG).show();
                if (!ActivityCompat.shouldShowRequestPermissionRationale(
                        this, Manifest.permission.CAMERA)) {
                    // Permission denied with checking "Do not ask again". Note that in Android R
                    // "Do not ask
                    // again" is not available anymore.
                    Log.i(TAG, "Permission denied with checking \"Do not ask again\".");
                    launchPermissionsSettings();
                }
                finish();
            }
        }
    }

    private void launchPermissionsSettings() {
        Intent intent = new Intent();
        intent.setAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(Uri.fromParts("package", getPackageName(), null));
        startActivity(intent);
    }

    /** Creates and starts the camera. */
    private void createCameraSource() {
        Context context = getApplicationContext();

        BarcodeDetector qrCodeDetector =
                new BarcodeDetector.Builder(context).setBarcodeFormats(Barcode.QR_CODE).build();

        QrCodeTrackerFactory qrCodeFactory = new QrCodeTrackerFactory(this);

        qrCodeDetector.setProcessor(new MultiProcessor.Builder<>(qrCodeFactory).build());

        // Check that native dependencies are downloaded.
        if (!qrCodeDetector.isOperational()) {
            Toast.makeText(this, R.string.missing_dependencies, Toast.LENGTH_LONG).show();
            Log.w(
                    TAG,
                    "QR Code detector is not operational. Try connecting to WiFi and updating"
                        + " Google Play Services or checking that the device storage isn't low.");
        }

        // Creates and starts the camera.
        cameraSource = new CameraSource(getApplicationContext(), qrCodeDetector);
    }

    /** Restarts the camera. */
    @Override
    protected void onResume() {
        super.onResume();
        // Checks for CAMERA permission and WRITE_EXTERNAL_STORAGE permission when running on
        // Android P
        // or below. If needed permissions are not granted, requests them.
        if (!(isCameraEnabled()
                && (VERSION.SDK_INT >= VERSION_CODES.Q
                        || isWriteExternalStoragePermissionsEnabled()))) {
            requestPermissions();
            return;
        }

        createCameraSource();
        qrCodeSaved = false;
        startCameraSource();
    }

    /** Stops the camera. */
    @Override
    protected void onPause() {
        super.onPause();
        if (cameraSourcePreview != null) {
            cameraSourcePreview.stop();
            cameraSourcePreview.release();
        }
    }

    /** Starts or restarts the camera source, if it exists. */
    private void startCameraSource() {
        // Check that the device has play services available.
        int code =
                GoogleApiAvailability.getInstance()
                        .isGooglePlayServicesAvailable(getApplicationContext(), MIN_SDK_VERSION);
        if (code != ConnectionResult.SUCCESS) {
            Log.i(TAG, "isGooglePlayServicesAvailable() returned: " + new ConnectionResult(code));
            Dialog dlg =
                    GoogleApiAvailability.getInstance().getErrorDialog(this, code, RC_HANDLE_GMS);
            dlg.show();
        }

        if (cameraSource != null) {
            try {
                cameraSourcePreview.start(cameraSource);
            } catch (IOException e) {
                Log.e(TAG, "Unable to start camera source.", e);
                cameraSource.release();
                cameraSource = null;
            } catch (SecurityException e) {
                Log.e(TAG, "Security exception: ", e);
            }
            Log.i(TAG, "cameraSourcePreview successfully started.");
        }
    }

    /** Callback for when "SKIP" is touched */
    public void skipQrCodeCapture(View view) {
        Log.d(TAG, "QR code capture skipped");

        // Check if there are already saved parameters, if not save Cardboard V1 ones.
        final Context context = getApplicationContext();
        byte[] deviceParams = CardboardParamsUtils.readDeviceParams(context);
        if (deviceParams == null) {
            CardboardParamsUtils.saveCardboardV1DeviceParams(context);
        }
        finish();
    }

    /**
     * Callback for when a QR code is detected.
     *
     * @param qrCode Detected QR code.
     */
    @Override
    public void onQrCodeDetected(Barcode qrCode) {
        if (qrCode != null && !qrCodeSaved) {
            qrCodeSaved = true;
            QrCodeContentProcessor qrCodeContentProcessor = new QrCodeContentProcessor(this);
            qrCodeContentProcessor.processAndSaveQrCode(qrCode, this);
        }
    }

    /**
     * Callback for when a QR code is processed and the parameters are saved in external storage.
     *
     * @param status Whether the parameters were successfully processed and saved.
     */
    @Override
    public void onQrCodeSaved(boolean status) {
        if (status) {
            Log.d(TAG, "Device parameters saved in external storage.");
            cameraSourcePreview.stop();
            nativeIncrementDeviceParamsChangedCount();
            finish();
        } else {
            Log.e(TAG, "Device parameters not saved in external storage.");
        }
        qrCodeSaved = false;
    }

    private native void nativeIncrementDeviceParamsChangedCount();
}
