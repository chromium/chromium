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

    public static void setListener(AccessibilityServiceListener listener) {
        synchronized (sLock) {
            if (sListener != null && listener != null) {
                Log.e(TAG, "Listener already set!");
            }
            sListener = listener;
        }
    }

    public static void clearListener() {
        synchronized (sLock) {
            sListener = null;
        }
    }

    public static boolean searchAndConsumeEventCache(int eventType, String className, String text) {
        synchronized (sLock) {
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
        }

        clearEventCache();
        return false;
    }

    public static void clearEventCache() {
        synchronized (sLock) {
            sEventCache.clear();
        }
    }

    public static boolean tryConsumeCachedEvent(
            int eventType, String className, String text, AccessibilityServiceListener listener) {
        synchronized (sLock) {
            // Clear any previous listener, as waitForEvent claims exclusive rights.
            clearListener();

            // Check the cache first.
            if (searchAndConsumeEventCache(eventType, className, text)) {
                Log.i(TAG, "Found event in cache.");
                return true;
            }

            setListener(listener);
            return false;
        }
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
            clearListener();
            clearEventCache();
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
