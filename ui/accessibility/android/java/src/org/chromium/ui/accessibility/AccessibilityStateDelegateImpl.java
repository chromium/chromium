// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import static android.accessibilityservice.AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES;
import static android.accessibilityservice.AccessibilityServiceInfo.CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION;
import static android.accessibilityservice.AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_CONTROLS;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_ICONS;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_TEXT;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.accessibility.AccessibilityState.AUTOFILL_COMPAT_ACCESSIBILITY_SERVICE_ID;
import static org.chromium.ui.accessibility.AccessibilityState.KNOWN_SCREEN_READER_SERVICE_IDS;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Activity;
import android.app.UiModeManager;
import android.app.UiModeManager.ContrastChangeListener;
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

import androidx.annotation.RequiresApi;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.accessibility.AccessibilityState.Listener;
import org.chromium.ui.accessibility.AccessibilityState.State;
import org.chromium.ui.accessibility.AccessibilityState.StateBuilderForTests;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Set;
import java.util.WeakHashMap;

/**
 * Provides utility methods relating to measuring accessibility state on Android. See native
 * counterpart in accessibility::AccessibilityState.
 */
@NullMarked
class AccessibilityStateDelegateImpl {
    private static final String TAG = "A11yState";

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
    private int mPollCount;

    // The service ID and whether the `isAccessibilityTool=true` manifest flag is explicitly
    // set for a given service. Before Android S, `isAccessibilityTool` is always false.
    private static class ServiceProperties {
        public final String id;
        public final boolean isAccessibilityTool;

        public ServiceProperties(String id, boolean isAccessibilityTool) {
            this.id = id;
            this.isAccessibilityTool = isAccessibilityTool;
        }
    }

    // Analysis of the most popular accessibility services on Android suggests that any service that
    // requests any of these three events is an accessibility service that has a more complex user
    // interaction than something like password managers, but not as much as screen readers. This
    // heuristic can be used to identify states where some, but not all, accessibility
    // considerations of clients are required.
    private static final int COMPLEX_USER_INTERACTION_SERVICE_EVENT_TYPE_MASK =
            AccessibilityEvent.TYPE_VIEW_SELECTED
                    | AccessibilityEvent.TYPE_VIEW_SCROLLED
                    | AccessibilityEvent.TYPE_ANNOUNCEMENT;

    // Analysis of the most popular password managers on Android suggests
    // that services that only request these events, flags, and capabilities is likely a password
    // manager. If not more than these events are requested, we can enable some optimizations.
    protected static final int PASSWORD_MANAGER_EVENT_TYPE_MASK =
            AccessibilityEvent.TYPE_VIEW_CLICKED
                    | AccessibilityEvent.TYPE_VIEW_FOCUSED
                    | AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED
                    | AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED
                    | AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED;

    protected static final int PASSWORD_MANAGER_FLAG_TYPE_MASK =
            AccessibilityServiceInfo.DEFAULT
                    | AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
                    | AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE
                    | AccessibilityServiceInfo.FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY
                    | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
                    | AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;

    protected static final int PASSWORD_MANAGER_CAPABILITY_TYPE_MASK =
            AccessibilityServiceInfo.CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT;

    // A bitmask containing the union of all event types, feedback types, flags,
    // and capabilities of running accessibility services.
    private int mEventTypeMask;
    private int mFeedbackTypeMask;
    private int mFlagsMask;
    private int mCapabilitiesMask;

    // A bitmask containing the union of all event types, feedback types, flags, and
    // capabilities of running accessibility services, with heuristics applied. These masks are
    // kept separate from the ones above, as those should be the source of truth.
    private int mEventTypeMaskHeuristic;
    private int mFeedbackTypeMaskHeuristic;
    private int mFlagsMaskHeuristic;
    private int mCapabilitiesMaskHeuristic;

    private @Nullable State mState;

    private boolean mInitialized;
    private boolean mHasRegisteredObservers;
    private boolean mIsInTestingMode;
    private @Nullable Boolean mPreInitCachedValuePerformGesturesEnabled;

    // A flag indicating whether the "extra state" values `mDisplayInversionEnabled`,
    // `mHighContrastEnabled`, `mTextCursorBlinkInterval`, and `mAnimatorDurationScale` have been
    // read yet from the system settings into these variables.
    private boolean mExtraStateInitialized;
    private boolean mDisplayInversionEnabled;
    private boolean mHighContrastEnabled;
    private int mFontWeightAdjustment;
    private int mTextCursorBlinkInterval =
            AconfigFlaggedApiDelegate.DEFAULT_TEXT_CURSOR_BLINK_INTERVAL_MS;
    private float mAnimatorDurationScale;

    // Observers for various System, Activity, and Settings states relevant to accessibility.
    private final ApplicationStatus.ActivityStateListener mActivityStateListener =
            this::onActivityStateChange;
    private final ApplicationStatus.ApplicationStateListener mApplicationStateListener =
            this::onApplicationStateChange;
    private @Nullable ServicesObserver mAccessibilityServicesObserver;
    private @Nullable ServicesObserver mAnimationDurationScaleObserver;
    private @Nullable ServicesObserver mDisplayInversionEnabledObserver;
    private @Nullable ServicesObserver mCursorBlinkRateObserver;
    private @Nullable ServicesObserver mTextContrastObserver;

    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    private @Nullable ContrastChangeListener mContrastChangeListener;

