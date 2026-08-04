// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import org.chromium.build.annotations.NullMarked;

/** Fake implementation of {@link AccessibilityStateDelegateImpl} for testing. */
@NullMarked
public class FakeAccessibilityStateDelegate extends AccessibilityStateDelegateImpl {
    public FakeAccessibilityStateDelegate(
            AccessibilityStateDelegateImpl.ListenerCallback listenerCallback) {
        super(listenerCallback);
    }
}
