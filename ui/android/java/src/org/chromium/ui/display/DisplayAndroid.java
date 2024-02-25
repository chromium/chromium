// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Point;
import android.view.Display;
import android.view.Surface;

import java.util.List;
import java.util.WeakHashMap;

/**
 * Chromium's object for android.view.Display. Instances of this object should be obtained
 * from WindowAndroid.
 * This class is designed to avoid leaks. It is ok to hold a strong ref of this class from
 * anywhere, as long as the corresponding WindowAndroids are destroyed. The observers are
 * held weakly so to not lead to leaks.
 */
public class DisplayAndroid {
    /** DisplayAndroidObserver interface for changes to this Display. */
    public interface DisplayAndroidObserver {
        /**
         * Called whenever the screen orientation changes.
         *
         * @param rotation One of Surface.ROTATION_* values.
         */
        default void onRotationChanged(int rotation) {}

        /**
         * Called whenever the screen density changes.
         *
         * @param dipScale Density Independent Pixel scale.
         */
        default void onDIPScaleChanged(float dipScale) {}

        /**
         * Called whenever the attached display's refresh rate changes.
         *
         * @param refreshRate the display's refresh rate in frames per second.
         */
        default void onRefreshRateChanged(float refreshRate) {}

        /**
         * Called whenever the attached display's supported Display.Modes are changed.
         *
         * @param supportedModes the array of supported modes.
         */
        default void onDisplayModesChanged(List<Display.Mode> supportedModes) {}

        /**
         * Called whenever the attached display's current mode is changed.
         *
         * @param currentMode the current display mode.
         */
        default void onCurrentModeChanged(Display.Mode currentMode) {}
    }

    private static final DisplayAndroidObserver[] EMPTY_OBSERVER_ARRAY =
            new DisplayAndroidObserver[0];

    private final WeakHashMap<DisplayAndroidObserver, Object /* null */> mObservers;
    // Do NOT add strong references to objects with potentially complex lifetime, like Context.

    private final int mDisplayId;
    private Point mSize;
    private float mDipScale;
    private float mXdpi;
    private float mYdpi;
    private int mBitsPerPixel;
    private int mBitsPerComponent;
    private int mRotation;
    private float mRefreshRate;
    private Display.Mode mCurrentDisplayMode;
    private List<Display.Mode> mDisplayModes;
    private boolean mIsHdr;
    private float mHdrMaxLuminanceRatio = 1.0f;
    protected boolean mIsDisplayWideColorGamut;
    protected boolean mIsDisplayServerWideColorGamut;

    protected static DisplayAndroidManager getManager() {
        return DisplayAndroidManager.getInstance();
    }

    /**
     * Get the non-multi-display DisplayAndroid for the given context. It's safe to call this with
     * any type of context, including the Application.
     *
     * To support multi-display, obtain DisplayAndroid from WindowAndroid instead.
     *
     * This function is intended to be analogous to GetPrimaryDisplay() for other platforms.
     * However, Android has historically had no real concept of a Primary Display, and instead uses
     * the notion of a default display for an Activity. Under normal circumstances, this function,
     * called with the correct context, will return the expected display for an Activity. However,
     * virtual, or "fake", displays that are not associated with any context may be used in special
     * cases, like Virtual Reality, and will lead to this function returning the incorrect display.
     *
     * @return What the Android WindowManager considers to be the default display for this context.
     */
    public static DisplayAndroid getNonMultiDisplay(Context context) {
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        return getManager().getDisplayAndroid(display);
    }

    /**
     * @return Display id that does not necessarily match the one defined in Android's Display.
     */
    public int getDisplayId() {
        return mDisplayId;
    }

    /**
     * Note: For JB pre-MR1, this can sometimes return values smaller than the actual screen.
     * https://crbug.com/829318
     * @return Display height in physical pixels.
     */
    public int getDisplayHeight() {
        return mSize.y;
    }

    /**
     * Note: For JB pre-MR1, this can sometimes return values smaller than the actual screen.
     * @return Display width in physical pixels.
     */
    public int getDisplayWidth() {
        return mSize.x;
    }

    /**
     * @return current orientation. One of Surface.ORIENTATION_* values.
     */
    public int getRotation() {
        return mRotation;
    }

    /**
     * @return current orientation in degrees. One of the values 0, 90, 180, 270.
     */
    public int getRotationDegrees() {
        switch (getRotation()) {
            case Surface.ROTATION_0:
                return 0;
            case Surface.ROTATION_90:
                return 90;
            case Surface.ROTATION_180:
                return 180;
            case Surface.ROTATION_270:
                return 270;
        }

        // This should not happen.
        assert false;
        return 0;
    }

