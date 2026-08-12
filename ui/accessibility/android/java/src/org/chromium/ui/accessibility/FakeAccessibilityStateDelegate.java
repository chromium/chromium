// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import android.view.accessibility.AccessibilityEvent;

import org.chromium.build.annotations.NullMarked;

import java.util.Set;

/** Fake implementation of {@link AccessibilityStateDelegate} for testing. */
@NullMarked
public class FakeAccessibilityStateDelegate implements AccessibilityStateDelegate {
    private boolean mIsComplexUserInteractionServiceEnabled;
    private boolean mIsTouchExplorationEnabled;
    private boolean mIsPerformGesturesEnabled;
    private boolean mIsAnyAccessibilityServiceEnabled;
    private boolean mIsAccessibilityToolPresent;
    private boolean mIsTextShowPasswordEnabled;
    private boolean mIsOnlyAutofillRunning;
    private boolean mIsOnlyPasswordManagersEnabled;
    private boolean mIsKnownScreenReaderEnabled;

    private int mEventTypeMask;
    private String[] mAccessibilityServiceIds = new String[0];
    private boolean[] mAccessibilityToolFlags = new boolean[0];

    private AccessibilityState.State mLastNotifiedState =
            new AccessibilityState.State(
                    false, false, false, false, false, false, false, false, false);

    private final ListenerCallback mListenerCallback;

    interface ListenerCallback {
        Set<AccessibilityState.Listener> getListeners();
    }

    public FakeAccessibilityStateDelegate(ListenerCallback listenerCallback) {
        mListenerCallback = listenerCallback;
    }

    private AccessibilityState.State getState() {
        return new AccessibilityState.State(
                mIsComplexUserInteractionServiceEnabled,
                mIsTouchExplorationEnabled,
                mIsPerformGesturesEnabled,
                mIsAnyAccessibilityServiceEnabled,
                mIsAccessibilityToolPresent,
                mIsTextShowPasswordEnabled,
                mIsOnlyAutofillRunning,
                mIsOnlyPasswordManagersEnabled,
                mIsKnownScreenReaderEnabled);
    }

    @Override
    public boolean isComplexUserInteractionServiceEnabled() {
        return mIsComplexUserInteractionServiceEnabled;
    }

    @Override
    public boolean isTouchExplorationEnabled() {
        return mIsTouchExplorationEnabled;
    }

    @Override
    public boolean isAnyAccessibilityServiceEnabled() {
        return mIsAnyAccessibilityServiceEnabled;
    }

    @Override
    public boolean isAccessibilityToolPresent() {
        return mIsAccessibilityToolPresent;
    }

    @Override
    public boolean isTextShowPasswordEnabled() {
        return mIsTextShowPasswordEnabled;
    }

    @Override
    public boolean isOnlyAutofillRunning() {
        return mIsOnlyAutofillRunning;
    }

    @Override
    public boolean isOnlyPasswordManagersEnabled() {
        return mIsOnlyPasswordManagersEnabled;
    }

    @Override
    public boolean isKnownScreenReaderEnabled() {
        return mIsKnownScreenReaderEnabled;
    }

    public void notifyStateChange() {
        AccessibilityState.State currentState = getState();
        for (AccessibilityState.Listener listener : mListenerCallback.getListeners()) {
            listener.onAccessibilityStateChanged(mLastNotifiedState, currentState);
        }
        mLastNotifiedState = currentState;
    }

    public void setIsComplexUserInteractionServiceEnabled(boolean enabled) {
        mIsComplexUserInteractionServiceEnabled = enabled;
    }

    public void setIsTouchExplorationEnabled(boolean enabled) {
        mIsTouchExplorationEnabled = enabled;
    }

    public void setIsPerformGesturesEnabled(boolean enabled) {
        mIsPerformGesturesEnabled = enabled;
    }

    public void setIsAnyAccessibilityServiceEnabled(boolean enabled) {
        mIsAnyAccessibilityServiceEnabled = enabled;
    }

    public void setIsAccessibilityToolPresent(boolean enabled) {
        mIsAccessibilityToolPresent = enabled;
    }

    public void setIsTextShowPasswordEnabled(boolean enabled) {
        mIsTextShowPasswordEnabled = enabled;
    }

    public void setIsOnlyAutofillRunning(boolean enabled) {
        mIsOnlyAutofillRunning = enabled;
    }

    public void setIsOnlyPasswordManagersEnabled(boolean enabled) {
        mIsOnlyPasswordManagersEnabled = enabled;
    }

    public void setIsKnownScreenReaderEnabled(boolean enabled) {
        mIsKnownScreenReaderEnabled = enabled;
    }

    public void setEventMask(int eventMask) {
        mEventTypeMask = eventMask;
    }

    public void setServiceIds(String newServiceId, boolean isAccessibilityTool) {
        mAccessibilityServiceIds = new String[] {newServiceId};
        mAccessibilityToolFlags = new boolean[] {isAccessibilityTool};
    }

    @Override
    public boolean isPerformGesturesEnabled() {
        return mIsPerformGesturesEnabled;
    }

    @Override
    public boolean isAccessibilityManagerEnabled() {
        return isAnyAccessibilityServiceEnabled();
    }

    @Override
    public int getNumberOfRunningServices() {
        return mAccessibilityServiceIds.length;
    }

    @Override
    public int getFontWeightAdjustment() {
        return 0;
    }

    @Override
    public int getRecommendedTimeoutMillis(int minimumTimeout, int nonA11yTimeout) {
        return Math.max(minimumTimeout, nonA11yTimeout);
    }

    @Override
    public void sendAccessibilityEvent(AccessibilityEvent event) {}

    @Override
    public float getAnimatorDurationScale() {
        return 1.0f;
    }

    @Override
    public int getTextCursorBlinkInterval() {
        return 500;
    }

    @Override
    public boolean isDisplayInversionEnabled() {
        return false;
    }

    @Override
    public boolean isHighContrastEnabled() {
        return false;
    }

    @Override
    public void registerObservers() {}

    @Override
    public void initializeOnStartup() {}

    @Override
    public int getAccessibilityServiceEventTypeMask() {
        return mEventTypeMask;
    }

    @Override
    public int getAccessibilityServiceFeedbackTypeMask() {
        return 0;
    }

    @Override
    public int getAccessibilityServiceFlagsMask() {
        return 0;
    }

    @Override
    public int getAccessibilityServiceCapabilitiesMask() {
        return 0;
    }

    @Override
    public String[] getAccessibilityServiceIds() {
        return mAccessibilityServiceIds;
    }

    @Override
    public boolean[] getAccessibilityToolFlags() {
        return mAccessibilityToolFlags;
    }

    @Override
    public void updateAccessibilityServices() {}

    @Override
    public void uninitializeForTesting() {}
}
