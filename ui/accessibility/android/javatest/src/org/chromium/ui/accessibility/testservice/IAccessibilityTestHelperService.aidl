// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import android.os.Bundle;
import org.chromium.ui.accessibility.testservice.EventMatcher;
import org.chromium.ui.accessibility.testservice.NodeMatcher;

interface IAccessibilityTestHelperService {
    /**
     * Waits for an accessibility event matching the given matcher.
     * Returns true if the condition is met within the timeout, false otherwise.
     * We will first attempt to look after the event in the cache. If we do, we
     * will erase the cache up to the point of the matched event (inclusive). If
     * we don't, we'll listen after incoming events. After an event matched, no
     * event having arrived earlier than that should stay in the cache.
     *
     * @param matcher The event matching criteria.
     * @param timeoutMs The maximum time to wait in milliseconds.
     */
    boolean waitForEvent(in EventMatcher matcher, long timeoutMs);

    /**
     * Waits for an accessibility node matching the given matcher.
     * Returns true if the condition is met within the timeout, false otherwise.
     *
     * @param matcher The node matching criteria.
     * @param timeoutMs The maximum time to wait in milliseconds.
     */
    boolean waitForNode(in NodeMatcher matcher, long timeoutMs);

    /**
     * Waits for a window to become active.
     * Returns true if an active window is present within the timeout, false otherwise.
     *
     * @param timeoutMs The maximum time to wait in milliseconds.
     */
    boolean waitForActiveWindow(long timeoutMs);

    /**
     * Finds a node matching the matcher and performs the given action on it.
     *
     * @param matcher The node matching criteria.
     * @param action The action to perform (e.g., AccessibilityNodeInfo.ACTION_HOVER_ENTER).
     * @param arguments The arguments bundle, which can be null.
     * @return true if the action was performed successfully.
     */
    boolean performActionOnNode(
            in NodeMatcher matcher, int action, in @nullable Bundle arguments);

    /**
     * Dumps the accessibility tree to a String.
     *
     * @return The accessibility tree as a String.
     */
    String dumpWebContentsAccessibilityTree();
}
