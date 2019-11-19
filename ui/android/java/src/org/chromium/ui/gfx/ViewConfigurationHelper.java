// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gfx;

import android.content.ComponentCallbacks;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.TypedValue;
import android.view.ViewConfiguration;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.R;

/**
 * This class facilitates access to ViewConfiguration-related properties, also
 * providing native-code notifications when such properties have changed.
 *
 */
@JNINamespace("gfx")
public class ViewConfigurationHelper {

    // Fallback constants when resource lookup fails, see
    // ui/android/java/res/values/dimens.xml.
    private static final float MIN_SCALING_SPAN_MM = 12.0f;

    private ViewConfiguration mViewConfiguration;
    private float mDensity;

    private ViewConfigurationHelper() {
        mViewConfiguration = ViewConfiguration.get(ContextUtils.getApplicationContext());
        mDensity = ContextUtils.getApplicationContext().getResources().getDisplayMetrics().density;
        assert mDensity > 0;
    }

    private void registerListener() {
        ContextUtils.getApplicationContext().registerComponentCallbacks(
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration configuration) {
                        updateNativeViewConfigurationIfNecessary();
                    }

                    @Override
                    public void onLowMemory() {
                    }
                });
    }

    private void updateNativeViewConfigurationIfNecessary() {
        ViewConfiguration configuration =
                ViewConfiguration.get(ContextUtils.getApplicationContext());
        if (mViewConfiguration == configuration) {
            // The density should remain the same as long as the ViewConfiguration remains the same.
            assert mDensity
                    == ContextUtils.getApplicationContext()
                               .getResources()
                               .getDisplayMetrics()
                               .density;
            return;
        }

        mViewConfiguration = configuration;
        mDensity = ContextUtils.getApplicationContext().getResources().getDisplayMetrics().density;
        assert mDensity > 0;
        ViewConfigurationHelperJni.get().updateSharedViewConfiguration(ViewConfigurationHelper.this,
                getMaximumFlingVelocity(), getMinimumFlingVelocity(), getTouchSlop(),
                getDoubleTapSlop(), getMinScalingSpan());
    }

    @CalledByNative
    private static int getDoubleTapTimeout() {
        return ViewConfiguration.getDoubleTapTimeout();
    }

    @CalledByNative
    private static int getLongPressTimeout() {
        return ViewConfiguration.getLongPressTimeout();
    }

    @CalledByNative
    private static int getTapTimeout() {
        return ViewConfiguration.getTapTimeout();
    }

    @CalledByNative
    private float getMaximumFlingVelocity() {
        return toDips(mViewConfiguration.getScaledMaximumFlingVelocity());
    }

    @CalledByNative
    private float getMinimumFlingVelocity() {
        return toDips(mViewConfiguration.getScaledMinimumFlingVelocity());
    }

    @CalledByNative
    private float getTouchSlop() {
        return toDips(mViewConfiguration.getScaledTouchSlop());
    }

    @CalledByNative
    private float getDoubleTapSlop() {
        return toDips(mViewConfiguration.getScaledDoubleTapSlop());
    }

    @CalledByNative
    private float getMinScalingSpan() {
        return toDips(getScaledMinScalingSpan());
    }

    private int getScaledMinScalingSpan() {
        final Resources res = ContextUtils.getApplicationContext().getResources();
        // The correct minimum scaling span depends on how we recognize scale
        // gestures. Since we've deviated from Android, don't use the Android
        // system value here.
        int id = R.dimen.config_min_scaling_span;
        try {
            return res.getDimensionPixelSize(id);
        } catch (Resources.NotFoundException e) {
            assert false : "MinScalingSpan resource lookup failed.";
            return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_MM, MIN_SCALING_SPAN_MM,
                    res.getDisplayMetrics());
        }
    }

    /**
     * @return the unscaled pixel quantity in DIPs.
     */
    private float toDips(int pixels) {
        return pixels / mDensity;
    }

    @CalledByNative
    private static ViewConfigurationHelper createWithListener() {
        ViewConfigurationHelper viewConfigurationHelper = new ViewConfigurationHelper();
        viewConfigurationHelper.registerListener();
        return viewConfigurationHelper;
    }

    @NativeMethods
    interface Natives {
        void updateSharedViewConfiguration(ViewConfigurationHelper caller,
                float maximumFlingVelocity, float minimumFlingVelocity, float touchSlop,
                float doubleTapSlop, float minScalingSpan);
    }
}
