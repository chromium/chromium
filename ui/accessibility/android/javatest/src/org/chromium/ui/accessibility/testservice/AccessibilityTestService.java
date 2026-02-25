// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.accessibilityservice.AccessibilityService;
import android.content.Intent;
import android.view.accessibility.AccessibilityEvent;

import org.chromium.base.Log;

public class AccessibilityTestService extends AccessibilityService {
    private static final String TAG = "A11yTestService";
    private static final String ACTION_ACCESSIBILITY_EVENT =
            "org.chromium.ui.accessibility.testservice.ACCESSIBILITY_EVENT";

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        Intent intent = new Intent(ACTION_ACCESSIBILITY_EVENT);
        sendBroadcast(intent);
    }

    @Override
    public void onInterrupt() {}

    @Override
    protected void onServiceConnected() {
        super.onServiceConnected();
        Log.d(TAG, "onServiceConnected");
    }
}