    private @Nullable AccessibilityManager mAccessibilityManager;

    // The IDs and `isAccessibilityTool` manifest state of all running accessibility services.
    private @Nullable List<ServiceProperties> mServiceProperties;

    // The set of listeners of AccessibilityState, implemented using
    // a WeakHashSet behind the scenes so that listeners can be garbage-collected
    // and will be automatically removed from this set.
    private final Set<Listener> mListeners =
            Collections.newSetFromMap(new WeakHashMap<Listener, Boolean>());

    // The number of milliseconds to wait before checking the set of running accessibility services
    // again, when we think it changed. Uses an exponential back-off until it's greater than
    // MAX_DELAY_MILLIS. Note that each delay is additive, so the total time for a guaranteed signal
    // to listener is ~7.5 seconds.
    private static final int MIN_DELAY_MILLIS = 250;
    private static final int MAX_DELAY_MILLIS = 5000;
    private int mNextDelayMillis = MIN_DELAY_MILLIS;

    public void addListener(Listener listener) {
        mListeners.add(listener);
    }

    public boolean isComplexUserInteractionServiceEnabled() {
        if (!mInitialized) updateAccessibilityServices();
        return assumeNonNull(mState).isComplexUserInteractionServiceEnabled;
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
    public boolean isTouchExplorationEnabled() {
        if (!mInitialized) {
            return fetchAccessibilityManager().isTouchExplorationEnabled();
        }
        return assumeNonNull(mState).isTouchExplorationEnabled;
    }

    /**
     * True when perform gestures is enabled. Since a client can call this after observers are
     * registered, but before the State has been queried for the first time, we allow for an early
     * return. This is a lighter weight query than the other State booleans, which require manual
     * calculation and heuristics. In this case we return the value directly from
     * AccessibilityManager.
     *
     * @return true if perform gestures is enabled.
     */
    public boolean isPerformGesturesEnabled() {
        if (!mInitialized) {
            if (mPreInitCachedValuePerformGesturesEnabled != null) {
                return mPreInitCachedValuePerformGesturesEnabled;
            }

            AccessibilityManager accessibilityManager = fetchAccessibilityManager();
            if (accessibilityManager.isEnabled()) {
                for (AccessibilityServiceInfo service :
                        accessibilityManager.getEnabledAccessibilityServiceList(
                                AccessibilityServiceInfo.FEEDBACK_ALL_MASK)) {
                    if ((service.getCapabilities()
                                    & AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES)
                            != 0) {
                        mPreInitCachedValuePerformGesturesEnabled = true;
                        return true;
                    }
                }
            }
            mPreInitCachedValuePerformGesturesEnabled = false;
            return false;
        }

        return assumeNonNull(mState).isPerformGesturesEnabled;
    }

    /**
     * True when at least one accessibility service is enabled on the system. Since a client can
     * call this after observers are registered, but before the State has been queried for the first
     * time, we allow for an early return. This is a lighter weight query than the other State
     * booleans, which require manual calculation and heuristics. In this case we return the value
     * directly from AccessibilityManager.
     *
     * @return true if any service is enabled (includes pseudo-accessibility services).
     */
    public boolean isAnyAccessibilityServiceEnabled() {
        if (!mInitialized) {
            return fetchAccessibilityManager().isEnabled();
        }
        return assumeNonNull(mState).isAnyAccessibilityServiceEnabled;
    }

    /**
     * Returns the value of AccessibilityManager.isEnabled(). This indicates whether the
     * accessibility manager is currently enabled.
     *
     * @return true if the accessibility manager is enabled.
     */
    public boolean isAccessibilityManagerEnabled() {
        return fetchAccessibilityManager().isEnabled();
    }

    public boolean isAccessibilityToolPresent() {
        if (!mInitialized) updateAccessibilityServices();
        return assumeNonNull(mState).isAccessibilityToolPresent;
    }

    public boolean isTextShowPasswordEnabled() {
        if (!mInitialized) updateAccessibilityServices();
        return assumeNonNull(mState).isTextShowPasswordEnabled;
    }

    public boolean isOnlyAutofillRunning() {
        if (!mInitialized) updateAccessibilityServices();
        return assumeNonNull(mState).isOnlyAutofillRunning;
    }

    public boolean isOnlyPasswordManagersEnabled() {
        if (!mInitialized) updateAccessibilityServices();
        return assumeNonNull(mState).isOnlyPasswordManagersEnabled;
    }

    public boolean isKnownScreenReaderEnabled() {
        if (!mInitialized) updateAccessibilityServices();
        return assumeNonNull(mState).isKnownScreenReaderEnabled;
    }

    public boolean isDisplayInversionEnabled() {
        if (!mExtraStateInitialized) updateExtraState();
        return mDisplayInversionEnabled;
    }

    public boolean isHighContrastEnabled() {
        if (!mExtraStateInitialized) updateExtraState();
        return mHighContrastEnabled;
    }

    public int getNumberOfRunningServices() {
        if (!mInitialized) updateAccessibilityServices();
        return assumeNonNull(mServiceProperties).size();
    }

    /**
     * The current font weight adjustment set at the Android-OS level. Initialized to be 0, the
     * default font weight. If a user has the bold text setting enabled, this will be 300. This is
     * not included as a part of the {State} object since it is only needed for the web contents
     * rendering (native widgets have font weight adjusted by the framework). This is only available
     * on Android S+, on previous versions of Android this is always 0.
     */
    public int getFontWeightAdjustment() {
        return mFontWeightAdjustment;
    }

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
    public int getRecommendedTimeoutMillis(int minimumTimeout, int nonA11yTimeout) {
        if (!mInitialized) updateAccessibilityServices();

        int recommendedTimeout =
                fetchAccessibilityManager()
                        .getRecommendedTimeoutMillis(
                                nonA11yTimeout,
                                FLAG_CONTENT_ICONS | FLAG_CONTENT_TEXT | FLAG_CONTENT_CONTROLS);

        return Math.max(minimumTimeout, recommendedTimeout);
    }

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
    public void sendAccessibilityEvent(AccessibilityEvent event) {
        if (!mInitialized) updateAccessibilityServices();

        AccessibilityManager accessibilityManager = fetchAccessibilityManager();
        if (accessibilityManager.isEnabled()) {
            accessibilityManager.sendAccessibilityEvent(event);
        }
    }

    /** Returns the current ANIMATOR_DURATION_SCALE from the users OS accessibility settings. */
    public float getAnimatorDurationScale() {
        if (!mExtraStateInitialized) updateExtraState();
        return mAnimatorDurationScale;
    }

    /** Returns the current TEXT_CURSOR_BLINK_INTERVAL from the users OS accessibility settings. */
    public int getTextCursorBlinkInterval() {
        if (!mExtraStateInitialized) updateExtraState();
        return mTextCursorBlinkInterval;
    }

    private AccessibilityManager fetchAccessibilityManager() {
        AccessibilityManager ret = mAccessibilityManager;
        if (ret == null) {
            // This instance is valid for the entire lifecycle of the app.
            ret =
                    (AccessibilityManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.ACCESSIBILITY_SERVICE);
            mAccessibilityManager = ret;
        }
        return ret;
    }

    void updateExtraState() {
        mExtraStateInitialized = true;
        Context context = ContextUtils.getApplicationContext();
        int displayInversionEnabledSetting =
                Settings.Secure.getInt(
                        context.getContentResolver(),
                        Settings.Secure.ACCESSIBILITY_DISPLAY_INVERSION_ENABLED,
                        0);
        mDisplayInversionEnabled = displayInversionEnabledSetting == 1;

        mAnimatorDurationScale =
                Settings.Global.getFloat(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE,
                        1f);

        AconfigFlaggedApiDelegate aconfigFlaggedApiDelegate =
                AconfigFlaggedApiDelegate.getInstance();
        if (aconfigFlaggedApiDelegate != null) {
            mTextCursorBlinkInterval = aconfigFlaggedApiDelegate.getTextCursorBlinkInterval();
        } else {
            mTextCursorBlinkInterval =
                    AconfigFlaggedApiDelegate.DEFAULT_TEXT_CURSOR_BLINK_INTERVAL_MS;
        }

        int highTextContrastEnabled =
                Settings.Secure.getInt(
                        context.getContentResolver(),
                        /*Settings.Secure.ACCESSIBILITY_HIGH_TEXT_CONTRAST_ENABLED*/
                        "high_text_contrast_enabled",
                        0);
        float contrastLevel = 0f;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            UiModeManager uiModeManager =
                    (UiModeManager) context.getSystemService(Context.UI_MODE_SERVICE);
            // This value can be between -1 and 1, but in practice the UI
            // exposes 0 (default), 0.5 (medium), or 1 (high).
            contrastLevel = uiModeManager.getContrast();
        }
        // If high text contrast is enabled or the colour contrast level is high,
        // then high contrast is enabled.
        mHighContrastEnabled = highTextContrastEnabled == 1 || contrastLevel == 1f;
    }

