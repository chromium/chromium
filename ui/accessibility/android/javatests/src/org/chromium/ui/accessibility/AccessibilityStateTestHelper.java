// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

/** Helper for tests to interact with stubbed out {@link AccessibilityState}. */
public class AccessibilityStateTestHelper {
    public static void setIsComplexUserInteractionServiceEnabledForTesting(boolean enabled) {
        getOrCreateDelegateForTesting()
                .setIsComplexUserInteractionServiceEnabledForTesting(enabled);
    }

    public static void setIsTouchExplorationEnabledForTesting(boolean enabled) {
        getOrCreateDelegateForTesting().setIsTouchExplorationEnabledForTesting(enabled);
    }

    public static void setIsPerformGesturesEnabledForTesting(boolean enabled) {
        getOrCreateDelegateForTesting().setIsPerformGesturesEnabledForTesting(enabled);
    }

    public static void setIsAnyAccessibilityServiceEnabledForTesting(boolean enabled) {
        getOrCreateDelegateForTesting().setIsAnyAccessibilityServiceEnabledForTesting(enabled);
    }

    public static void setIsAccessibilityToolPresentForTesting(boolean enabled) {
        getOrCreateDelegateForTesting().setIsAccessibilityToolPresentForTesting(enabled);
    }

    public static void setIsTextShowPasswordEnabledForTesting(boolean enabled) {
        getOrCreateDelegateForTesting().setIsTextShowPasswordEnabledForTesting(enabled);
    }

    public static void setIsOnlyAutofillRunningForTesting(boolean enabled) {
        getOrCreateDelegateForTesting().setIsOnlyAutofillRunningForTesting(enabled);
    }

    public static void setIsOnlyPasswordManagersEnabledForTesting(boolean enabled) {
        getOrCreateDelegateForTesting().setIsOnlyPasswordManagersEnabledForTesting(enabled);
    }

    public static void setIsKnownScreenReaderEnabledForTesting(boolean enabled) {
        getOrCreateDelegateForTesting().setIsKnownScreenReaderEnabledForTesting(enabled);
    }

    public static void setEventMaskForTesting(int eventMask) {
        getOrCreateDelegateForTesting().setEventMaskForTesting(eventMask);
    }

    public static void setServiceIdsForTesting(String newServiceId, boolean isAccessibilityTool) {
        getOrCreateDelegateForTesting().setServiceIdsForTesting(newServiceId, isAccessibilityTool);
    }

    private static FakeAccessibilityStateDelegate getOrCreateDelegateForTesting() {
        AccessibilityStateDelegateImpl delegate = AccessibilityState.getDelegate();
        if (!(delegate instanceof FakeAccessibilityStateDelegate)) {
            delegate = new FakeAccessibilityStateDelegate(() -> AccessibilityState.getListeners());
            AccessibilityState.setDelegateForTesting(delegate);
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
