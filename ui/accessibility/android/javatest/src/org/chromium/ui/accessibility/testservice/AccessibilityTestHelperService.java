// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.Log;

public class AccessibilityTestHelperService extends Service {
    private static final String TAG = "A11yTestHelperSvc";

    private final IAccessibilityTestHelperService.Stub mBinder =
            new IAccessibilityTestHelperService.Stub() {
                @Override
                public boolean waitForEvent(
                        int eventType, String className, String text, long timeoutMs) {
                    Log.i(
                            TAG,
                            "waitForEvent called with type: "
                                    + eventType
                                    + ", class: "
                                    + className
                                    + ", text: "
                                    + text);
                    return AccessibilityTestService.tryWaitForEvent(
                            eventType, className, text, timeoutMs);
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        Log.i(TAG, "onBind: " + intent);
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.i(TAG, "onUnbind: " + intent);
        return super.onUnbind(intent);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "onCreate");
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy");
    }
}