    /**
     * @return A scaling factor for the Density Independent Pixel unit.
     */
    public float getDipScale() {
        return mDipScale;
    }

    /**
     * @return The exact physical pixels per inch of the screen in the X dimension.
     */
    public float getXdpi() {
        return mXdpi;
    }

    /**
     * @return The exact physical pixels per inch of the screen in the Y dimension.
     */
    public float getYdpi() {
        return mYdpi;
    }

    /**
     * @return Number of bits per pixel.
     */
    /* package */ int getBitsPerPixel() {
        return mBitsPerPixel;
    }

    /**
     * @return Number of bits per each color component.
     */
    @SuppressWarnings("deprecation")
    /* package */ int getBitsPerComponent() {
        return mBitsPerComponent;
    }

    /**
     * @return Whether or not it is possible to use wide color gamut rendering with this display.
     */
    public boolean getIsWideColorGamut() {
        return mIsDisplayWideColorGamut && mIsDisplayServerWideColorGamut;
    }

    /**
     * @return Display's refresh rate in frames per second.
     */
    public float getRefreshRate() {
        return mRefreshRate;
    }

    /*
     * @return Display.Modes supported by this Display.
     */
    public List<Display.Mode> getSupportedModes() {
        return mDisplayModes;
    }

    /*
     * @return Current Display.Mode for the display.
     */
    public Display.Mode getCurrentMode() {
        return mCurrentDisplayMode;
    }

    /**
     * Whether or not the display is HDR capable. If false then getHdrMaxLuminanceRatio will
     * always return 1.0.
     */
    // Package private only because no client needs to access this from java.
    boolean getIsHdr() {
        return mIsHdr;
    }

    /**
     * Max luminance HDR content can display, represented as a multiple of the SDR white luminance
     * (so a display that is incapable of HDR would have a value of 1.0).
     */
    // Package private only because no client needs to access this from java.
    float getHdrMaxLuminanceRatio() {
        return mHdrMaxLuminanceRatio;
    }

    /**
     * Return window context for display android. Implemented by @{@link PhysicalDisplayAndroid}
     * @return window context.
     */
    public Context getWindowContext() {
        return null;
    }

    /**
     * Add observer. Note repeat observers will be called only one.
     * Observers are held only weakly by Display.
     */
    public void addObserver(DisplayAndroidObserver observer) {
        mObservers.put(observer, null);
    }

    /** Remove observer. */
    public void removeObserver(DisplayAndroidObserver observer) {
        mObservers.remove(observer);
    }

    protected DisplayAndroid(int displayId) {
        mDisplayId = displayId;
        mObservers = new WeakHashMap<>();
        mSize = new Point();
    }

    private DisplayAndroidObserver[] getObservers() {
        // Makes a copy to allow concurrent edit.
        return mObservers.keySet().toArray(EMPTY_OBSERVER_ARRAY);
    }

    public void updateIsDisplayServerWideColorGamut(Boolean isDisplayServerWideColorGamut) {
        update(
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                isDisplayServerWideColorGamut,
                null,
                null,
                null,
                /* isHdr= */ null,
                /* hdrMaxLuminanceRatio= */ null);
    }

    /** Update the display to the provided parameters. Null values leave the parameter unchanged. */
    @SuppressLint("NewApi")
    protected void update(
            Point size,
            Float dipScale,
            Integer bitsPerPixel,
            Integer bitsPerComponent,
            Integer rotation,
            Boolean isDisplayWideColorGamut,
            Boolean isDisplayServerWideColorGamut,
            Float refreshRate,
            Display.Mode currentMode,
            List<Display.Mode> supportedModes) {
        update(
                size,
                dipScale,
                null,
                null,
                bitsPerPixel,
                bitsPerComponent,
                rotation,
                isDisplayWideColorGamut,
                isDisplayServerWideColorGamut,
                refreshRate,
                currentMode,
                supportedModes,
                /* isHdr= */ null,
                /* hdrMaxLuminanceRatio= */ null);
    }

