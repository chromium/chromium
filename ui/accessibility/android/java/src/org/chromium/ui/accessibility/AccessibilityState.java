// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import android.view.accessibility.AccessibilityEvent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.WeakHashMap;

/**
 * Provides utility methods relating to measuring accessibility state on Android. See native
 * counterpart in accessibility::AccessibilityState.
 */
@JNINamespace("ui")
@NullMarked
public class AccessibilityState {
    public static final int EVENT_TYPE_MASK_ALL = ~0;
    public static final int EVENT_TYPE_MASK_NONE = 0;

    public static final String AUTOFILL_COMPAT_ACCESSIBILITY_SERVICE_ID =
            "android/com.android.server.autofill.AutofillCompatAccessibilityService";

    // Known screen reader service IDs, currently set only to TalkBack but can be expanded to a list
    // if more screen readers appear in the ecosystem.
    public static final String KNOWN_SCREEN_READER_SERVICE_IDS =
            "com.google.android.marvin.talkback/.TalkBackService";

    /** Interface for the observers of the system's accessibility state. */
    public interface Listener {
        /**
         * Called when any aspect of the system's accessibility state changes. This can happen for
         * example when a user:
         *     - enables/disables an accessibility service (e.g. TalkBack, VoiceAccess, etc.)
         *     - enables/disables a pseudo-accessibility service (e.g. password manager, etc.)
         *     - changes an accessibility-related system setting (e.g. animation duration, password
         *       obscuring, touch exploration, etc.)
         *
         * For a full list of triggers, see: {AccessibilityState#registerObservers}
         * For a full list of tracked settings, see: {AccessibilityState.State}
         *
         * This method passes both the previous and new (old current vs. now current) accessibility
         * state. Clients that are only interested in a subset of the state should compare the
         * oldAccessibilityState to newAccessibilityState to see if a relevant setting changed.
         *
         * @param oldAccessibilityState The previous accessibility state
         * @param newAccessibilityState The new accessibility state
         */
        void onAccessibilityStateChanged(State oldAccessibilityState, State newAccessibilityState);
    }

    /** A representation of the current accessibility state. */
    public static class State {
        // True when we determine that an assistive technology that performs complex user
        // interactions is enabled. False otherwise.
        // Note: This is based on a heuristic from an analysis of the most common assistive
        // technologies on Android. Certain AccessibilityEvents are associated with complex ATs, but
        // do not necessarily indicate the presence of a screen reader. See {@link
        // COMPLEX_USER_INTERACTION_SERVICE_EVENT_TYPE_MASK}.
        public final boolean isComplexUserInteractionServiceEnabled;

        // True when the user has touch exploration enabled. False otherwise.
        public final boolean isTouchExplorationEnabled;

        // True when a service that requested to perform gestures is enabled. False otherwise.
        public final boolean isPerformGesturesEnabled;

        // True when at least one accessibility service is enabled. False otherwise.
        public final boolean isAnyAccessibilityServiceEnabled;

        // True when android version is less than 31 or at least one enabled accessibility service
        // returns true for isAccessibilityTool(). False otherwise.
        public final boolean isAccessibilityToolPresent;

        // True when the user has enabled the Android-OS privacy setting for showing passwords,
        // found in: Settings > Privacy > Show passwords. (Settings.System.TEXT_SHOW_PASSWORD).
        // False otherwise.
        public final boolean isTextShowPasswordEnabled;

        // True when the autofill manager is enabled and the autofill service is the only service
        // running that requires accessibility.
        public final boolean isOnlyAutofillRunning;

        // True when we suspect that only password managers are enabled, based on the information
        // from running accessibility services. False otherwise.
        public final boolean isOnlyPasswordManagersEnabled;

        // True when a known screen reader is enabled, based on service IDs. False otherwise.
        public final boolean isKnownScreenReaderEnabled;

        public State(
                boolean isComplexUserInteractionServiceEnabled,
                boolean isTouchExplorationEnabled,
                boolean isPerformGesturesEnabled,
                boolean isAnyAccessibilityServiceEnabled,
                boolean isAccessibilityToolPresent,
                boolean isTextShowPasswordEnabled,
                boolean isOnlyAutofillRunning,
                boolean isOnlyPasswordManagersEnabled,
                boolean isKnownScreenReaderEnabled) {
            this.isComplexUserInteractionServiceEnabled = isComplexUserInteractionServiceEnabled;
            this.isTouchExplorationEnabled = isTouchExplorationEnabled;
            this.isPerformGesturesEnabled = isPerformGesturesEnabled;
            this.isAnyAccessibilityServiceEnabled = isAnyAccessibilityServiceEnabled;
            this.isAccessibilityToolPresent = isAccessibilityToolPresent;
            this.isTextShowPasswordEnabled = isTextShowPasswordEnabled;
            this.isOnlyAutofillRunning = isOnlyAutofillRunning;
            this.isOnlyPasswordManagersEnabled = isOnlyPasswordManagersEnabled;
            this.isKnownScreenReaderEnabled = isKnownScreenReaderEnabled;
        }

