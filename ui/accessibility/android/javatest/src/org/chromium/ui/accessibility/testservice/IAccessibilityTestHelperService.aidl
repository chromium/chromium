// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

interface IAccessibilityTestHelperService {
    /**
     * Waits for an accessibility event of the given type on a node
     * with the given class name. Returns true if the event is received
     * within the timeout, false otherwise.
     *
     * @param eventType The type of event to wait for (e.g.,
     *     AccessibilityEvent.TYPE_VIEW_FOCUSED).
     * @param className The expected class name of the event source.  Null or empty string matches any class name.
     * @param text The expected text of the event source. Null or empty string matches any text.
     * @param timeoutMs The maximum time to wait in milliseconds.
     */
    boolean waitForEvent(int eventType, String className, String text, long timeoutMs);
}
