// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.view.accessibility.AccessibilityEvent;

import org.chromium.base.Log;

import java.util.function.IntFunction;

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
                    String contentChangeTypesString =
                            contentChangeTypesToString(params.contentChangeTypes);

                    Log.i(
                            TAG,
                            "waitForEvent called with type: "
                                    + params.eventType
                                    + ", class: "
                                    + params.className
                                    + ", ContentChangeTypes: "
                                    + contentChangeTypesString
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

    private String contentChangeTypesToString(int types) {
        return flagsToString(
                types, AccessibilityTestHelperService::singleContentChangeTypeToString);
    }

    private static String flagsToString(int flags, IntFunction<String> getFlagName) {
        if (flags == 0) {
            return "UNDEFINED";
        }

        // Parsing out the bits from flags bitmask, querying the corresponding
        // value using getFlagName function parameter and appending to the
        // return value.
        StringBuilder builder = new StringBuilder();
        int count = 0;
        while (flags != 0) {
            final int flag = 1 << Integer.numberOfTrailingZeros(flags);
            flags &= ~flag;
            if (count > 0) builder.append(", ");
            builder.append(getFlagName.apply(flag));
            count++;
        }
        return builder.toString();
    }

    private static String singleContentChangeTypeToString(int type) {
        switch (type) {
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_UNDEFINED:
                return "UNDEFINED";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_SUBTREE:
                return "SUBTREE";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_TEXT:
                return "TEXT";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_CONTENT_DESCRIPTION:
                return "CONTENT_DESCRIPTION";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_STATE_DESCRIPTION:
                return "STATE_DESCRIPTION";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_TITLE:
                return "PANE_TITLE";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_APPEARED:
                return "PANE_APPEARED";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_DISAPPEARED:
                return "PANE_DISAPPEARED";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_DRAG_STARTED:
                return "DRAG_STARTED";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_DRAG_CANCELLED:
                return "DRAG_CANCELLED";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_DRAG_DROPPED:
                return "DRAG_DROPPED";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_CONTENT_INVALID:
                return "CONTENT_INVALID";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_ERROR:
                return "ERROR";
            case AccessibilityEvent.CONTENT_CHANGE_TYPE_ENABLED:
                return "ENABLED";
            default:
                return "UNKNOWN: " + Integer.toString(type);
        }
    }
}