        @Override
        public String toString() {
            return "State{"
                    + "isComplexUserInteractionServiceEnabled="
                    + isComplexUserInteractionServiceEnabled
                    + ", isTouchExplorationEnabled="
                    + isTouchExplorationEnabled
                    + ", isPerformGesturesEnabled="
                    + isPerformGesturesEnabled
                    + ", isAnyAccessibilityServiceEnabled="
                    + isAnyAccessibilityServiceEnabled
                    + ", isAccessibilityToolPresent="
                    + isAccessibilityToolPresent
                    + ", isTextShowPasswordEnabled="
                    + isTextShowPasswordEnabled
                    + ", isOnlyAutofillRunning="
                    + isOnlyAutofillRunning
                    + ", isOnlyPasswordManagersEnabled="
                    + isOnlyPasswordManagersEnabled
                    + ", isKnownScreenReaderEnabled="
                    + isKnownScreenReaderEnabled
                    + '}';
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (this == obj) return true;
            if (!(obj instanceof State)) return false;
            State other = (State) obj;
            return isComplexUserInteractionServiceEnabled
                            == other.isComplexUserInteractionServiceEnabled
                    && isTouchExplorationEnabled == other.isTouchExplorationEnabled
                    && isPerformGesturesEnabled == other.isPerformGesturesEnabled
                    && isAnyAccessibilityServiceEnabled == other.isAnyAccessibilityServiceEnabled
                    && isAccessibilityToolPresent == other.isAccessibilityToolPresent
                    && isTextShowPasswordEnabled == other.isTextShowPasswordEnabled
                    && isOnlyAutofillRunning == other.isOnlyAutofillRunning
                    && isOnlyPasswordManagersEnabled == other.isOnlyPasswordManagersEnabled
                    && isKnownScreenReaderEnabled == other.isKnownScreenReaderEnabled;
        }
    }

    private static @Nullable AccessibilityStateDelegate sDelegate;

    // The set of listeners of AccessibilityState, implemented using
    // a WeakHashSet behind the scenes so that listeners can be garbage-collected
    // and will be automatically removed from this set.
    // Not a member of delegate because {@code sListeners} can outlive the delegate in JUnit
    // tests. Static instances such as {@link DeviceAccessibilitySettingsHandler} add listeners to
    // AccessibilityState. These static instances are reused between JUnit test suites.
    private static final Set<Listener> sListeners =
            Collections.newSetFromMap(new WeakHashMap<Listener, Boolean>());

    static AccessibilityStateDelegate getDelegate() {
        if (sDelegate == null) {
            sDelegate = new AccessibilityStateDelegateImpl(() -> sListeners);
        }
        return sDelegate;
    }

    /** Should be called on UI thread. */
    public static void addListener(Listener listener) {
        assert ThreadUtils.runningOnUiThread();
        sListeners.add(listener);
    }

    public static Set<Listener> getListeners() {
        return sListeners;
    }

    public static boolean isComplexUserInteractionServiceEnabled() {
        return getDelegate().isComplexUserInteractionServiceEnabled();
    }

    public static boolean isTouchExplorationEnabled() {
        return getDelegate().isTouchExplorationEnabled();
    }

    public static boolean isPerformGesturesEnabled() {
        return getDelegate().isPerformGesturesEnabled();
    }

    public static boolean isAnyAccessibilityServiceEnabled() {
        return getDelegate().isAnyAccessibilityServiceEnabled();
    }

    public static boolean isAccessibilityManagerEnabled() {
        return getDelegate().isAccessibilityManagerEnabled();
    }

    public static boolean isAccessibilityToolPresent() {
        return getDelegate().isAccessibilityToolPresent();
    }

    public static boolean isTextShowPasswordEnabled() {
        return getDelegate().isTextShowPasswordEnabled();
    }

    public static boolean isOnlyAutofillRunning() {
        return getDelegate().isOnlyAutofillRunning();
    }