    protected List<AccessibilityServiceInfo> getRunningServiceInfoList() {
        return fetchAccessibilityManager()
                .getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
    }

    protected String getEnabledServiceString(Context context) {
        return Settings.Secure.getString(
                context.getContentResolver(), Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
    }

    protected static List<String> getCanonicalizedEnabledServiceNames(String enabledServiceString) {
        ArrayList<String> enabledServiceNames = new ArrayList<>();
        if (enabledServiceString != null && !enabledServiceString.isEmpty()) {
            String[] serviceNames = enabledServiceString.split(":");
            for (String name : serviceNames) {
                addCanonicalizedComponentNameToArray(enabledServiceNames, name);
            }
        }
        return enabledServiceNames;
    }

    protected static void addCanonicalizedComponentNameToArray(List<String> array, String name) {
        assert array != null;

        // null or empty names can be skipped
        if (name == null || name.isEmpty()) return;

        // Try to canonicalize the component name if possible.
        ComponentName componentName = ComponentName.unflattenFromString(name);
        if (componentName != null) {
            array.add(componentName.flattenToShortString());
        } else {
            array.add(name);
        }
    }

    protected void calculateHeuristicState(AccessibilityServiceInfo service) {
        // Only check the event, feedback, flag, and capability types for the password manager
        // heuristic if the running service is not the AutofillCompatAccessibilityService. The
        // AutofillCompatAccessibilityService requests all events like a screenreader but
        // does not serve assistive technology. It only serves autofill applications. The
        // AutofillCompatAccessibilityService event mask would prevent the form controls
        // heuristic from identifying the presence of other assistive technologies, so skip
        // the mask for this service.
        if (!service.getId().equals(AUTOFILL_COMPAT_ACCESSIBILITY_SERVICE_ID)) {
            mEventTypeMaskHeuristic |= service.eventTypes;
            mFeedbackTypeMaskHeuristic |= service.feedbackType;
            mFlagsMaskHeuristic |= service.flags;
            mCapabilitiesMaskHeuristic |= service.getCapabilities();
        }
    }

    protected boolean areOnlyPasswordManagerMasksRequested() {
        // If there are some events, flags, and capabilities enabled and if there are, at most, the
        // expected set of password manager event, flags, and capabilities enabled, then the system
        // is probably running only password managers
        return (mEventTypeMaskHeuristic != 0
                        && mFlagsMaskHeuristic != 0
                        && mCapabilitiesMaskHeuristic != 0)
                && ((mEventTypeMaskHeuristic | PASSWORD_MANAGER_EVENT_TYPE_MASK)
                        == PASSWORD_MANAGER_EVENT_TYPE_MASK)
                && ((mFlagsMaskHeuristic | PASSWORD_MANAGER_FLAG_TYPE_MASK)
                        == PASSWORD_MANAGER_FLAG_TYPE_MASK)
                && ((mCapabilitiesMaskHeuristic | PASSWORD_MANAGER_CAPABILITY_TYPE_MASK)
                        == PASSWORD_MANAGER_CAPABILITY_TYPE_MASK)
                && ((mFeedbackTypeMaskHeuristic | AccessibilityServiceInfo.FEEDBACK_GENERIC)
                        == AccessibilityServiceInfo.FEEDBACK_GENERIC);
    }

    protected void updateAccessibilityServices() {
        updateAccessibilityServices(/* recordHistograms= */ false);
    }

    private void updateAccessibilityServices(boolean recordHistograms) {
        long now = SystemClock.elapsedRealtimeNanos() / 1000;
        if (!mInitialized) {
            mState = new State(false, false, false, false, false, false, false, false, false);
            fetchAccessibilityManager();
        }
        mInitialized = true;

        // Reset previous state calculations.
        mEventTypeMask = 0;
        mFeedbackTypeMask = 0;
        mFlagsMask = 0;
        mCapabilitiesMask = 0;

        // Reset previous heuristic state calculations.
        mEventTypeMaskHeuristic = 0;
        mFeedbackTypeMaskHeuristic = 0;
        mFlagsMaskHeuristic = 0;
        mCapabilitiesMaskHeuristic = 0;

        boolean isAnyAccessibilityServiceEnabled = false;
        boolean isAccessibilityToolPresent = false;

        // Get the list of currently running accessibility services.
        List<AccessibilityServiceInfo> serviceInfoList = getRunningServiceInfoList();
        mServiceProperties = new ArrayList<>();
        List<String> runningServiceNames = new ArrayList<>();
        for (AccessibilityServiceInfo service : serviceInfoList) {
            if (service == null) continue;
            isAnyAccessibilityServiceEnabled = true;

            String serviceId = service.getId();
            addCanonicalizedComponentNameToArray(runningServiceNames, serviceId);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                // Check if the service is an accessibility tool based on the manifest flag.
                if (service.isAccessibilityTool()) {
                    isAccessibilityToolPresent = true;
                    mServiceProperties.add(new ServiceProperties(serviceId, true));
                } else {
                    mServiceProperties.add(new ServiceProperties(serviceId, false));
                }
            } else {
                // Before Android S, assume all accessibility services are accessibility tools,
                // but none explicitly flag themselves as such without the manifest flag.
                isAccessibilityToolPresent = true;
                mServiceProperties.add(new ServiceProperties(serviceId, false));
            }

            mEventTypeMask |= service.eventTypes;
            mFeedbackTypeMask |= service.feedbackType;
            mFlagsMask |= service.flags;
            mCapabilitiesMask |= service.getCapabilities();

            calculateHeuristicState(service);
        }

        Context context = ContextUtils.getApplicationContext();

        // Update the font weight adjustment (e.g. bold text setting).
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            mFontWeightAdjustment = context.getResources().getConfiguration().fontWeightAdjustment;
        } else {
            mFontWeightAdjustment = 0;
        }

