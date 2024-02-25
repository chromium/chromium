// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.KeyguardManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.PowerManager;
import android.os.SystemClock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

import java.util.concurrent.TimeUnit;

/**
 * Simple proxy that provides C++ code with an access pathway to the Android
 * idle detection.
 */
@JNINamespace("ui")
public class IdleDetector extends BroadcastReceiver {
    private static final String TAG = "IdleDetector";
    // Memory handled by idle_android:cc: a singleton (```detector```) gets
    // constructed every time this class gets loaded (which happens the first
    // time IdleDetector#getIdleTime gets called). Upon construction, the
    // broadcast receivers are registered.
    private boolean mIdle;
    private long mLast;

    private IdleDetector() {
        if (isScreenLocked()) {
            start();
        } else {
            reset();
        }

        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_USER_PRESENT);
        ContextUtils.registerProtectedBroadcastReceiver(
                ContextUtils.getApplicationContext(), this, filter);
    }

    @CalledByNative
    private static IdleDetector create() {
        return new IdleDetector();
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction().equals(Intent.ACTION_SCREEN_OFF)) {
            start();
        } else if (intent.getAction().equals(Intent.ACTION_USER_PRESENT)) {
            reset();
        }
    }

    private long now() {
        return SystemClock.elapsedRealtime();
    }

    private void start() {
        mIdle = true;
        mLast = now();
    }

    private void reset() {
        mIdle = false;
        mLast = 0;
    }

    @CalledByNative
    private long getIdleTime() {
        if (!mIdle) return 0;
        return TimeUnit.MILLISECONDS.toSeconds(now() - mLast);
    }

    @CalledByNative
    private boolean isScreenLocked() {
        Context context = ContextUtils.getApplicationContext();
        KeyguardManager keyguardManager =
                (KeyguardManager) context.getSystemService(Context.KEYGUARD_SERVICE);
        if (keyguardManager.inKeyguardRestrictedInputMode()) return true;

        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        if (powerManager == null) return false;
        return !powerManager.isInteractive();
    }
}
