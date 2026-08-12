// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

/** Helper for tests to interact with stubbed out {@link AccessibilityState}. */
public class AccessibilityStateTestHelper {
    public static void setIsComplexUserInteractionServiceEnabledForTesting(boolean enabled) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setIsComplexUserInteractionServiceEnabled(enabled);
        delegate.notifyStateChange();
    }

    public static void setIsTouchExplorationEnabledForTesting(boolean enabled) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setIsTouchExplorationEnabled(enabled);
        delegate.notifyStateChange();
    }

    public static void setIsPerformGesturesEnabledForTesting(boolean enabled) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setIsPerformGesturesEnabled(enabled);
        delegate.notifyStateChange();
    }

    public static void setIsAnyAccessibilityServiceEnabledForTesting(boolean enabled) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setIsAnyAccessibilityServiceEnabled(enabled);
        delegate.notifyStateChange();
    }

    public static void setIsAccessibilityToolPresentForTesting(boolean enabled) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setIsAccessibilityToolPresent(enabled);
        delegate.notifyStateChange();
    }

    public static void setIsTextShowPasswordEnabledForTesting(boolean enabled) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setIsTextShowPasswordEnabled(enabled);
        delegate.notifyStateChange();
    }

    public static void setIsOnlyAutofillRunningForTesting(boolean enabled) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setIsOnlyAutofillRunning(enabled);
        delegate.notifyStateChange();
    }

    public static void setIsOnlyPasswordManagersEnabledForTesting(boolean enabled) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setIsOnlyPasswordManagersEnabled(enabled);
        delegate.notifyStateChange();
    }

    public static void setIsKnownScreenReaderEnabledForTesting(boolean enabled) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setIsKnownScreenReaderEnabled(enabled);
        delegate.notifyStateChange();
    }

    public static void setEventMaskForTesting(int eventMask) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setEventMask(eventMask);
        delegate.notifyStateChange();
    }

    public static void setServiceIdsForTesting(String newServiceId, boolean isAccessibilityTool) {
        FakeAccessibilityStateDelegate delegate = getOrCreateDelegateForTesting();
        delegate.setServiceIds(newServiceId, isAccessibilityTool);
        delegate.notifyStateChange();
    }

    private static FakeAccessibilityStateDelegate getOrCreateDelegateForTesting() {
        AccessibilityStateDelegate delegate = AccessibilityState.getDelegate();
        if (!(delegate instanceof FakeAccessibilityStateDelegate)) {
            delegate = new FakeAccessibilityStateDelegate(() -> AccessibilityState.getListeners());
            AccessibilityState.setDelegateForTesting(delegate);
            ((FakeAccessibilityStateDelegate) delegate).notifyStateChange();
        }
        return (FakeAccessibilityStateDelegate) delegate;
    }

    public static void setAccessibilityEnabledForTesting(boolean isEnabled) {
        setIsPerformGesturesEnabledForTesting(isEnabled);
        setIsTouchExplorationEnabledForTesting(isEnabled);
    }

    public static void uninitializeForTesting() {
        AccessibilityState.setDelegateForTesting(null);
    }
}
