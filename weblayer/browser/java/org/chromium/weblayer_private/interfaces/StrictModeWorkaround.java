// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.os.Parcel;
import android.os.StrictMode;
import android.util.Log;

import java.lang.reflect.Field;

/**
 * Workaround to unset PENALTY_GATHER in StrictMode.
 * Normally the serialization code to set the PENALTY_GATHER bit in each AIDL
 * call and the IPC code unsets the bit. WebLayer only uses AIDL for
 * serialization, but not IPC; this leaves the PENALTY_GATHER bit always set,
 * causing any StrictMode violations to be gathered and serialized in ever AIDL
 * call but never reported.
 * StrictModeWorkaround.apply will unset the PENALTY_GATHER bit and should be
 * called at the beginning of the implementation of an AIDL call.
 */
public final class StrictModeWorkaround {
    private static final String TAG = "StrictModeWorkaround";

    private static final int PENALTY_GATHER;
    private static final Field sThreadPolicyMaskField;
    static {
        Field field;
        int mask;
        try {
            field = StrictMode.ThreadPolicy.class.getDeclaredField("mask");
            field.setAccessible(true);

            int currentMask = getCurrentPolicyMask(field);
            try {
                setCurrentPolicyMask(field, 0);

                // The value of PENALTY_GATHER has changed between Android
                // versions. Instead of hard coding them, try to get the value
                // from Parcel directly.
                Parcel parcel = Parcel.obtain();
                // The value of the token does not matter.
                parcel.writeInterfaceToken(TAG);
                parcel.setDataPosition(0);
                mask = parcel.readInt();
                parcel.recycle();
            } finally {
                setCurrentPolicyMask(field, currentMask);
            }
        } catch (NoSuchFieldException | SecurityException e) {
            Log.w(TAG, "StrictMode reflection exception", e);
            field = null;
            mask = 0;
        } catch (RuntimeException e) {
            Log.w(TAG, "StrictMode run time exception", e);
            field = null;
            mask = 0;
        }
        sThreadPolicyMaskField = field;
        PENALTY_GATHER = mask;
    }

    private static int getCurrentPolicyMask(Field field) {
        StrictMode.ThreadPolicy policy = StrictMode.getThreadPolicy();
        try {
            return field.getInt(policy);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }
    }

    private static void setCurrentPolicyMask(Field field, int newPolicyMask) {
        StrictMode.ThreadPolicy policy = StrictMode.getThreadPolicy();
        try {
            field.setInt(policy, newPolicyMask);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }
        StrictMode.setThreadPolicy(policy);
    }

    public static void apply() {
        if (sThreadPolicyMaskField == null) return;
        try {
            int currentPolicyMask = getCurrentPolicyMask(sThreadPolicyMaskField);
            if ((currentPolicyMask & PENALTY_GATHER) == 0) {
                return;
            }
            setCurrentPolicyMask(sThreadPolicyMaskField, currentPolicyMask & ~PENALTY_GATHER);
        } catch (RuntimeException e) {
            // Ignore exceptions.
        }
    }
}