    /** Update the display to the provided parameters. Null values leave the parameter unchanged. */
    @SuppressLint("NewApi")
    protected void update(
            Point size,
            Float dipScale,
            Float xdpi,
            Float ydpi,
            Integer bitsPerPixel,
            Integer bitsPerComponent,
            Integer rotation,
            Boolean isDisplayWideColorGamut,
            Boolean isDisplayServerWideColorGamut,
            Float refreshRate,
            Display.Mode currentMode,
            List<Display.Mode> supportedModes,
            Boolean isHdr,
            Float hdrMaxLuminanceRatio) {
        boolean sizeChanged = size != null && !mSize.equals(size);
        // Intentional comparison of floats: we assume that if scales differ, they differ
        // significantly.
        boolean dipScaleChanged = dipScale != null && mDipScale != dipScale;
        boolean xdpiChanged = xdpi != null && mXdpi != xdpi;
        boolean ydpiChanged = ydpi != null && mYdpi != ydpi;
        boolean bitsPerPixelChanged = bitsPerPixel != null && mBitsPerPixel != bitsPerPixel;
        boolean bitsPerComponentChanged =
                bitsPerComponent != null && mBitsPerComponent != bitsPerComponent;
        boolean rotationChanged = rotation != null && mRotation != rotation;
        boolean isDisplayWideColorGamutChanged =
                isDisplayWideColorGamut != null
                        && mIsDisplayWideColorGamut != isDisplayWideColorGamut;
        boolean isDisplayServerWideColorGamutChanged =
                isDisplayServerWideColorGamut != null
                        && mIsDisplayServerWideColorGamut != isDisplayServerWideColorGamut;
        boolean isRefreshRateChanged = refreshRate != null && mRefreshRate != refreshRate;
        boolean displayModesChanged =
                supportedModes != null
                        && (mDisplayModes == null ? true : mDisplayModes.equals(supportedModes));
        boolean currentModeChanged =
                currentMode != null && !currentMode.equals(mCurrentDisplayMode);
        boolean isHdrChanged = isHdr != null && isHdr != mIsHdr;
        boolean hdrMaxLuninanceRatioChanged =
                hdrMaxLuminanceRatio != null && hdrMaxLuminanceRatio != mHdrMaxLuminanceRatio;
        boolean changed =
                sizeChanged
                        || dipScaleChanged
                        || bitsPerPixelChanged
                        || bitsPerComponentChanged
                        || rotationChanged
                        || isDisplayWideColorGamutChanged
                        || isDisplayServerWideColorGamutChanged
                        || isRefreshRateChanged
                        || displayModesChanged
                        || currentModeChanged
                        || isHdrChanged
                        || hdrMaxLuninanceRatioChanged;
        if (!changed) return;

        if (sizeChanged) mSize = size;
        if (dipScaleChanged) mDipScale = dipScale;
        if (xdpiChanged) mXdpi = xdpi;
        if (ydpiChanged) mYdpi = ydpi;
        if (bitsPerPixelChanged) mBitsPerPixel = bitsPerPixel;
        if (bitsPerComponentChanged) mBitsPerComponent = bitsPerComponent;
        if (rotationChanged) mRotation = rotation;
        if (isDisplayWideColorGamutChanged) mIsDisplayWideColorGamut = isDisplayWideColorGamut;
        if (isDisplayServerWideColorGamutChanged) {
            mIsDisplayServerWideColorGamut = isDisplayServerWideColorGamut;
        }
        if (isHdrChanged) mIsHdr = isHdr;
        if (hdrMaxLuninanceRatioChanged) {
            mHdrMaxLuminanceRatio = hdrMaxLuminanceRatio;
        }
        if (isRefreshRateChanged) mRefreshRate = refreshRate;
        if (displayModesChanged) mDisplayModes = supportedModes;
        if (currentModeChanged) mCurrentDisplayMode = currentMode;

        getManager().updateDisplayOnNativeSide(this);
        if (rotationChanged) {
            DisplayAndroidObserver[] observers = getObservers();
            for (DisplayAndroidObserver o : observers) {
                o.onRotationChanged(mRotation);
            }
        }
        if (dipScaleChanged) {
            DisplayAndroidObserver[] observers = getObservers();
            for (DisplayAndroidObserver o : observers) {
                o.onDIPScaleChanged(mDipScale);
            }
        }
        if (isRefreshRateChanged) {
            DisplayAndroidObserver[] observers = getObservers();
            for (DisplayAndroidObserver o : observers) {
                o.onRefreshRateChanged(mRefreshRate);
            }
        }
        if (displayModesChanged) {
            DisplayAndroidObserver[] observers = getObservers();
            for (DisplayAndroidObserver o : observers) {
                o.onDisplayModesChanged(mDisplayModes);
            }
        }
        if (currentModeChanged) {
            DisplayAndroidObserver[] observers = getObservers();
            for (DisplayAndroidObserver o : observers) {
                o.onCurrentModeChanged(mCurrentDisplayMode);
            }
        }
    }
}