        // Update the user show password preferences.
        int textShowPasswordSetting =
                Settings.System.getInt(
                        context.getContentResolver(), Settings.System.TEXT_SHOW_PASSWORD, 1);
        boolean isTextShowPasswordEnabled = textShowPasswordSetting == 1;

        // Get the list of enabled accessibility services, from settings, in
        // case it's different.
        List<String> enabledServiceNames =
                getCanonicalizedEnabledServiceNames(getEnabledServiceString(context));

        // Compare the list of enabled package names to the list of running package names.
        // When the system setting containing the list of running accessibility services
        // changes, it isn't always reflected in getEnabledAccessibilityServiceList
        // immediately. To ensure we always have an up-to-date value, check that the
        // set of services match, and if they don't, schedule an update with an exponential
        // back-off.
        runningServiceNames.sort(Comparator.naturalOrder());
        enabledServiceNames.sort(Comparator.naturalOrder());

        // In some cases, Autofill will be running but will not be listed as an enabled service,
        // such as when some third-party password managers are running. In these cases, we will
        // have a mismatch between these lists until the max timeout. So try comparing the lists
        // while ignoring autofill, and if they match, then we can continue.
        List<String> prunedRunningServiceNames = new ArrayList<>();
        for (String service : runningServiceNames) {
            if (!service.equals(AUTOFILL_COMPAT_ACCESSIBILITY_SERVICE_ID)) {
                prunedRunningServiceNames.add(service);
            }
        }

