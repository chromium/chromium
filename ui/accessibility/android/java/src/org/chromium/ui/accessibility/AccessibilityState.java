// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import static android.accessibilityservice.AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES;
import static android.accessibilityservice.AccessibilityServiceInfo.FEEDBACK_SPOKEN;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_CONTROLS;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_ICONS;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_TEXT;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.autofill.AutofillManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.WeakHashMap;

/**
 * Provides utility methods relating to measuring accessibility state on Android. See native
 * counterpart in ui::accessibility::AccessibilityState.
 */
@JNINamespace("ui")
public class AccessibilityState {
    private static final String TAG = "A11yState";

    public static final int EVENT_TYPE_MASK_ALL = ~0;
    public static final int EVENT_TYPE_MASK_NONE = 0;

    public static final String AUTOFILL_COMPAT_ACCESSIBILITY_SERVICE_ID =
            "android/com.android.server.autofill.AutofillCompatAccessibilityService";

    // Constant value to multiply animation timeouts by for pre-Q Android versions.
    private static final int ANIMATION_TIMEOUT_MULTIPLIER = 2;

    // Histogram strings and constants.
    private static final String UPDATE_ACCESSIBILITY_SERVICES_DID_POLL =
            "Accessibility.Android.UpdateAccessibilityServices.DidPoll";
    private static final String UPDATE_ACCESSIBILITY_SERVICES_POLL_COUNT =
            "Accessibility.Android.UpdateAccessibilityServices.PollCount";
    private static final String UPDATE_ACCESSIBILITY_SERVICES_POLL_TIMEOUT =
            "Accessibility.Android.UpdateAccessibilityServices.PollTimeout";
    private static final String UPDATE_ACCESSIBILITY_SERVICES_RUNTIME =
            "Accessibility.Android.UpdateAccessibilityServices.Runtime";
    private static final int MAX_RUNTIME_BUCKET = 16 * 1000; // 16,000 microseconds = 16ms.
    private static int sPollCount;

    /**
     * Interface for the observers of the system's accessibility state.
     */
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
        // True when we determine that genuine assistive technology such as a screen reader
        // is running, based on the information from running accessibility services. False
        // otherwise.
        public final boolean isScreenReaderEnabled;

        // True when the user has touch exploration enabled. False otherwise.
        public final boolean isTouchExplorationEnabled;

        // True when a service that requested to perform gestures is enabled. False otherwise.
        public final boolean isPerformGesturesEnabled;

        // True when at least one accessibility service is enabled. False otherwise.
        public final boolean isAnyAccessibilityServiceEnabled;

        // True when android version is less than 31 or at least one enabled accessibility service
        // returns true for isAccessibilityTool(). False otherwise.
        public final boolean isAccessibilityToolPresent;

        // True when the user is running at least one service that requests the FEEDBACK_SPOKEN
        // feedback type in AccessibilityServiceInfo. False otherwise.
        public final boolean isSpokenFeedbackServicePresent;

        // True when the user has enabled the Android-OS privacy setting for showing passwords,
        // found in: Settings > Privacy > Show passwords. (Settings.System.TEXT_SHOW_PASSWORD).
        // False otherwise.
        public final boolean isTextShowPasswordEnabled;

        // True when we suspect that only password managers are enabled, based on the information
        // from running accessibility services. False otherwise.
        public final boolean isOnlyPasswordManagersEnabled;

        public State(boolean isScreenReaderEnabled, boolean isTouchExplorationEnabled,
                boolean isPerformGesturesEnabled, boolean isAnyAccessibilityServiceEnabled,
                boolean isAccessibilityToolPresent, boolean isSpokenFeedbackServicePresent,
                boolean isTextShowPasswordEnabled, boolean isOnlyPasswordManagersEnabled) {
            this.isScreenReaderEnabled = isScreenReaderEnabled;
            this.isTouchExplorationEnabled = isTouchExplorationEnabled;
            this.isPerformGesturesEnabled = isPerformGesturesEnabled;
            this.isAnyAccessibilityServiceEnabled = isAnyAccessibilityServiceEnabled;
            this.isAccessibilityToolPresent = isAccessibilityToolPresent;
            this.isSpokenFeedbackServicePresent = isSpokenFeedbackServicePresent;
            this.isTextShowPasswordEnabled = isTextShowPasswordEnabled;
            this.isOnlyPasswordManagersEnabled = isOnlyPasswordManagersEnabled;
        }

