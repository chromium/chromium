// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.accessibilityservice.AccessibilityService;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import javax.annotation.concurrent.GuardedBy;

public class AccessibilityTestService extends AccessibilityService {
    private static final String TAG = "A11yTestService";

    private static AccessibilityTestService sInstance;
    private static final Object sLock = new Object();

    public interface AccessibilityServiceListener {
        default void onAccessibilityEvent(AccessibilityEvent event) {}
    }

    @GuardedBy("sLock")
    private static AccessibilityServiceListener sListener;

    @GuardedBy("sLock")
    private static final List<AccessibilityEvent> sEventCache = new ArrayList<>();

    public static AccessibilityTestService getInstance() {
        return sInstance;
    }

    public static boolean tryWaitForEvent(
            int eventType, String className, String text, long timeoutMs) {
        CompletableFuture<Boolean> eventFuture = new CompletableFuture<>();
        AccessibilityServiceListener listener =
                new AccessibilityServiceListener() {
                    @Override
                    public void onAccessibilityEvent(AccessibilityEvent event) {
                        if (eventMatches(event, eventType, className, text)) {
                            Log.i(TAG, "  Event MATCHED.");
                            eventFuture.complete(true);
                        }
                    }
                };

        synchronized (sLock) {
            // Clear any previous listener, as waitForEvent claims exclusive rights.
            clearListenerLocked();

            if (searchAndConsumeEventCacheLocked(eventType, className, text)) {
                Log.i(TAG, "Found event in cache.");
                return true;
            }

            // Not in cache, clear it and prepare to wait for new events.
            clearEventCacheLocked();
            setListenerLocked(listener);
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
            synchronized (sLock) {
                clearListenerLocked();
            }
        }
    }

    @GuardedBy("sLock")
    public static void setListenerLocked(AccessibilityServiceListener listener) {
        if (sListener != null && listener != null) {
            Log.e(TAG, "Listener already set!");
        }
        sListener = listener;
    }

    @GuardedBy("sLock")
    public static void clearListenerLocked() {
        sListener = null;
    }

    @GuardedBy("sLock")
    public static boolean searchAndConsumeEventCacheLocked(
            int eventType, String className, String text) {
        ListIterator<AccessibilityEvent> iterator = sEventCache.listIterator();
        int foundIndex = -1;
        while (iterator.hasNext()) {
            int index = iterator.nextIndex();
            AccessibilityEvent event = iterator.next();
            if (eventMatches(event, eventType, className, text)) {
                foundIndex = index;
                break;
            }
        }

        if (foundIndex != -1) {
            sEventCache.subList(0, foundIndex + 1).clear();
            return true;
        }
        return false;
    }

    @GuardedBy("sLock")
    public static void clearEventCacheLocked() {
        sEventCache.clear();
    }

    static boolean eventMatches(
            AccessibilityEvent event, int eventType, String className, String text) {
        if (event.getEventType() != eventType) return false;

        AccessibilityNodeInfo source = event.getSource();
        CharSequence sourceClassName = source != null ? source.getClassName() : "";
        CharSequence sourceText = source != null ? source.getText() : "";

        boolean classNameMatches =
                TextUtils.isEmpty(className) || TextUtils.equals(sourceClassName, className);
        boolean textMatches = TextUtils.isEmpty(text) || TextUtils.equals(sourceText, text);

        return classNameMatches && textMatches;
    }

    @Override
    protected void onServiceConnected() {
        super.onServiceConnected();
        Log.d(TAG, "onServiceConnected");
        synchronized (sLock) {
            sInstance = this;
        }
    }

    @Override
    public boolean onUnbind(android.content.Intent intent) {
        Log.d(TAG, "onUnbind");
        sInstance = null;
        synchronized (sLock) {
            clearListenerLocked();
            clearEventCacheLocked();
        }
        return super.onUnbind(intent);
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event) {
        Log.i(TAG, "onAccessibilityEvent: " + event);
        synchronized (sLock) {
            if (sListener != null) {
                sListener.onAccessibilityEvent(event);
            } else {
                sEventCache.add(AccessibilityEvent.obtain(event));
            }
        }
    }

    @Override
    public void onInterrupt() {}
}