        if (runningServiceNames.equals(enabledServiceNames)
                || prunedRunningServiceNames.equals(enabledServiceNames)) {
            Log.i(
                    TAG,
                    "Enabled accessibility services list updated. "
                            + enabledServiceNames.toString());
            mNextDelayMillis = MIN_DELAY_MILLIS;
        } else {
            Log.i(TAG, "Enabled accessibility services: " + enabledServiceNames.toString());
            Log.i(TAG, "Running accessibility services: " + runningServiceNames.toString());

            // Do not inform listeners until the services agree, unless the limit set by
            // {MAX_DELAY_MILLIS} has been reached, in which case send whatever we have.
            if (mNextDelayMillis < MAX_DELAY_MILLIS) {
                Log.i(TAG, "Will check again after " + mNextDelayMillis + " milliseconds.");
                RecordHistogram.recordBooleanHistogram(
                        UPDATE_ACCESSIBILITY_SERVICES_DID_POLL, true);
                ThreadUtils.getUiThreadHandler()
                        .postDelayed(
                                recordHistograms
                                        ? this::processServicesChange
                                        : this::updateAccessibilityServices,
                                mNextDelayMillis);
                recordHistograms = false; // Leave histograms to delayed call.
                mPollCount++;
                mNextDelayMillis *= 2;
                return;
            } else {
                Log.i(TAG, "Max delay reached. Send information as is.");
                RecordHistogram.recordBooleanHistogram(
                        UPDATE_ACCESSIBILITY_SERVICES_POLL_TIMEOUT, true);

                // Reset if we have reached {MAX_DELAY_MILLIS} so we do not miss later discrepancies
                // between the services.
                mNextDelayMillis = MIN_DELAY_MILLIS;
            }
        }

        // Calculate heuristic state value derivations.
        boolean isComplexUserInteractionServiceEnabled =
                (0 != (mEventTypeMaskHeuristic & COMPLEX_USER_INTERACTION_SERVICE_EVENT_TYPE_MASK));
        boolean isKnownScreenReaderEnabled = false;
        for (ServiceProperties service : mServiceProperties) {
            if (KNOWN_SCREEN_READER_SERVICE_IDS.equals(service.id)) {
                isKnownScreenReaderEnabled = true;
                break;
            }
        }

        boolean isOnlyAutofillRunning = false;
        try {
            AutofillManager autofillManager = context.getSystemService(AutofillManager.class);
            if (autofillManager != null
                    && autofillManager.isEnabled()
                    && autofillManager.hasEnabledAutofillServices()) {
                // Confirm that autofill service is the only service running that requires
                // accessibility.
                if (runningServiceNames.isEmpty()
                        || (runningServiceNames.size() == 1
                                && runningServiceNames
                                        .get(0)
                                        .equals(AUTOFILL_COMPAT_ACCESSIBILITY_SERVICE_ID))) {
                    isOnlyAutofillRunning = true;
                }
            }
        } catch (RuntimeException e) {
            Log.e(TAG, "AutofillManager did not resolve before timelimit.");
        }