        @NonNull
        @Override
        public String toString() {
            return "State{"
                    + "isScreenReaderEnabled=" + isScreenReaderEnabled
                    + ", isTouchExplorationEnabled=" + isTouchExplorationEnabled
                    + ", isPerformGesturesEnabled=" + isPerformGesturesEnabled
                    + ", isAnyAccessibilityServiceEnabled=" + isAnyAccessibilityServiceEnabled
                    + ", isAccessibilityToolPresent=" + isAccessibilityToolPresent
                    + ", isSpokenFeedbackServicePresent=" + isSpokenFeedbackServicePresent
                    + ", isTextShowPasswordEnabled=" + isTextShowPasswordEnabled
                    + ", isOnlyPasswordManagersEnabled=" + isOnlyPasswordManagersEnabled + '}';
        }
    }

    // Analysis of the most popular accessibility services on Android suggests
    // that any service that requests any of these three events is a screen reader
    // or other complete assistive technology. If none of these events are requested,
    // we can enable some optimizations.
    private static final int SCREEN_READER_EVENT_TYPE_MASK = AccessibilityEvent.TYPE_VIEW_SELECTED
            | AccessibilityEvent.TYPE_VIEW_SCROLLED | AccessibilityEvent.TYPE_ANNOUNCEMENT;

    // Analysis of the most popular password managers on Android suggests
    // that services that only request these events, flags, and capabilities is likely a password
    // manager. If not more than these events are requested, we can enable some optimizations.
    private static final int PASSWORD_MANAGER_EVENT_TYPE_MASK = AccessibilityEvent.TYPE_VIEW_CLICKED
            | AccessibilityEvent.TYPE_VIEW_FOCUSED | AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED
            | AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED
            | AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED;

    private static final int PASSWORD_MANAGER_FLAG_TYPE_MASK = AccessibilityServiceInfo.DEFAULT
            | AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
            | AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE
            | AccessibilityServiceInfo.FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY
            | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
            | AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;

    private static final int PASSWORD_MANAGER_CAPABILITY_TYPE_MASK =
            AccessibilityServiceInfo.CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT;

    // A bitmask containing the union of all event types, feedback types, flags,
    // and capabilities of running accessibility services.
    private static int sEventTypeMask;
    private static int sFeedbackTypeMask;
    private static int sFlagsMask;
    private static int sCapabilitiesMask;

    private static State sState;
    private static boolean sInitialized;
    private static boolean sIsInTestingMode;
    private static Boolean sPreInitCachedValuePerformGesturesEnabled;

    private static boolean sExtraStateInitialized;
    private static boolean sDisplayInversionEnabled;
    private static boolean sHighContrastEnabled;

    // Observers for various System, Activity, and Settings states relevant to accessibility.
    private static final ApplicationStatus.ActivityStateListener sActivityStateListener =
            AccessibilityState::onActivityStateChange;
    private static final ApplicationStatus.ApplicationStateListener sApplicationStateListener =
            AccessibilityState::onApplicationStateChange;
    private static ServicesObserver sAccessibilityServicesObserver;
    private static ServicesObserver sAnimationDurationScaleObserver;
    private static ServicesObserver sDisplayInversionEnabledObserver;
    private static ServicesObserver sTextContrastObserver;
    private static AccessibilityManager sAccessibilityManager;

    /**
     * Whether the user has enabled the Android-OS speak password when in accessibility mode,
     * available on pre-Android O. (Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD).
     *
     * From Android docs:
     * @deprecated The speaking of passwords is controlled by individual accessibility services.
     * Apps should ignore this setting and provide complete information to accessibility
     * at all times, which was the behavior when this value was {@code true}.
     */
    @Deprecated
    private static boolean sAccessibilitySpeakPasswordEnabled;

    /**
     * The current font weight adjustment set at the Android-OS level. Initialized to be 0, the
     * default font weight. If a user has the bold text setting enabled, this will be 300.
     *
     * This is not included as a part of the {State} object since it is only needed for the web
     * contents rendering (native widgets have font weight adjusted by the framework).
     *
     * This is only available on Android S+, on previous versions of Android this is always 300.
     */
    private static int sFontWeightAdjustment;

    // The IDs of all running accessibility services.
    private static String[] sServiceIds;

    // The set of listeners of AccessibilityState, implemented using
    // a WeakHashSet behind the scenes so that listeners can be garbage-collected
    // and will be automatically removed from this set.
    private static final Set<Listener> sListeners =
            Collections.newSetFromMap(new WeakHashMap<Listener, Boolean>());

    // The number of milliseconds to wait before checking the set of running accessibility services
    // again, when we think it changed. Uses an exponential back-off until it's greater than
    // MAX_DELAY_MILLIS. Note that each delay is additive, so the total time for a guaranteed signal
    // to listener is ~7.5 seconds.
    private static final int MIN_DELAY_MILLIS = 250;
    private static final int MAX_DELAY_MILLIS = 5000;
    private static int sNextDelayMillis = MIN_DELAY_MILLIS;

    public static void addListener(Listener listener) {
        sListeners.add(listener);
    }

    public static boolean isScreenReaderEnabled() {
        if (!sInitialized) updateAccessibilityServices();
        return sState.isScreenReaderEnabled;
    }

    /**
     * True when touch exploration is enabled. Since a client can call this after observers are
     * registered, but before the State has been queried for the first time, we allow for an early
     * return. This is a lighter weight query than the other State booleans, which require manual
     * calculation and heuristics. In this case we return the value directly from
     * AccessibilityManager.
     *
     * @return true if touch exploration is enabled.
     */
    public static boolean isTouchExplorationEnabled() {
        if (!sInitialized) {
            fetchAccessibilityManager();
            return sAccessibilityManager.isTouchExplorationEnabled();
        }
        return sState.isTouchExplorationEnabled;
    }

    public static boolean isPerformGesturesEnabled() {
        if (!sInitialized) {
            if (sPreInitCachedValuePerformGesturesEnabled != null) {
                return sPreInitCachedValuePerformGesturesEnabled;
            }

            fetchAccessibilityManager();
            if (sAccessibilityManager.isEnabled()) {
                for (AccessibilityServiceInfo service :
                        sAccessibilityManager.getEnabledAccessibilityServiceList(
                                AccessibilityServiceInfo.FEEDBACK_ALL_MASK)) {
                    if ((service.getCapabilities()
                                & AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES)
                            != 0) {
                        sPreInitCachedValuePerformGesturesEnabled = true;
                        return true;
                    }
                }
            }
            sPreInitCachedValuePerformGesturesEnabled = false;
            return false;
        }

        return sState.isPerformGesturesEnabled;
    }

    /**
     * True when at least one accessibility service is enabled on the system. Since a client can
     * call this after observers are registered, but before the State has been queried for the first
     * time, we allow for an early return. This is a lighter weight query than the other State
     * booleans, which require manual calculation and heuristics. In this case we return the value
     * directly from AccessibilityManager.
     *
     * @return true if any service is enabled.
     */
    public static boolean isAnyAccessibilityServiceEnabled() {
        if (!sInitialized) {
            fetchAccessibilityManager();
            return sAccessibilityManager.isEnabled();
        }
        return sState.isAnyAccessibilityServiceEnabled;
    }

    public static boolean isAccessibilityToolPresent() {
        if (!sInitialized) updateAccessibilityServices();
        return sState.isAccessibilityToolPresent;
    }

    public static boolean isSpokenFeedbackServicePresent() {
        if (!sInitialized) updateAccessibilityServices();
        return sState.isSpokenFeedbackServicePresent;
    }

    public static boolean isTextShowPasswordEnabled() {
        if (!sInitialized) updateAccessibilityServices();
        return sState.isTextShowPasswordEnabled;
    }

    public static boolean isOnlyPasswordManagersEnabled() {
        if (!sInitialized) updateAccessibilityServices();
        return sState.isOnlyPasswordManagersEnabled;
    }

    public static boolean isDisplayInversionEnabled() {
        if (!sExtraStateInitialized) updateExtraState();
        return sDisplayInversionEnabled;
    }

    public static boolean isHighContrastEnabled() {
        if (!sExtraStateInitialized) updateExtraState();
        return sHighContrastEnabled;
    }

    @Deprecated
    public static boolean isAccessibilitySpeakPasswordEnabled() {
        if (!sInitialized) updateAccessibilityServices();
        return sAccessibilitySpeakPasswordEnabled;
    }

    public static int getFontWeightAdjustment() {
        return sFontWeightAdjustment;
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

    /**
     * Convenience method to get a recommended timeout on all versions of Android. The method that
     * is part of AccessibilityManager is only available on Android >= Q. For earlier versions of
     * Android, we will multiply by an arbitrary constant.
     *
     * This method will query the AccessibilityManager, which considers the currently running
     * services, to provide a suggested timeout. On Android >= Q, the returned value may not be
     * either of the provided timeouts, and for versions < Q this will return the maximum of the
     * two timeouts.
     *
     * @param minimumTimeout - minimum allowed timeout for the calling feature.
     * @param nonA11yTimeout - the timeout if no a11y services are running for the feature.
     * @return Suggested timeout given the currently running services (in milliseconds).
     */
    public static int getRecommendedTimeoutMillis(int minimumTimeout, int nonA11yTimeout) {
        if (!sInitialized) updateAccessibilityServices();

        int recommendedTimeout = nonA11yTimeout;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            recommendedTimeout = sAccessibilityManager.getRecommendedTimeoutMillis(
                    nonA11yTimeout, FLAG_CONTENT_ICONS | FLAG_CONTENT_TEXT | FLAG_CONTENT_CONTROLS);
        } else {
            // For pre-Q Android versions, we will multiply by a constant when services are enabled.
            if (AccessibilityState.isAnyAccessibilityServiceEnabled()) {
                recommendedTimeout *= ANIMATION_TIMEOUT_MULTIPLIER;
            }
        }

        return Math.max(minimumTimeout, recommendedTimeout);
    }

    /**
     * Convenience method to send an AccessibilityEvent to the system's AccessibilityManager
     * without requiring a hard dependency on AccessibilityManager or an instance of a View. If
     * this method is called when accessibility has been disabled (e.g. stale state after calling
     * off the main thread), then the event will be ignored. If an event is sent, this does not
     * guarantee a correct user experience for downstream AT.
     *
     * Note: This should only be used in exceptional situations. Apps can generally achieve the
     *       correct behavior for accessibility with a semantically correct UI. Deprecated to
     *       prompt dev to reconsider their approach.
     *
     * @param event AccessibilityEvent to send to the AccessibilityManager
     */
    @Deprecated
    public static void sendAccessibilityEvent(AccessibilityEvent event) {
        if (!sInitialized) updateAccessibilityServices();

        if (sAccessibilityManager.isEnabled()) {
            sAccessibilityManager.sendAccessibilityEvent(event);
        }
    }

    private static void fetchAccessibilityManager() {
        if (sAccessibilityManager != null) return;

        // This instance is valid for the entire lifecycle of the app.
        sAccessibilityManager =
                (AccessibilityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACCESSIBILITY_SERVICE);
    }

    static void updateExtraState() {
        sExtraStateInitialized = true;
        Context context = ContextUtils.getApplicationContext();
        int displayInversionEnabledSetting = Settings.Secure.getInt(context.getContentResolver(),
                Settings.Secure.ACCESSIBILITY_DISPLAY_INVERSION_ENABLED, 0);
        boolean isDisplayInversionEnabled = displayInversionEnabledSetting == 1;
        sDisplayInversionEnabled = isDisplayInversionEnabled;

        int highTextContrastEnabled = Settings.Secure.getInt(context.getContentResolver(),
                /*Settings.Secure.ACCESSIBILITY_HIGH_TEXT_CONTRAST_ENABLED*/
                "high_text_contrast_enabled", 0);
        sHighContrastEnabled = highTextContrastEnabled == 1;
    }

    static void updateAccessibilityServices() {
        long now = SystemClock.elapsedRealtimeNanos() / 1000;
        if (!sInitialized) {
            sState = new State(false, false, false, false, false, false, false, false);
            fetchAccessibilityManager();
        }
        sInitialized = true;
        sEventTypeMask = 0;
        sFeedbackTypeMask = 0;
        sFlagsMask = 0;
        sCapabilitiesMask = 0;

        // Used solely as part of the heuristic to identify whether screen readers are running.
        // This mask is kept separate from the above masks as those should be the source of
        // truth.
        int screenReaderCheckEventTypeMask = 0;

        // Used solely as part of the heuristic to identify whether password managers are running.
        // These masks are kept separate from the above masks as those should be the source of
        // truth.
        int passwordCheckEventTypeMask = 0;
        int passwordCheckFeedbackTypeMask = 0;
        int passwordCheckFlagsMask = 0;
        int passwordCheckCapabilitiesMask = 0;

        boolean isAnyAccessibilityServiceEnabled = false;
        boolean isAccessibilityToolPresent = false;

        // Get the list of currently running accessibility services.
        List<AccessibilityServiceInfo> services =
                sAccessibilityManager.getEnabledAccessibilityServiceList(
                        AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
        sServiceIds = new String[services.size()];
        ArrayList<String> runningServiceNames = new ArrayList<String>();
        int i = 0;
        for (AccessibilityServiceInfo service : services) {
            if (service == null) continue;
            isAccessibilityToolPresent |= (Build.VERSION.SDK_INT < Build.VERSION_CODES.S
                    || service.isAccessibilityTool());
            isAnyAccessibilityServiceEnabled = true;

            String serviceId = service.getId();
            sServiceIds[i++] = serviceId;

            sEventTypeMask |= service.eventTypes;
            sFeedbackTypeMask |= service.feedbackType;
            sFlagsMask |= service.flags;
            sCapabilitiesMask |= service.getCapabilities();

            // Only check the event, feedback, flag, and capability types for the password manager
            // heuristic if the running service is not the AutofillCompatAccessibilityService. The
            // AutofillCompatAccessibilityService requests all events like a screenreader but
            // does not serve assistive technology. It only serves autofill applications. The
            // AutofillCompatAccessibilityService event mask would prevent the form controls
            // heuristic from identifying the presence of other assistive technologies, so skip
            // the mask for this service.
            if (!serviceId.equals(AUTOFILL_COMPAT_ACCESSIBILITY_SERVICE_ID)) {
                screenReaderCheckEventTypeMask |= service.eventTypes;
                passwordCheckEventTypeMask |= service.eventTypes;
                passwordCheckFeedbackTypeMask |= service.feedbackType;
                passwordCheckFlagsMask |= service.flags;
                passwordCheckCapabilitiesMask |= service.getCapabilities();
            }

            // Try to canonicalize the component name.
            ComponentName componentName = ComponentName.unflattenFromString(serviceId);
            if (componentName != null) {
                runningServiceNames.add(componentName.flattenToShortString());
            } else {
                runningServiceNames.add(serviceId);
            }
        }

        Context context = ContextUtils.getApplicationContext();

        // Update the font weight adjustment (e.g. bold text setting).
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            sFontWeightAdjustment = context.getResources().getConfiguration().fontWeightAdjustment;
        } else {
            sFontWeightAdjustment = 0;
        }

        // Update the user password show/speak preferences.
        int textShowPasswordSetting = Settings.System.getInt(
                context.getContentResolver(), Settings.System.TEXT_SHOW_PASSWORD, 1);
        boolean isTextShowPasswordEnabled = textShowPasswordSetting == 1;

        int accessibilitySpeakPasswordSetting = Settings.Secure.getInt(
                context.getContentResolver(), Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD, 0);
        sAccessibilitySpeakPasswordEnabled = accessibilitySpeakPasswordSetting == 1;

        // Get the list of enabled accessibility services, from settings, in
        // case it's different.
        ArrayList<String> enabledServiceNames = new ArrayList<String>();
        String serviceNamesString = Settings.Secure.getString(
                context.getContentResolver(), Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (serviceNamesString != null && !serviceNamesString.isEmpty()) {
            String[] serviceNames = serviceNamesString.split(":");
            for (String name : serviceNames) {
                // null or empty names can be skipped
                if (name == null || name.isEmpty()) continue;
                // Try to canonicalize the component name if possible.
                ComponentName componentName = ComponentName.unflattenFromString(name);
                if (componentName != null) {
                    enabledServiceNames.add(componentName.flattenToShortString());
                } else {
                    enabledServiceNames.add(name);
                }
            }
        }

        // Compare the list of enabled package names to the list of running package names.
        // When the system setting containing the list of running accessibility services
        // changes, it isn't always reflected in getEnabledAccessibilityServiceList
        // immediately. To ensure we always have an up-to-date value, check that the
        // set of services match, and if they don't, schedule an update with an exponential
        // back-off.
        Collections.sort(runningServiceNames);
        Collections.sort(enabledServiceNames);
        if (runningServiceNames.equals(enabledServiceNames)) {
            Log.i(TAG, "Enabled accessibility services list updated.");
            sNextDelayMillis = MIN_DELAY_MILLIS;
        } else {
            Log.i(TAG, "Enabled accessibility services: " + enabledServiceNames.toString());
            Log.i(TAG, "Running accessibility services: " + runningServiceNames.toString());

            // Do not inform listeners until the services agree, unless the limit set by
            // {MAX_DELAY_MILLIS} has been reached, in which case send whatever we have.
            if (sNextDelayMillis < MAX_DELAY_MILLIS) {
                Log.i(TAG, "Will check again after " + sNextDelayMillis + " milliseconds.");
                RecordHistogram.recordBooleanHistogram(
                        UPDATE_ACCESSIBILITY_SERVICES_DID_POLL, true);
                ThreadUtils.getUiThreadHandler().postDelayed(
                        AccessibilityState::updateAccessibilityServices, sNextDelayMillis);
                sPollCount++;
                sNextDelayMillis *= 2;
                return;
            } else {
                Log.i(TAG, "Max delay reached. Send information as is.");
                RecordHistogram.recordBooleanHistogram(
                        UPDATE_ACCESSIBILITY_SERVICES_POLL_TIMEOUT, true);

                // Reset if we have reached {MAX_DELAY_MILLIS} so we do not miss later discrepancies
                // between the services.
                sNextDelayMillis = MIN_DELAY_MILLIS;
            }
        }

        // If there are some events, flags, and capabilities enabled
        // and if there are, at most, the expected set of password manager event, flags, and
        // capabilities enabled, then the system is probably running only password managers
        boolean areOnlyPasswordManagerMasksRequestedByServices =
                (passwordCheckEventTypeMask != 0 && passwordCheckFlagsMask != 0
                        && passwordCheckCapabilitiesMask != 0)
                && ((passwordCheckEventTypeMask | PASSWORD_MANAGER_EVENT_TYPE_MASK)
                        == PASSWORD_MANAGER_EVENT_TYPE_MASK)
                && ((passwordCheckFlagsMask | PASSWORD_MANAGER_FLAG_TYPE_MASK)
                        == PASSWORD_MANAGER_FLAG_TYPE_MASK)
                && ((passwordCheckCapabilitiesMask | PASSWORD_MANAGER_CAPABILITY_TYPE_MASK)
                        == PASSWORD_MANAGER_CAPABILITY_TYPE_MASK)
                && ((passwordCheckFeedbackTypeMask | AccessibilityServiceInfo.FEEDBACK_GENERIC)
                        == AccessibilityServiceInfo.FEEDBACK_GENERIC);

        boolean isOnlyAutofillRunning = false;

        // Only explicitly check for Autofill on compatible versions
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            AutofillManager autofillManager = context.getSystemService(AutofillManager.class);

            if (autofillManager != null && autofillManager.isEnabled()
                    && autofillManager.hasEnabledAutofillServices()) {
                // Confirm that autofill service is the only service running that requires
                // accessibility.
                if (runningServiceNames.isEmpty()
                        || (runningServiceNames.size() == 1
                                && runningServiceNames.get(0).equals(
                                        AUTOFILL_COMPAT_ACCESSIBILITY_SERVICE_ID))) {
                    isOnlyAutofillRunning = true;
                }
            }
        }

        boolean isOnlyPasswordManagersEnabled = false;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // If build is >= S, then check if there are no accessibility tools present, then turn
            // on form controls mode if the heuristic indicates that only password managers are
            // enabled or Autofill is the only service running.
            isOnlyPasswordManagersEnabled = !isAccessibilityToolPresent
                    && (areOnlyPasswordManagerMasksRequestedByServices || isOnlyAutofillRunning);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // If build is >= O and < S, isAccessibilityToolPresent will always be true.
            // Turn on form controls mode if the heuristic indicates that only password managers are
            // enabled or Autofill is the only service running.
            isOnlyPasswordManagersEnabled =
                    areOnlyPasswordManagerMasksRequestedByServices || isOnlyAutofillRunning;
        } else {
            // If the build is < O, isAccessibilityToolPresent will always be true and
            // isOnlyAutofillRunning will always be false. Turn on form controls mode if the
            // heuristic indicates that only password managers are enabled.
            isOnlyPasswordManagersEnabled = areOnlyPasswordManagerMasksRequestedByServices;
        }

        // Update all listeners that there was a state change and pass whether or not the
        // new state includes a screen reader.
        Log.i(TAG, "Informing listeners of changes.");
        boolean isScreenReaderEnabled =
                (0 != (screenReaderCheckEventTypeMask & SCREEN_READER_EVENT_TYPE_MASK));
        boolean isSpokenFeedbackServicePresent = (0 != (sFeedbackTypeMask & FEEDBACK_SPOKEN));
        boolean isTouchExplorationEnabled = sAccessibilityManager.isTouchExplorationEnabled();
        boolean isPerformGesturesEnabled =
                (0 != (sCapabilitiesMask & CAPABILITY_CAN_PERFORM_GESTURES));

        // Record time of this method call, and number of times polling was required.
        RecordHistogram.recordLinearCountHistogram(
                UPDATE_ACCESSIBILITY_SERVICES_RUNTIME,
                (int) ((SystemClock.elapsedRealtimeNanos() / 1000) - now),
                1,
                MAX_RUNTIME_BUCKET,
                100);
        RecordHistogram.recordLinearCountHistogram(
                UPDATE_ACCESSIBILITY_SERVICES_POLL_COUNT, sPollCount, 1, 10, 11);
        sPollCount = 0;

        updateAndNotifyStateChange(new State(isScreenReaderEnabled, isTouchExplorationEnabled,
                isPerformGesturesEnabled, isAnyAccessibilityServiceEnabled,
                isAccessibilityToolPresent, isSpokenFeedbackServicePresent,
                isTextShowPasswordEnabled, isOnlyPasswordManagersEnabled));
    }

    private static void updateAndNotifyStateChange(State newState) {
        State oldState = sState;
        sState = newState;

        Log.i(TAG, "New AccessibilityState: " + sState.toString());
        for (Listener listener : sListeners) {
            listener.onAccessibilityStateChanged(oldState, newState);
        }
    }

    /**
     * Return a bitmask containing the union of all event types that running accessibility
     * services listen to.
     * @return
     */
    // TODO(mschillaci,jacklynch): Make this private and update current callers.
    @CalledByNative
    public static int getAccessibilityServiceEventTypeMask() {
        if (!sInitialized) updateAccessibilityServices();
        return sEventTypeMask;
    }

    /**
     * Return a bitmask containing the union of all feedback types that running accessibility
     * services provide.
     * @return
     */
    @CalledByNative
    private static int getAccessibilityServiceFeedbackTypeMask() {
        if (!sInitialized) updateAccessibilityServices();
        return sFeedbackTypeMask;
    }

    /**
     * Return a bitmask containing the union of all flags from running accessibility services.
     * @return
     */
    @CalledByNative
    private static int getAccessibilityServiceFlagsMask() {
        if (!sInitialized) updateAccessibilityServices();
        return sFlagsMask;
    }

    /**
     * Return a bitmask containing the union of all service capabilities from running
     * accessibility services.
     * @return
     */
    @CalledByNative
    private static int getAccessibilityServiceCapabilitiesMask() {
        if (!sInitialized) updateAccessibilityServices();
        return sCapabilitiesMask;
    }

    /**
     * Return a list of ids of all running accessibility services.
     * @return
     */
    @CalledByNative
    private static String[] getAccessibilityServiceIds() {
        if (!sInitialized) updateAccessibilityServices();
        return sServiceIds;
    }

    /**
     * Register observers of various system properties and initialize a state for clients.
     *
     * Note: This should only be called once, and before any client queries of accessibility state.
     *       The first time any client queries the state, |this| will be initialized.
     */
    public static void registerObservers() {
        assert !sInitialized
                || sIsInTestingMode
            : "AccessibilityState has been called to register observers, but observers have "
              + "already been registered, or, a client has already queried the state. Observers "
              + "should only be registered once during browser init and before any client queries.";

        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        sAnimationDurationScaleObserver = new ServicesObserver(ThreadUtils.getUiThreadHandler(),
                () -> AccessibilityStateJni.get().onAnimatorDurationScaleChanged());
        sAccessibilityServicesObserver = new ServicesObserver(
                ThreadUtils.getUiThreadHandler(), AccessibilityState::processServicesChange);
        sDisplayInversionEnabledObserver = new ServicesObserver(
                ThreadUtils.getUiThreadHandler(), AccessibilityState::processExtraStateChange);
        sTextContrastObserver = new ServicesObserver(
                ThreadUtils.getUiThreadHandler(), AccessibilityState::processExtraStateChange);

        // We want to be notified whenever the user has updated the animator duration scale.
        contentResolver.registerContentObserver(
                Settings.Global.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE), false,
                sAnimationDurationScaleObserver);

        // We want to be notified whenever the currently enabled services changes.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES), false,
                sAccessibilityServicesObserver);
        contentResolver.registerContentObserver(
                Settings.System.getUriFor(Settings.Secure.TOUCH_EXPLORATION_ENABLED), false,
                sAccessibilityServicesObserver);

        // We want to be notified if the user changes their preferred password show/speak settings.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD), false,
                sAccessibilityServicesObserver);
        contentResolver.registerContentObserver(
                Settings.System.getUriFor(Settings.System.TEXT_SHOW_PASSWORD), false,
                sAccessibilityServicesObserver);

        // We want to be notified if the user changes their display inversion settings.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.ACCESSIBILITY_DISPLAY_INVERSION_ENABLED),
                false, sDisplayInversionEnabledObserver);

        // We want to be notified if the user changes their contrast settings.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(
                        /*Settings.Secure.ACCESSIBILITY_HIGH_TEXT_CONTRAST_ENABLED*/
                        "high_text_contrast_enabled"),
                false, sTextContrastObserver);
    }

    public static void initializeOnStartup() {
        // This method is called as a deferred task during browser init. If no services are enabled,
        // this will ensure the state is populated for any client queries later. If a service is
        // enabled during startup, the current state may be queried before this method is called,
        // in which case another update is not needed.
        if (!sInitialized) {
            updateAccessibilityServices();
        }
        if (!sExtraStateInitialized) {
            updateExtraState();
        }

        // We want to be notified whenever an Activity or Application state changes.
        ApplicationStatus.registerStateListenerForAllActivities(sActivityStateListener);
        ApplicationStatus.registerApplicationStateListener(sApplicationStateListener);

        // Histograms are recorded once during startup, and any time services change afterwards.
        AccessibilityStateJni.get().recordAccessibilityServiceInfoHistograms();
    }

    private static void onActivityStateChange(Activity activity, int newState) {
        // If Chrome is sent to the background, we will unregister observers, and re-register the
        // observers and query state when Chrome is brought back to the foreground.
        if (newState == ActivityState.RESUMED) {
            processServicesChange();
            processExtraStateChange();
        }
    }

    private static void onApplicationStateChange(int newState) {
        // If Chrome is sent to the background, we will unregister observers, and re-register the
        // observers when Chrome is brought back to the foreground.
        if (newState != ApplicationState.HAS_RUNNING_ACTIVITIES
                && newState != ApplicationState.HAS_PAUSED_ACTIVITIES) {
            unregisterObservers();
        } else if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES && !sInitialized) {
            registerObservers();
        }
    }

    private static void unregisterObservers() {
        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        contentResolver.unregisterContentObserver(sAccessibilityServicesObserver);
        contentResolver.unregisterContentObserver(sAnimationDurationScaleObserver);
        contentResolver.unregisterContentObserver(sDisplayInversionEnabledObserver);
        contentResolver.unregisterContentObserver(sTextContrastObserver);
        sState = null;
        sPreInitCachedValuePerformGesturesEnabled = null;
        sInitialized = false;
        sExtraStateInitialized = false;
        sDisplayInversionEnabled = false;
        sHighContrastEnabled = false;
        sAccessibilityManager = null;
    }

    private static void processServicesChange() {
        updateAccessibilityServices();
        AccessibilityStateJni.get().recordAccessibilityServiceInfoHistograms();
    }

    private static void processExtraStateChange() {
        updateExtraState();
        AccessibilityStateJni.get().onDisplayInversionEnabledChanged(isDisplayInversionEnabled());
        AccessibilityStateJni.get().onContrastLevelChanged(isHighContrastEnabled());
    }

    private static class ServicesObserver extends ContentObserver {
        private final Runnable mRunnable;

        public ServicesObserver(Handler handler, Runnable runnable) {
            super(handler);
            mRunnable = runnable;
        }

        @Override
        public void onChange(boolean selfChange) {
            onChange(selfChange, null);
        }

        @Override
        public void onChange(boolean selfChange, @Nullable Uri uri) {
            ThreadUtils.getUiThreadHandler().post(mRunnable);
        }
    }

    @NativeMethods
    interface Natives {
        void onAnimatorDurationScaleChanged();
        void onDisplayInversionEnabledChanged(boolean enabled);
        void onContrastLevelChanged(boolean highContrastEnabled);
        void recordAccessibilityServiceInfoHistograms();
    }

    // ForTesting methods.

    public static void setIsScreenReaderEnabledForTesting(boolean enabled) {
        if (!sInitialized) initializeForTesting();

        State newState = new State(
            enabled,
            sState.isTouchExplorationEnabled,
            sState.isPerformGesturesEnabled,
            sState.isAnyAccessibilityServiceEnabled,
            sState.isAccessibilityToolPresent,
            sState.isSpokenFeedbackServicePresent,
            sState.isTextShowPasswordEnabled,
            sState.isOnlyPasswordManagersEnabled);

        updateAndNotifyStateChange(newState);
    }

    public static void setIsTouchExplorationEnabledForTesting(boolean enabled) {
        if (!sInitialized) initializeForTesting();

        State newState = new State(
            sState.isScreenReaderEnabled,
            enabled,
            sState.isPerformGesturesEnabled,
            sState.isAnyAccessibilityServiceEnabled,
            sState.isAccessibilityToolPresent,
            sState.isSpokenFeedbackServicePresent,
            sState.isTextShowPasswordEnabled,
            sState.isOnlyPasswordManagersEnabled);

        updateAndNotifyStateChange(newState);
    }

    public static void setIsPerformGesturesEnabledForTesting(boolean enabled) {
        if (!sInitialized) initializeForTesting();

        State newState = new State(
            sState.isScreenReaderEnabled,
            sState.isTouchExplorationEnabled,
            enabled,
            sState.isAnyAccessibilityServiceEnabled,
            sState.isAccessibilityToolPresent,
            sState.isSpokenFeedbackServicePresent,
            sState.isTextShowPasswordEnabled,
            sState.isOnlyPasswordManagersEnabled);

        updateAndNotifyStateChange(newState);
    }

    public static void setIsAnyAccessibilityServiceEnabledForTesting(boolean enabled) {
        if (!sInitialized) initializeForTesting();

        State newState = new State(
            sState.isScreenReaderEnabled,
            sState.isTouchExplorationEnabled,
            sState.isPerformGesturesEnabled,
            enabled,
            sState.isAccessibilityToolPresent,
            sState.isSpokenFeedbackServicePresent,
            sState.isTextShowPasswordEnabled,
            sState.isOnlyPasswordManagersEnabled);

        updateAndNotifyStateChange(newState);
    }

    public static void setIsAccessibilityToolPresentForTesting(boolean enabled) {
        if (!sInitialized) initializeForTesting();

        State newState = new State(
            sState.isScreenReaderEnabled,
            sState.isTouchExplorationEnabled,
            sState.isPerformGesturesEnabled,
            sState.isAnyAccessibilityServiceEnabled,
            enabled,
            sState.isSpokenFeedbackServicePresent,
            sState.isTextShowPasswordEnabled,
            sState.isOnlyPasswordManagersEnabled);

        updateAndNotifyStateChange(newState);
    }

    public static void setIsSpokenFeedbackServicePresentForTesting(boolean enabled) {
        if (!sInitialized) initializeForTesting();

        State newState = new State(
            sState.isScreenReaderEnabled,
            sState.isTouchExplorationEnabled,
            sState.isPerformGesturesEnabled,
            sState.isAnyAccessibilityServiceEnabled,
            sState.isAccessibilityToolPresent,
            enabled,
            sState.isTextShowPasswordEnabled,
            sState.isOnlyPasswordManagersEnabled);

        updateAndNotifyStateChange(newState);
    }

    public static void setIsTextShowPasswordEnabledForTesting(boolean enabled) {
        if (!sInitialized) initializeForTesting();

        State newState = new State(
            sState.isScreenReaderEnabled,
            sState.isTouchExplorationEnabled,
            sState.isPerformGesturesEnabled,
            sState.isAnyAccessibilityServiceEnabled,
            sState.isAccessibilityToolPresent,
            sState.isSpokenFeedbackServicePresent,
            enabled,
            sState.isOnlyPasswordManagersEnabled);

        updateAndNotifyStateChange(newState);
    }

    public static void setIsOnlyPasswordManagersEnabledForTesting(boolean enabled) {
        if (!sInitialized) initializeForTesting();

        State newState = new State(
            sState.isScreenReaderEnabled,
            sState.isTouchExplorationEnabled,
            sState.isPerformGesturesEnabled,
            sState.isAnyAccessibilityServiceEnabled,
            sState.isAccessibilityToolPresent,
            sState.isSpokenFeedbackServicePresent,
            sState.isTextShowPasswordEnabled,
            enabled);

        updateAndNotifyStateChange(newState);
    }

    public static void setEventTypeMaskForTesting(int mask) {
        if (!sInitialized) updateAccessibilityServices();

        // Explicitly set mask so events can be (ir)relevant to currently enabled service.
        sEventTypeMask = mask;
    }

    private static void initializeForTesting() {
        sState = new State(false, false, false, false, false, false, false, false);
        fetchAccessibilityManager();
        sInitialized = true;
        sIsInTestingMode = true;
    }

}