    public static boolean isOnlyPasswordManagersEnabled() {
        return getDelegate().isOnlyPasswordManagersEnabled();
    }

    public static boolean isKnownScreenReaderEnabled() {
        return getDelegate().isKnownScreenReaderEnabled();
    }

    public static boolean isDisplayInversionEnabled() {
        return getDelegate().isDisplayInversionEnabled();
    }

    public static boolean isHighContrastEnabled() {
        return getDelegate().isHighContrastEnabled();
    }

    public static int getNumberOfRunningServices() {
        return getDelegate().getNumberOfRunningServices();
    }

    public static int getFontWeightAdjustment() {
        return getDelegate().getFontWeightAdjustment();
    }

    /**
     * Helper method to return the value that is equivalent to the deprecated approach:
     *     ChromeAccessibilityUtil.get().isAccessibilityEnabled()
     *
     * Avoid calling this method at all costs. The naming of this method is misleading and its
     * usage is tricky. Use the more granular methods of this class.
     *
     * Returns true if an accessibility service is running that uses touch exploration OR a service
     * is running that can perform gestures.
     *
     * @return true when touch exploration or gesture performing services are running.
     */
    // TODO(mschillaci): Replace all calls of this method with newer approach.
    @Deprecated
    public static boolean isAccessibilityEnabled() {
        return AccessibilityState.isTouchExplorationEnabled()
                || AccessibilityState.isPerformGesturesEnabled();
    }

    public static int getRecommendedTimeoutMillis(int minimumTimeout, int nonA11yTimeout) {
        return getDelegate().getRecommendedTimeoutMillis(minimumTimeout, nonA11yTimeout);
    }

    @Deprecated
    public static void sendAccessibilityEvent(AccessibilityEvent event) {
        getDelegate().sendAccessibilityEvent(event);
    }

    public static float getAnimatorDurationScale() {
        return getDelegate().getAnimatorDurationScale();
    }

    @CalledByNative
    public static int getTextCursorBlinkInterval() {
        return getDelegate().getTextCursorBlinkInterval();
    }

    /** Returns whether the user settings specify preferred reduced motion. */
    @CalledByNative
    public static boolean prefersReducedMotion() {
        return getAnimatorDurationScale() == 0.0;
    }

    public static Set<Integer> relevantEventTypesForCurrentServices() {
        Set<Integer> relevantEventTypes = new HashSet<>();
        int eventTypeBit;
        int currentEventTypes = getAccessibilityServiceEventTypeMask();
        while (currentEventTypes != 0) {
            eventTypeBit = (1 << Integer.numberOfTrailingZeros(currentEventTypes));
            relevantEventTypes.add(eventTypeBit);
            currentEventTypes &= ~eventTypeBit;
        }

        return relevantEventTypes;
    }

    @CalledByNative
    private static int getAccessibilityServiceEventTypeMask() {
        return getDelegate().getAccessibilityServiceEventTypeMask();
    }

    @CalledByNative
    private static int getAccessibilityServiceFeedbackTypeMask() {
        return getDelegate().getAccessibilityServiceFeedbackTypeMask();
    }

    @CalledByNative
    private static int getAccessibilityServiceFlagsMask() {
        return getDelegate().getAccessibilityServiceFlagsMask();
    }

    @CalledByNative
    private static int getAccessibilityServiceCapabilitiesMask() {
        return getDelegate().getAccessibilityServiceCapabilitiesMask();
    }

    @CalledByNative
    private static String[] getAccessibilityServiceIds() {
        return getDelegate().getAccessibilityServiceIds();
    }

    @CalledByNative
    private static boolean[] getAccessibilityToolFlags() {
        return getDelegate().getAccessibilityToolFlags();
    }

    public static void registerObservers() {
        getDelegate().registerObservers();
    }

    public static void initializeOnStartup() {
        getDelegate().initializeOnStartup();
    }

    @NativeMethods
    interface Natives {
        void onAnimatorDurationScaleChanged();

        void onDisplayInversionEnabledChanged(boolean enabled);

        void onContrastLevelChanged(boolean highContrastEnabled);

        void onTextCursorBlinkIntervalChanged(int textCursorBlinkInterval);

        void recordAccessibilityServiceInfoHistograms();
    }

    public static void setDelegateForTesting(@Nullable AccessibilityStateDelegate delegate) {
        if (sDelegate != null) {
            sDelegate.uninitializeForTesting();
        }
        sDelegate = delegate;
    }
}
