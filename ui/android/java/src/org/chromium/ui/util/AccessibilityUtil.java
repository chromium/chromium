// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.os.Build.VERSION_CODES;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityManager.TouchExplorationStateChangeListener;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;

import java.util.List;

/**
 * Exposes information about the current accessibility state.
 */
public class AccessibilityUtil {
    /**
     * An observer to be notified of accessibility status changes.
     */
    @Deprecated
    public interface Observer {
        /**
         * @param enabled Whether touch exploration or an accessibility service that can perform
         *        gestures is enabled. Indicates that the UI must be fully navigable using
         *        the accessibility view tree.
         */
        void onAccessibilityModeChanged(boolean enabled);
    }

    // Cached value of isAccessibilityEnabled(). If null, indicates the value needs to be
    // recalculated.
    private Boolean mIsAccessibilityEnabled;
    private Boolean mIsTouchExplorationEnabled;
    private ObserverList<Observer> mObservers;
    private final class ModeChangeHandler
            implements AccessibilityStateChangeListener, TouchExplorationStateChangeListener {
        // AccessibilityStateChangeListener

        @Override
        public final void onAccessibilityStateChanged(boolean enabled) {
            updateIsAccessibilityEnabledAndNotify();
        }

        // TouchExplorationStateChangeListener

        @Override
        public void onTouchExplorationStateChanged(boolean enabled) {
            updateIsAccessibilityEnabledAndNotify();
        }
    }

    private ModeChangeHandler mModeChangeHandler;

    protected AccessibilityUtil() {}

