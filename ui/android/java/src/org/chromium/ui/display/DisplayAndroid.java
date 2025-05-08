// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.display;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.view.Display;
import android.view.Surface;

import androidx.annotation.RequiresApi;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;
import java.util.WeakHashMap;

/**
 * Chromium's object for android.view.Display. Instances of this object should be obtained
 * from WindowAndroid.
 * This class is designed to avoid leaks. It is ok to hold a strong ref of this class from
 * anywhere, as long as the corresponding WindowAndroids are destroyed. The observers are
 * held weakly so to not lead to leaks.
 */
@NullMarked
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
        default void onDisplayModesChanged(@Nullable List<Display.Mode> supportedModes) {}

        /**
         * Called whenever the attached display's current mode is changed.
         *
         * @param currentMode the current display mode.
         */
        default void onCurrentModeChanged(Display.@Nullable Mode currentMode) {}

        default void onAdaptiveRefreshRateInfoChanged(AdaptiveRefreshRateInfo arrInfo) {}
    }

    public static final class AdaptiveRefreshRateInfo {
        public final boolean supportsAdaptiveRefreshRate;
        public final float suggestedFrameRateHigh;

        public AdaptiveRefreshRateInfo(
                boolean supportsAdaptiveRefreshRate, float suggestedFrameRateHigh) {
            this.supportsAdaptiveRefreshRate = supportsAdaptiveRefreshRate;
            this.suggestedFrameRateHigh = suggestedFrameRateHigh;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof AdaptiveRefreshRateInfo)) {
                return false;
            }
            AdaptiveRefreshRateInfo other = (AdaptiveRefreshRateInfo) obj;
            return supportsAdaptiveRefreshRate == other.supportsAdaptiveRefreshRate
                    && suggestedFrameRateHigh == other.suggestedFrameRateHigh;
        }
    }

    private static final DisplayAndroidObserver[] EMPTY_OBSERVER_ARRAY =
            new DisplayAndroidObserver[0];

    private final WeakHashMap<DisplayAndroidObserver, Object /* null */> mObservers;
    // Do NOT add strong references to objects with potentially complex lifetime, like Context.

    private final int mDisplayId;
    private @Nullable String mName;
    private Rect mBounds;
    private @Nullable Insets mInsets;
    private float mDipScale;
    private float mXdpi;
    private float mYdpi;
    private int mBitsPerPixel;
    private int mBitsPerComponent;
    private int mRotation;
    private float mRefreshRate;
    private Display.@Nullable Mode mCurrentDisplayMode;
    private @Nullable List<Display.Mode> mDisplayModes;
    private boolean mIsHdr;
    private float mHdrMaxLuminanceRatio = 1.0f;
    private boolean mIsInternal;
    protected boolean mIsDisplayWideColorGamut;
    protected boolean mIsDisplayServerWideColorGamut;
    private AdaptiveRefreshRateInfo mAdaptiveRefreshRateInfo =
            new AdaptiveRefreshRateInfo(false, 0.0f);

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

    /** Returns the display ID that matches the one defined in Android's Display. */
    public int getDisplayId() {
        return mDisplayId;
    }

    /** Returns the name of the display. */
    public @Nullable String getDisplayName() {
        return mName;
    }

    /** Returns display height in physical pixels. */
    public int getDisplayHeight() {
        return mBounds.height();
    }

    /** Returns display width in physical pixels. */
    public int getDisplayWidth() {
        return mBounds.width();
    }

    /** Returns the bounds of the display. */
    public Rect getBounds() {
        return new Rect(mBounds);
    }

    /** Returns the bounds as an array. */
    public int[] getBoundsAsArray() {
        return new int[] {mBounds.left, mBounds.top, mBounds.right, mBounds.bottom};
    }

    /** Returns the insets of the display. */
    @RequiresApi(Build.VERSION_CODES.R)
    public Insets getInsets() {
        return assumeNonNull(mInsets);
    }

    /** Returns the insets as an array. */
    @RequiresApi(Build.VERSION_CODES.R)
    public int[] getInsetsAsArray() {
        Insets insets = assumeNonNull(mInsets);
        return new int[] {insets.left, insets.top, insets.right, insets.bottom};
    }

    /** Returns current orientation. One of Surface.ORIENTATION_* values. */
    public int getRotation() {
        return mRotation;
    }

    /** Returns current orientation in degrees. One of the values 0, 90, 180, 270. */
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

    /** Returns the scaling factor for the Density Independent Pixel unit. */
    public float getDipScale() {
        return mDipScale;
    }

    /** Returns the exact physical pixels per inch of the screen in the X dimension. */
    public float getXdpi() {
        return mXdpi;
    }

    /** Returns the exact physical pixels per inch of the screen in the Y dimension. */
    public float getYdpi() {
        return mYdpi;
    }

    /** Returns the number of bits per pixel. */
    /* package */ int getBitsPerPixel() {
        return mBitsPerPixel;
    }

    /** Returns the number of bits per each color component. */
    @SuppressWarnings("deprecation")
    /* package */ int getBitsPerComponent() {
        return mBitsPerComponent;
    }

    /**
     * Returns whether or not it is possible to use wide color gamut rendering with this display.
     */
    public boolean getIsWideColorGamut() {
        return mIsDisplayWideColorGamut && mIsDisplayServerWideColorGamut;
    }

    /** Returns display's refresh rate in frames per second. */
    public float getRefreshRate() {
        return mRefreshRate;
    }

    /** Returns Display.Modes supported by this Display. */
    public @Nullable List<Display.Mode> getSupportedModes() {
        return mDisplayModes;
    }

    /** Returns current Display.Mode for the display. */
    public Display.@Nullable Mode getCurrentMode() {
        return mCurrentDisplayMode;
    }

    public AdaptiveRefreshRateInfo getAdaptiveRefreshRateInfo() {
        return mAdaptiveRefreshRateInfo;
    }

    /**
     * Whether or not the display is HDR capable. If false then getHdrMaxLuminanceRatio will always
     * return 1.0. Package private only because no client needs to access this from java.
     */
    /* package */ boolean getIsHdr() {
        return mIsHdr;
    }

    /**
     * Max luminance HDR content can display, represented as a multiple of the SDR white luminance
     * (so a display that is incapable of HDR would have a value of 1.0).
     * Package private only because no client needs to access this from java.
     */
    /* package */ float getHdrMaxLuminanceRatio() {
        return mHdrMaxLuminanceRatio;
    }

    /**
     * Returns whether or not the display is internal. Internal screens are part of the device.
     * Package private only because no client needs to access this from java.
     */
    /* package */ boolean isInternal() {
        return mIsInternal;
    }

    /**
     * Return window context for display android. Implemented by @{@link PhysicalDisplayAndroid}.
     */
    public @Nullable Context getWindowContext() {
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

    /**
     * Constructs an instance.
     *
     * @param displayId The display ID of Android's Display represented by this object.
     */
    protected DisplayAndroid(int displayId) {
        mDisplayId = displayId;
        mObservers = new WeakHashMap<>();
        mBounds = new Rect();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            mInsets = Insets.of(0, 0, 0, 0);
        }
    }

    private DisplayAndroidObserver[] getObservers() {
        // Makes a copy to allow concurrent edit.
        return mObservers.keySet().toArray(EMPTY_OBSERVER_ARRAY);
    }

    public void updateIsDisplayServerWideColorGamut(
            @Nullable Boolean isDisplayServerWideColorGamut) {
        update(
                /* name= */ null,
                /* bounds= */ null,
                /* insets= */ null,
                /* dipScale= */ null,
                /* xdpi= */ null,
                /* ydpi= */ null,
                /* bitsPerPixel= */ null,
                /* bitsPerComponent= */ null,
                /* rotation= */ null,
                /* isDisplayWideColorGamut= */ null,
                isDisplayServerWideColorGamut,
                /* refreshRate= */ null,
                /* currentMode= */ null,
                /* supportedModes= */ null,
                /* isHdr= */ null,
                /* hdrMaxLuminanceRatio= */ null,
                /* isInternal= */ null,
                /* arrInfo= */ null);
    }

    /** Update the display to the provided parameters. Null values leave the parameter unchanged. */
    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/98
    protected void update(
            @Nullable String name,
            @Nullable Rect bounds,
            @Nullable Insets insets,
            @Nullable Float dipScale,
            @Nullable Float xdpi,
            @Nullable Float ydpi,
            @Nullable Integer bitsPerPixel,
            @Nullable Integer bitsPerComponent,
            @Nullable Integer rotation,
            @Nullable Boolean isDisplayWideColorGamut,
            @Nullable Boolean isDisplayServerWideColorGamut,
            @Nullable Float refreshRate,
            Display.@Nullable Mode currentMode,
            @Nullable List<Display.Mode> supportedModes,
            @Nullable Boolean isHdr,
            @Nullable Float hdrMaxLuminanceRatio,
            @Nullable Boolean isInternal,
            @Nullable AdaptiveRefreshRateInfo arrInfo) {
        boolean nameChanged = name != null && !name.equals(mName);
        boolean boundsChanged = bounds != null && !bounds.equals(mBounds);
        boolean insetsChanged = insets != null && !insets.equals(mInsets);
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
                supportedModes != null && !supportedModes.equals(mDisplayModes);
        boolean currentModeChanged =
                currentMode != null && !currentMode.equals(mCurrentDisplayMode);
        boolean isHdrChanged = isHdr != null && isHdr != mIsHdr;
        boolean hdrMaxLuminanceRatioChanged =
                hdrMaxLuminanceRatio != null && hdrMaxLuminanceRatio != mHdrMaxLuminanceRatio;
        boolean isInternalChanged = isInternal != null && mIsInternal != isInternal;
        boolean adaptiveRefreshRateInfoChanged =
                arrInfo != null
                        && (mAdaptiveRefreshRateInfo == null
                                || !mAdaptiveRefreshRateInfo.equals(arrInfo));
        boolean changed =
                nameChanged
                        || boundsChanged
                        || insetsChanged
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
                        || hdrMaxLuminanceRatioChanged
                        || isInternalChanged
                        || adaptiveRefreshRateInfoChanged;
        if (!changed) return;

        if (nameChanged) mName = name;
        if (boundsChanged) mBounds = bounds;
        if (insetsChanged) mInsets = insets;
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
        if (hdrMaxLuminanceRatioChanged) {
            mHdrMaxLuminanceRatio = hdrMaxLuminanceRatio;
        }
        if (isRefreshRateChanged) mRefreshRate = refreshRate;
        if (displayModesChanged) mDisplayModes = supportedModes;
        if (currentModeChanged) mCurrentDisplayMode = currentMode;
        if (isInternalChanged) mIsInternal = isInternal;
        if (adaptiveRefreshRateInfoChanged) {
            mAdaptiveRefreshRateInfo = arrInfo;
        }

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
        if (adaptiveRefreshRateInfoChanged) {
            DisplayAndroidObserver[] observers = getObservers();
            for (DisplayAndroidObserver o : observers) {
                o.onAdaptiveRefreshRateInfoChanged(mAdaptiveRefreshRateInfo);
            }
        }
    }
}
