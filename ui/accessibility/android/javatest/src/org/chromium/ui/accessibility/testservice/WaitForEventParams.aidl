// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility.testservice;

parcelable WaitForEventParams {
    /** The type of event to wait for (e.g., AccessibilityEvent.TYPE_VIEW_FOCUSED). */
    int eventType;
    /** The expected class name of the event source. Null or empty string matches any class name. */
    String className;
    /** The expected text of the event source. Null or empty string matches any text. */
    String text;
    /** The maximum time to wait in milliseconds. */
    long timeoutMs;
}