    /**
     * Checks to see that touch exploration or an accessibility service that can perform gestures
     * is enabled.
     * @return        Whether or not accessibility and touch exploration are enabled.
     */
    public boolean isAccessibilityEnabled() {
        if (mModeChangeHandler == null) registerModeChangeListeners();
        if (mIsAccessibilityEnabled != null) return mIsAccessibilityEnabled;

        TraceEvent.begin("AccessibilityManager::isAccessibilityEnabled");

        AccessibilityManager manager = getAccessibilityManager();
        boolean accessibilityEnabled =
                manager != null && manager.isEnabled() && manager.isTouchExplorationEnabled();
        mIsTouchExplorationEnabled = accessibilityEnabled;

        if (manager != null && manager.isEnabled() && !accessibilityEnabled) {
            List<AccessibilityServiceInfo> services = manager.getEnabledAccessibilityServiceList(
                    AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
            for (AccessibilityServiceInfo service : services) {
                if (canPerformGestures(service)) {
                    accessibilityEnabled = true;
                    break;
                }
            }
        }

        mIsAccessibilityEnabled = accessibilityEnabled;

        TraceEvent.end("AccessibilityManager::isAccessibilityEnabled");
        return mIsAccessibilityEnabled;
    }

    /**
     * Checks to see that touch exploration is enabled. Does not include accessibility services that
     * perform gestures (e.g. switchaccess returns false here)
     * @return        Whether or not accessibility and touch exploration are enabled.
     */
    public boolean isTouchExplorationEnabled() {
        if (mModeChangeHandler == null) registerModeChangeListeners();
        if (mIsTouchExplorationEnabled != null) return mIsTouchExplorationEnabled;

        TraceEvent.begin("AccessibilityManager::isTouchExplorationEnabled");

        AccessibilityManager manager = getAccessibilityManager();
        mIsTouchExplorationEnabled =
                manager != null && manager.isEnabled() && manager.isTouchExplorationEnabled();

        TraceEvent.end("AccessibilityManager::isTouchExplorationEnabled");
        return mIsTouchExplorationEnabled;
    }

    /**
     * Get the recommended timeout for changes to the UI needed by this user. The timeout value
     * can be set by users on Q+.
     *
     * https://d.android.com/reference/android/view/accessibility/AccessibilityManager#getRecommendedTimeoutMillis(int,%20int)
     * @param originalTimeout The timeout appropriate for users with no accessibility needs.
     * @param uiContentFlags The combination of content flags to indicate contents of UI.
     * @return The recommended UI timeout for the current user in milliseconds.
     */
    @RequiresApi(api = VERSION_CODES.Q)
    public int getRecommendedTimeoutMillis(int originalTimeout, int uiContentFlags) {
        AccessibilityManager manager = getAccessibilityManager();
        assert manager != null : "AccessibilityManager is not available";
        return manager.getRecommendedTimeoutMillis(originalTimeout, uiContentFlags);
    }

    /**
     * Add {@link Observer} object. The observer will be notified of the current accessibility
     * mode immediately.
     * @param observer Observer object monitoring a11y mode change.
     */
    public void addObserver(Observer observer) {
        getObservers().addObserver(observer);

        // Notify mode change to a new observer so things are initialized correctly when Chrome
        // has been re-started after closing due to the last tab being closed when homepage is
        // enabled. See crbug.com/541546.
        observer.onAccessibilityModeChanged(isAccessibilityEnabled());
    }

    /**
     * Remove {@link Observer} object.
     * @param observer Observer object monitoring a11y mode change.
     */
    public void removeObserver(Observer observer) {
        getObservers().removeObserver(observer);
    }

    private AccessibilityManager getAccessibilityManager() {
        return (AccessibilityManager) ContextUtils.getApplicationContext().getSystemService(
                Context.ACCESSIBILITY_SERVICE);
    }

    private void registerModeChangeListeners() {
        assert mModeChangeHandler == null;
        mModeChangeHandler = new ModeChangeHandler();
        AccessibilityManager manager = getAccessibilityManager();
        manager.addAccessibilityStateChangeListener(mModeChangeHandler);
        manager.addTouchExplorationStateChangeListener(mModeChangeHandler);
    }

    /**
     * Removes all global state tracking observers/listeners as well as any observers added to this.
     * As this removes all observers, be very careful in calling. In general, only call when the
     * application is going to be destroyed.
     */
    protected void stopTrackingStateAndRemoveObservers() {
        if (mObservers != null) mObservers.clear();
        if (mModeChangeHandler == null) return;
        AccessibilityManager manager = getAccessibilityManager();
        manager.removeAccessibilityStateChangeListener(mModeChangeHandler);
        manager.removeTouchExplorationStateChangeListener(mModeChangeHandler);
        mModeChangeHandler = null;
    }

    /**
     * Forces recalculating the value of isAccessibilityEnabled(). If the value has changed
     * observers are notified.
     */
    protected void updateIsAccessibilityEnabledAndNotify() {
        boolean oldIsAccessibilityEnabled = isAccessibilityEnabled();
        // Setting to null forces the next call to isAccessibilityEnabled() to update the value.
        mIsAccessibilityEnabled = null;
        mIsTouchExplorationEnabled = null;
        if (oldIsAccessibilityEnabled != isAccessibilityEnabled()) notifyModeChange();
    }

    private ObserverList<Observer> getObservers() {
        if (mObservers == null) mObservers = new ObserverList<>();
        return mObservers;
    }

    /**
     * Notify all the observers of the mode change.
     */
    private void notifyModeChange() {
        boolean enabled = isAccessibilityEnabled();
        for (Observer observer : getObservers()) {
            observer.onAccessibilityModeChanged(enabled);
        }
    }

    /**
     * Checks whether the given {@link AccessibilityServiceInfo} can perform gestures.
     * @param service The service to check.
     * @return Whether the {@code service} can perform gestures. This relies on the capabilities
     *         the service can perform.
     */
    private boolean canPerformGestures(AccessibilityServiceInfo service) {
        return (service.getCapabilities()
                       & AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES)
                != 0;
    }

    /**
     * Set whether the device has accessibility enabled. Should be reset back to null after the test
     * has finished.
     * @param isEnabled whether the device has accessibility enabled.
     */
    public void setAccessibilityEnabledForTesting(@Nullable Boolean isEnabled) {
        ThreadUtils.assertOnUiThread();
        mIsAccessibilityEnabled = isEnabled;
        notifyModeChange();
    }

    /**
     * Set whether the device has touch exploration enabled. Should be reset back to null after the
     * test has finished.
     * @param isEnabled whether the device has touch exploration enabled.
     */
    public void setTouchExplorationEnabledForTesting(@Nullable Boolean isEnabled) {
        ThreadUtils.assertOnUiThread();
        mIsTouchExplorationEnabled = isEnabled;
        notifyModeChange();
    }
}
