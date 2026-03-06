// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

import org.chromium.ui.accessibility.testservice.WaitForEventParams;

interface IAccessibilityTestHelperService {
    /**
     * Waits for an accessibility event matching the given query parameters.
     * Returns true if the event is received within the timeout, false otherwise.
     *
     * @param params The event query parameters.
     */
    boolean waitForEvent(in WaitForEventParams params);

    /**
     * Finds a node matching the criteria and performs the given action on it.
     *
     * @param className The class name to match.
     * @param text The text to match.
     * @param action The action to perform (e.g., AccessibilityNodeInfo.ACTION_HOVER_ENTER).
     * @return true if the action was performed successfully.
     */
    boolean performActionOnNode(String className, String text, int action);
}
