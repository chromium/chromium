// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;
import android.content.pm.FeatureInfo;
import android.os.Build;
import android.view.MotionEvent;

import org.chromium.base.ContextUtils;

/** Support S-Pen event detection and conversion. */
public final class SPenSupport {
    // These values are obtained from Samsung.
    private static final int SPEN_ACTION_DOWN = 211;
    private static final int SPEN_ACTION_UP = 212;
    private static final int SPEN_ACTION_MOVE = 213;
    private static final int SPEN_ACTION_CANCEL = 214;
    private static Boolean sIsSPenSupported;

    /**
     * Initialize SPen support. This is done lazily at the first invocation of
     * {@link #convertSPenEventAction(int)}.
     */
    private static void initialize() {
        if (sIsSPenSupported != null) return;

        if (!"SAMSUNG".equalsIgnoreCase(Build.MANUFACTURER)) {
            sIsSPenSupported = false;
            return;
        }

        Context context = ContextUtils.getApplicationContext();
        final FeatureInfo[] infos = context.getPackageManager().getSystemAvailableFeatures();
        for (FeatureInfo info : infos) {
            if ("com.sec.feature.spen_usp".equalsIgnoreCase(info.name)) {
                sIsSPenSupported = true;
                return;
            }
        }
        sIsSPenSupported = false;
    }

    /**
     * Convert SPen event action into normal event action.
     *
     * @param eventActionMasked Input event action. It is assumed that it is masked as the values
     *     cannot be ORed.
     * @return Event action after the conversion.
     */
    public static int convertSPenEventAction(int eventActionMasked) {
        if (sIsSPenSupported == null) initialize();
        if (!sIsSPenSupported.booleanValue()) return eventActionMasked;

        // S-Pen support: convert to normal stylus event handling
        switch (eventActionMasked) {
            case SPEN_ACTION_DOWN:
                return MotionEvent.ACTION_DOWN;
            case SPEN_ACTION_UP:
                return MotionEvent.ACTION_UP;
            case SPEN_ACTION_MOVE:
                return MotionEvent.ACTION_MOVE;
            case SPEN_ACTION_CANCEL:
                return MotionEvent.ACTION_CANCEL;
            default:
                return eventActionMasked;
        }
    }
}
