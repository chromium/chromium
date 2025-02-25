// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.app.Activity;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.xr.scenecore.PanelEntity;
import androidx.xr.scenecore.Session;
import androidx.xr.scenecore.impl.JxrPlatformAdapterAxr;

import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.Executors;

/** A singleton utility class to manages XR session and UI environment. */
@NullMarked
public class XrUtils {
    private static final String TAG = "XrUtils";

    private static XrUtils sInstance = new XrUtils();
    private static @Nullable Boolean sXrDeviceOverrideForTesting;

    // For spatialization of Chrome app using Jetpack XR.
    private @Nullable Session mXrSession;
    private boolean mModeSwitchInProgress;
    private boolean mInFullSpaceMode;
    private boolean mCompletedSwitchToFSM;

    /** Returns the singleton instance of XRUtils. */
    public static XrUtils getInstance() {
        return sInstance;
    }

    /** Set the XR device envornment for testing. */
    public static void setXrDeviceForTesting(Boolean isXrDevice) {
        sXrDeviceOverrideForTesting = isXrDevice;
    }

    /** Reset the XR device envornment to the default value. */
    public static void resetXrDeviceForTesting() {
        sXrDeviceOverrideForTesting = null;
    }

    /** Return if the app is running on an immersive XR device. */
    public static boolean isXrDevice() {
        boolean xrDevice =
                (sXrDeviceOverrideForTesting != null)
                        ? sXrDeviceOverrideForTesting
                        : (PackageManagerUtils.hasSystemFeature(
                                        PackageManagerUtils.XR_IMMERSIVE_FEATURE_NAME)
                                || PackageManagerUtils.hasSystemFeature(
                                        PackageManagerUtils.XR_OPENXR_FEATURE_NAME));
        return xrDevice;
    }

    /**
     * Initialize the class and store info that will be used during spatialization and maintaining
     * viewing state.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void init(@NonNull Activity activity) {
        // Note: The activity/window handle is only used in the function during initialization of XR
        // and not saved in the class to prevent activity leaks.
        if (!isXrDevice()) return;

        // Initialization of XR for spatialization will occur here using JXR.
        mXrSession = createJxrSession(activity);
        mXrSession
                .getActivitySpace()
                .addBoundsChangedListener(
                        dimensions -> {
                            if (mXrSession == null) return;

                            PanelEntity mainPanelEntity = mXrSession.getMainPanelEntity();
                            if (mModeSwitchInProgress) {
                                mModeSwitchInProgress = false;
                                if (dimensions.getWidth() == Float.POSITIVE_INFINITY
                                        && mainPanelEntity != null) {
                                    Log.i(TAG, "SPA completed switch to FSM,");
                                    mCompletedSwitchToFSM = true;
                                }
                            }
                        });
    }

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private JxrPlatformAdapterAxr createJxrPlatformAdapter(@NonNull Activity activity) {
        // TODO(crbug.com/397984536) Upstream ClankListeningScheduledExecutorService.
        return JxrPlatformAdapterAxr.create(
                activity,
                Executors.newSingleThreadScheduledExecutor(),
                /* useSplitEngine= */ false);
    }

    @VisibleForTesting
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    protected Session createJxrSession(@NonNull Activity activity) {
        return Session.create(activity, createJxrPlatformAdapter(activity));
    }

    /**
     * Initialize viewing of the XR environment in the full space mode in which only the single
     * activity is visible to the user and all other activities are hidden out.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void viewInFullSpaceMode() {
        if (mXrSession == null) return;

        // Requesting of full space mode using JXR.
        mModeSwitchInProgress = true;
        Log.i(TAG, "SPA requesting FullSpaceMode");
        mInFullSpaceMode = true;
        // Tracks the asynchronous requests to switch to full space mode and will be used to decide
        // when to resize the main panel on completion.
        mCompletedSwitchToFSM = false;
        mXrSession.getSpatialEnvironment().requestFullSpaceMode();
    }

    /**
     * Initiate viewing of the XR environment in the default home space mode in which all open
     * activity is visible to the user similar to desktop environment.
     */
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void viewInHomeSpaceMode() {
        if (mXrSession == null) return;

        // Requesting return to home space mode using JXR.
        mModeSwitchInProgress = true;
        Log.i(TAG, "SPA requesting HomeSpaceMode");
        mXrSession.getSpatialEnvironment().requestHomeSpaceMode();
        mInFullSpaceMode = false;
        mCompletedSwitchToFSM = false;
    }

    /**
     * Check if the device is in full space mode or in home space mode. Only one of these mode is
     * active at any given time.
     */
    public boolean isFsmOnXrDevice() {
        return isXrDevice() && mInFullSpaceMode;
    }

    boolean isXrInitializedForTesting() {
        return mXrSession != null;
    }

    public static void setXrUtilsForTesting(XrUtils instance) {
        XrUtils oldInstance = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }
}