        boolean isOnlyPasswordManagersEnabled;
        boolean areOnlyPasswordManagerMasksRequestedByServices =
                areOnlyPasswordManagerMasksRequested();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // If build is >= S, then check if there are no accessibility tools present, then turn
            // on form controls mode if the heuristic indicates that only password managers are
            // enabled or Autofill is the only service running.
            isOnlyPasswordManagersEnabled =
                    !isAccessibilityToolPresent
                            && (areOnlyPasswordManagerMasksRequestedByServices
                                    || isOnlyAutofillRunning);
        } else {
            // If build is < S, isAccessibilityToolPresent will always be true.
            // Turn on form controls mode if the heuristic indicates that only password managers are
            // enabled or Autofill is the only service running.
            isOnlyPasswordManagersEnabled =
                    areOnlyPasswordManagerMasksRequestedByServices || isOnlyAutofillRunning;
        }

        // Calculate traditional state values.
        boolean isTouchExplorationEnabled =
                (0 != (mCapabilitiesMask & CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION))
                        && (0 != (mFlagsMask & FLAG_REQUEST_TOUCH_EXPLORATION_MODE));

        boolean isPerformGesturesEnabled =
                (0 != (mCapabilitiesMask & CAPABILITY_CAN_PERFORM_GESTURES));

        // Record time of this method call, and number of times polling was required.
        RecordHistogram.recordLinearCountHistogram(
                UPDATE_ACCESSIBILITY_SERVICES_RUNTIME,
                (int) ((SystemClock.elapsedRealtimeNanos() / 1000) - now),
                1,
                MAX_RUNTIME_BUCKET,
                100);
        RecordHistogram.recordLinearCountHistogram(
                UPDATE_ACCESSIBILITY_SERVICES_POLL_COUNT, mPollCount, 1, 10, 11);
        mPollCount = 0;

        // Update all listeners that there was a state change and pass whether or not the
        // new state includes a screen reader.
        Log.i(TAG, "Informing listeners of changes.");
        updateAndNotifyStateChange(
                new State(
                        isComplexUserInteractionServiceEnabled,
                        isTouchExplorationEnabled,
                        isPerformGesturesEnabled,
                        isAnyAccessibilityServiceEnabled,
                        isAccessibilityToolPresent,
                        isTextShowPasswordEnabled,
                        isOnlyAutofillRunning,
                        isOnlyPasswordManagersEnabled,
                        isKnownScreenReaderEnabled));
        if (recordHistograms) {
            AccessibilityStateJni.get().recordAccessibilityServiceInfoHistograms();
        }
    }

    private void updateAndNotifyStateChange(State newState) {
        assert mState != null;
        State oldState = mState;
        mState = newState;

        Log.i(TAG, "New AccessibilityState: " + mState.toString());
        for (Listener listener : mListeners) {
            listener.onAccessibilityStateChanged(oldState, newState);
        }
    }

    /**
     * Return a bitmask containing the union of all event types that running accessibility services
     * listen to.
     */
    int getAccessibilityServiceEventTypeMask() {
        if (!mInitialized) updateAccessibilityServices();
        return mEventTypeMask;
    }

    /**
     * Return a bitmask containing the union of all feedback types that running accessibility
     * services provide.
     */
    int getAccessibilityServiceFeedbackTypeMask() {
        if (!mInitialized) updateAccessibilityServices();
        return mFeedbackTypeMask;
    }

    /** Return a bitmask containing the union of all flags from running accessibility services. */
    int getAccessibilityServiceFlagsMask() {
        if (!mInitialized) updateAccessibilityServices();
        return mFlagsMask;
    }

    /**
     * Return a bitmask containing the union of all service capabilities from running accessibility
     * services.
     */
    int getAccessibilityServiceCapabilitiesMask() {
        if (!mInitialized) updateAccessibilityServices();
        return mCapabilitiesMask;
    }

    /** Return a list of ids of all running accessibility services. */
    String[] getAccessibilityServiceIds() {
        if (!mInitialized) updateAccessibilityServices();
        assert mServiceProperties != null;

        String[] ids = new String[mServiceProperties.size()];
        for (int i = 0; i < ids.length; i++) {
            ids[i] = mServiceProperties.get(i).id;
        }
        return ids;
    }

    /**
     * Return a list of whether running accessibility services have {@code isAccessibilityTool=true}
     * declared in their manifest. Note that {@code isAccessibilityTool} was introduced in Android
     * S; on earlier Android versions this will return all {@code false}. The returned array will
     * have the same length as the array returned by {@link #getAccessibilityServiceIds()}.
     */
    boolean[] getAccessibilityToolFlags() {
        if (!mInitialized) updateAccessibilityServices();
        assert mServiceProperties != null;

        boolean[] flags = new boolean[mServiceProperties.size()];
        for (int i = 0; i < flags.length; i++) {
            flags[i] = mServiceProperties.get(i).isAccessibilityTool;
        }
        return flags;
    }

    /**
     * Register observers of various system properties and initialize a state for clients.
     *
     * <p>Note: This should only be called once, and before any client queries of accessibility
     * state. The first time any client queries the state, |this| will be initialized.
     */
    public void registerObservers() {
        assert !mInitialized || !mHasRegisteredObservers || mIsInTestingMode
                : "AccessibilityState has been called to register observers, but observers have"
                        + " already been registered, or, a client has already queried the state."
                        + " Observers should only be registered once during browser init and before"
                        + " any client queries.";

        ContentResolver contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        mAnimationDurationScaleObserver =
                new ServicesObserver(
                        ThreadUtils.getUiThreadHandler(), this::processExtraStateChange);
        mAccessibilityServicesObserver =
                new ServicesObserver(ThreadUtils.getUiThreadHandler(), this::processServicesChange);
        mDisplayInversionEnabledObserver =
                new ServicesObserver(
                        ThreadUtils.getUiThreadHandler(), this::processExtraStateChange);
        mCursorBlinkRateObserver =
                new ServicesObserver(
                        ThreadUtils.getUiThreadHandler(), this::processExtraStateChange);
        mTextContrastObserver =
                new ServicesObserver(
                        ThreadUtils.getUiThreadHandler(), this::processExtraStateChange);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            mContrastChangeListener = (contrast) -> processExtraStateChange();
        }

        // We want to be notified whenever the user has updated the animator duration scale.
        contentResolver.registerContentObserver(
                Settings.Global.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE),
                false,
                mAnimationDurationScaleObserver);

        // We want to be notified whenever the currently enabled services changes.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES),
                false,
                mAccessibilityServicesObserver);
        contentResolver.registerContentObserver(
                Settings.System.getUriFor(Settings.Secure.TOUCH_EXPLORATION_ENABLED),
                false,
                mAccessibilityServicesObserver);

        // We want to be notified if the user changes their preferred password show/speak settings.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD),
                false,
                mAccessibilityServicesObserver);
        contentResolver.registerContentObserver(
                Settings.System.getUriFor(Settings.System.TEXT_SHOW_PASSWORD),
                false,
                mAccessibilityServicesObserver);

        // We want to be notified if the user changes their display inversion settings.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.ACCESSIBILITY_DISPLAY_INVERSION_ENABLED),
                false,
                mDisplayInversionEnabledObserver);

        // We want to be notified if the user changes their cursor blink settings.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(
                        /* Settings.Secure.ACCESSIBILITY_TEXT_CURSOR_BLINK_INTERVAL_MS */
                        "accessibility_text_cursor_blink_interval_ms"),
                false,
                mCursorBlinkRateObserver);

        // We want to be notified if the user changes their text contrast settings.
        contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(
                        /*Settings.Secure.ACCESSIBILITY_HIGH_TEXT_CONTRAST_ENABLED*/
                        "high_text_contrast_enabled"),
                false,
                mTextContrastObserver);

        // We want to be notified if the user changes their colour contrast settings.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            Context context = ContextUtils.getApplicationContext();
            UiModeManager uiModeManager =
                    (UiModeManager) context.getSystemService(Context.UI_MODE_SERVICE);
            if (uiModeManager != null && mContrastChangeListener != null) {
                uiModeManager.addContrastChangeListener(
                        context.getMainExecutor(), mContrastChangeListener);
            }
        }

        mHasRegisteredObservers = true;
    }

    public void initializeOnStartup() {
        // This method is called as a deferred task during browser init. If no services are enabled,
        // this will ensure the state is populated for any client queries later. If a service is
        // enabled during startup, the current state may be queried before this method is called,
        // in which case another state update is not needed. In either case, the state should be
        // propagated to all listeners once during browser init.
        if (!mInitialized) {
            updateAccessibilityServices();
        }
        if (!mExtraStateInitialized) {
            updateExtraState();
        }
        notifyExtraStateListeners();

        // We want to be notified whenever an Activity or Application state changes.
        ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
        ApplicationStatus.registerApplicationStateListener(mApplicationStateListener);

        // Histograms are recorded once during startup, and any time services change afterwards.
        AccessibilityStateJni.get().recordAccessibilityServiceInfoHistograms();
    }

    private void onActivityStateChange(Activity activity, int newState) {
        // If Chrome is sent to the background, we will unregister observers, and re-register the
        // observers and query state when Chrome is brought back to the foreground.
        if (newState == ActivityState.RESUMED) {
            processServicesChange();
            processExtraStateChange();
        }
    }

    private void onApplicationStateChange(int newState) {
        // If Chrome is sent to the background, we will unregister observers, and re-register the
        // observers when Chrome is brought back to the foreground.
        if (newState != ApplicationState.HAS_RUNNING_ACTIVITIES
                && newState != ApplicationState.HAS_PAUSED_ACTIVITIES) {
            unregisterObservers();
        } else if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES
                && (!mInitialized || !mHasRegisteredObservers)) {
            registerObservers();
        }
    }

    private void unregisterObservers() {
        if (!mHasRegisteredObservers) {
            return;
        }

        assert mAccessibilityServicesObserver != null;
        assert mAnimationDurationScaleObserver != null;
        assert mDisplayInversionEnabledObserver != null;
        assert mCursorBlinkRateObserver != null;
        assert mTextContrastObserver != null;
        Context context = ContextUtils.getApplicationContext();
        ContentResolver contentResolver = context.getContentResolver();
        contentResolver.unregisterContentObserver(mAccessibilityServicesObserver);
        contentResolver.unregisterContentObserver(mAnimationDurationScaleObserver);
        contentResolver.unregisterContentObserver(mDisplayInversionEnabledObserver);
        contentResolver.unregisterContentObserver(mCursorBlinkRateObserver);
        contentResolver.unregisterContentObserver(mTextContrastObserver);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            UiModeManager uiModeManager =
                    (UiModeManager) context.getSystemService(Context.UI_MODE_SERVICE);
            if (uiModeManager != null && mContrastChangeListener != null) {
                uiModeManager.removeContrastChangeListener(mContrastChangeListener);
            }
            mContrastChangeListener = null;
        }
        mState = null;
        mPreInitCachedValuePerformGesturesEnabled = null;
        mInitialized = false;
        mHasRegisteredObservers = false;
        mExtraStateInitialized = false;
        mDisplayInversionEnabled = false;
        mHighContrastEnabled = false;
        mAnimatorDurationScale = 1f;
        mAccessibilityManager = null;
        mTextCursorBlinkInterval = AconfigFlaggedApiDelegate.DEFAULT_TEXT_CURSOR_BLINK_INTERVAL_MS;
    }

    private void processServicesChange() {
        updateAccessibilityServices(/* recordHistograms= */ true);
    }

    private void processExtraStateChange() {
        updateExtraState();
        notifyExtraStateListeners();
    }

    /** Inform native listeners of changes to the extra state. */
    private void notifyExtraStateListeners() {
        AccessibilityStateJni.get().onAnimatorDurationScaleChanged();
        AccessibilityStateJni.get().onDisplayInversionEnabledChanged(isDisplayInversionEnabled());
        AccessibilityStateJni.get().onContrastLevelChanged(isHighContrastEnabled());
        AccessibilityStateJni.get().onTextCursorBlinkIntervalChanged(getTextCursorBlinkInterval());
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

    // ForTesting methods.

    public void setIsComplexUserInteractionServiceEnabledForTesting(boolean enabled) {
        if (!mInitialized) initializeForTesting();
        State oldState = assumeNonNull(mState);
        State newState =
                new StateBuilderForTests(oldState)
                        .setIsComplexUserInteractionServiceEnabled(enabled)
                        .build();
        updateAndNotifyStateChange(newState);
    }

    public void setIsTouchExplorationEnabledForTesting(boolean enabled) {
        if (!mInitialized) initializeForTesting();
        State oldState = assumeNonNull(mState);
        State newState =
                new StateBuilderForTests(oldState).setIsTouchExplorationEnabled(enabled).build();
        updateAndNotifyStateChange(newState);
    }

    public void setIsPerformGesturesEnabledForTesting(boolean enabled) {
        if (!mInitialized) initializeForTesting();
        State oldState = assumeNonNull(mState);
        State newState =
                new StateBuilderForTests(oldState).setIsPerformGesturesEnabled(enabled).build();
        updateAndNotifyStateChange(newState);
    }

    public void setIsAnyAccessibilityServiceEnabledForTesting(boolean enabled) {
        if (!mInitialized) initializeForTesting();
        State oldState = assumeNonNull(mState);
        State newState =
                new StateBuilderForTests(oldState)
                        .setIsAnyAccessibilityServiceEnabled(enabled)
                        .build();
        updateAndNotifyStateChange(newState);
    }

    public void setIsAccessibilityToolPresentForTesting(boolean enabled) {
        if (!mInitialized) initializeForTesting();
        State oldState = assumeNonNull(mState);
        State newState =
                new StateBuilderForTests(oldState).setIsAccessibilityToolPresent(enabled).build();
        updateAndNotifyStateChange(newState);
    }

    public void setIsTextShowPasswordEnabledForTesting(boolean enabled) {
        if (!mInitialized) initializeForTesting();
        State oldState = assumeNonNull(mState);
        State newState =
                new StateBuilderForTests(oldState).setIsTextShowPasswordEnabled(enabled).build();
        updateAndNotifyStateChange(newState);
    }

    public void setIsOnlyAutofillRunningForTesting(boolean enabled) {
        if (!mInitialized) initializeForTesting();
        State oldState = assumeNonNull(mState);
        State newState =
                new StateBuilderForTests(oldState).setIsOnlyAutofillRunning(enabled).build();
        updateAndNotifyStateChange(newState);
    }

    public void setIsOnlyPasswordManagersEnabledForTesting(boolean enabled) {
        if (!mInitialized) initializeForTesting();
        State oldState = assumeNonNull(mState);
        State newState =
                new StateBuilderForTests(oldState)
                        .setIsOnlyPasswordManagersEnabled(enabled)
                        .build();
        updateAndNotifyStateChange(newState);
    }

    public void setIsKnownScreenReaderEnabledForTesting(boolean enabled) {
        if (!mInitialized) initializeForTesting();
        State oldState = assumeNonNull(mState);
        State newState =
                new StateBuilderForTests(oldState).setIsKnownScreenReaderEnabled(enabled).build();
        updateAndNotifyStateChange(newState);
    }

    public void setEventMaskForTesting(int eventMask) {
        if (!mInitialized) initializeForTesting();

        mEventTypeMask = eventMask;
    }

    public void setServiceIdsForTesting(String newServiceId, boolean isAccessibilityTool) {
        if (!mInitialized) initializeForTesting();

        mServiceProperties = new ArrayList<>();
        mServiceProperties.add(new ServiceProperties(newServiceId, isAccessibilityTool));
    }

    private void initializeForTesting() {
        mState = new State(false, false, false, false, false, false, false, false, false);
        mServiceProperties = new ArrayList<>();
        fetchAccessibilityManager();
        mInitialized = true;
        mIsInTestingMode = true;
    }

    protected void uninitializeForTesting() {
        unregisterObservers();
        ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
        ApplicationStatus.unregisterApplicationStateListener(mApplicationStateListener);
        mState = null;
        mServiceProperties = null;
        mAccessibilityManager = null;
        mInitialized = false;
        mIsInTestingMode = false;
        mPreInitCachedValuePerformGesturesEnabled = null;
    }
}
