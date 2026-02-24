// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.view.accessibility.AccessibilityEvent;

import org.chromium.base.Log;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

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

                    CompletableFuture<Boolean> eventFuture = new CompletableFuture<>();
                    AccessibilityTestService.AccessibilityServiceListener listener =
                            new AccessibilityTestService.AccessibilityServiceListener() {
                                @Override
                                public void onAccessibilityEvent(AccessibilityEvent event) {
                                    if (AccessibilityTestService.eventMatches(
                                            event, eventType, className, text)) {
                                        Log.i(TAG, "  Event MATCHED.");
                                        eventFuture.complete(true);
                                    }
                                }
                            };

                    if (AccessibilityTestService.tryConsumeCachedEvent(
                            eventType, className, text, listener)) {
                        return true;
                    }

                    // Did not find the event in the cache, so wait on the listener to return.
                    try {
                        return eventFuture.get(timeoutMs, TimeUnit.MILLISECONDS);
                    } catch (TimeoutException e) {
                        Log.w(TAG, "Timed out waiting for event");
                        return false;
                    } catch (Exception e) {
                        Log.e(TAG, "Error waiting for event", e);
                        return false;
                    } finally {
                        // Ensure the listener is cleared in all cases.
                        AccessibilityTestService.clearListener();
                    }
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
