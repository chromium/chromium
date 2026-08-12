// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import android.view.accessibility.AccessibilityEvent;

import org.chromium.build.annotations.NullMarked;

/** Delegate for interacting with accessibility state. */
@NullMarked
interface AccessibilityStateDelegate {
    boolean isAccessibilityToolPresent();

    boolean isTextShowPasswordEnabled();

    boolean isOnlyAutofillRunning();

    boolean isOnlyPasswordManagersEnabled();

    boolean isKnownScreenReaderEnabled();

    boolean isComplexUserInteractionServiceEnabled();

    /** True when touch exploration is enabled. */
    boolean isTouchExplorationEnabled();

    /** True when perform gestures is enabled. */
    boolean isPerformGesturesEnabled();

    /** True when at least one accessibility service is enabled on the system. */
    boolean isAnyAccessibilityServiceEnabled();

    /**
     * Returns the value of AccessibilityManager.isEnabled(). This indicates whether the
     * accessibility manager is currently enabled.
     *
     * @return true if the accessibility manager is enabled.
     */
    boolean isAccessibilityManagerEnabled();

    boolean isDisplayInversionEnabled();

    boolean isHighContrastEnabled();

    int getNumberOfRunningServices();

    /**
     * The current font weight adjustment set at the Android-OS level. Initialized to be 0, the
     * default font weight. If a user has the bold text setting enabled, this will be 300. This is
     * not included as a part of the {State} object since it is only needed for the web contents
     * rendering (native widgets have font weight adjusted by the framework). This is only available
     * on Android S+, on previous versions of Android this is always 0.
     */
    int getFontWeightAdjustment();

    /**
     * Convenience method to get a recommended timeout on all versions of Android. The method that
     * is part of AccessibilityManager is only available on Android >= Q. For earlier versions of
     * Android, we will multiply by an arbitrary constant.
     *
     * <p>This method will query the AccessibilityManager, which considers the currently running
     * services, to provide a suggested timeout. On Android >= Q, the returned value may not be
     * either of the provided timeouts, and for versions < Q this will return the maximum of the two
     * timeouts.
     *
     * @param minimumTimeout - minimum allowed timeout for the calling feature.
     * @param nonA11yTimeout - the timeout if no a11y services are running for the feature.
     * @return Suggested timeout given the currently running services (in milliseconds).
     */
    int getRecommendedTimeoutMillis(int minimumTimeout, int nonA11yTimeout);

    /**
     * Convenience method to send an AccessibilityEvent to the system's AccessibilityManager without
     * requiring a hard dependency on AccessibilityManager or an instance of a View. If this method
     * is called when accessibility has been disabled (e.g. stale state after calling off the main
     * thread), then the event will be ignored. If an event is sent, this does not guarantee a
     * correct user experience for downstream AT.
     *
     * <p>Note: This should only be used in exceptional situations. Apps can generally achieve the
     * correct behavior for accessibility with a semantically correct UI. Deprecated to prompt dev
     * to reconsider their approach.
     *
     * @param event AccessibilityEvent to send to the AccessibilityManager
     */
    @Deprecated
    void sendAccessibilityEvent(AccessibilityEvent event);

    /** Returns the current ANIMATOR_DURATION_SCALE from the users OS accessibility settings. */
    float getAnimatorDurationScale();

    /** Returns the current TEXT_CURSOR_BLINK_INTERVAL from the users OS accessibility settings. */
    int getTextCursorBlinkInterval();

    /**
     * Return a bitmask containing the union of all event types that running accessibility services
     * listen to.
     */
    int getAccessibilityServiceEventTypeMask();

    /**
     * Return a bitmask containing the union of all feedback types that running accessibility
     * services provide.
     */
    int getAccessibilityServiceFeedbackTypeMask();

    /** Return a bitmask containing the union of all flags from running accessibility services. */
    int getAccessibilityServiceFlagsMask();

    /**
     * Return a bitmask containing the union of all service capabilities from running accessibility
     * services.
     */
    int getAccessibilityServiceCapabilitiesMask();

    /** Return a list of ids of all running accessibility services. */
    String[] getAccessibilityServiceIds();

    /**
     * Return a list of whether running accessibility services have {@code isAccessibilityTool=true}
     * declared in their manifest. Note that {@code isAccessibilityTool} was introduced in Android
     * S; on earlier Android versions this will return all {@code false}. The returned array will
     * have the same length as the array returned by {@link #getAccessibilityServiceIds()}.
     */
    boolean[] getAccessibilityToolFlags();

    /**
     * Register observers of various system properties and initialize a state for clients.
     *
     * <p>Note: This should only be called once, and before any client queries of accessibility
     * state. The first time any client queries the state, |this| will be initialized.
     */
    void registerObservers();

    void initializeOnStartup();

    void updateAccessibilityServices();

    /**
     * Unregisters all observers registered by the delegate, not just those registered by {@link
     * #registerObservers()}.
     */
    void uninitializeForTesting();
}
