// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.Log;

/**
 * Helper service to provide a bridge between instrumentation tests and the
 * AccessibilityTestService.
 */
public class AccessibilityTestHelperService extends Service {
    private static final String TAG = "A11yTestHelperSvc";

    private final IAccessibilityTestHelperService.Stub mBinder =
            new IAccessibilityTestHelperService.Stub() {
                @Override
                public boolean waitForEvent(WaitForEventParams params) {
                    Log.i(
                            TAG,
                            "waitForEvent called with type: "
                                    + params.eventType
                                    + ", class: "
                                    + params.className
                                    + ", ContentChangeTypes: "
                                    + params.contentChangeTypes
                                    + ", text: "
                                    + params.text);
                    return AccessibilityTestService.tryWaitForEvent(params);
                }

                @Override
                public boolean performActionOnNode(String className, String text, int action) {
                    Log.i(TAG, "performActionOnNode called in HelperService");
                    return AccessibilityTestService.tryPerformActionOnNode(className, text, action);
                }

                @Override
                public String dumpWebContentsAccessibilityTree() {
                    Log.i(TAG, "dumpWebContentsAccessibilityTree called in HelperService");
                    return AccessibilityTestService.dumpWebContentsAccessibilityTree();
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
