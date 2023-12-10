// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.accessibility.AccessibilityState.State;

/** Exposes information about the current accessibility state. */
public class AccessibilityUtil implements AccessibilityState.Listener {
    /** An observer to be notified of accessibility status changes. */
    @Deprecated
    public interface Observer {
        /**
         * @param enabled Whether touch exploration or an accessibility service that can perform
         *        gestures is enabled. Indicates that the UI must be fully navigable using
         *        the accessibility view tree.
         */
        void onAccessibilityModeChanged(boolean enabled);
    }

    private ObserverList<Observer> mObservers;

    protected AccessibilityUtil() {}

    /**
     * Add {@link Observer} object. The observer will be notified of the current accessibility
     * mode immediately.
     * @param observer Observer object monitoring a11y mode change.
     */
    @Deprecated
    public void addObserver(Observer observer) {
        getObservers().addObserver(observer);

        // Notify mode change to a new observer so things are initialized correctly when Chrome
        // has been re-started after closing due to the last tab being closed when homepage is
        // enabled. See crbug.com/541546.
        observer.onAccessibilityModeChanged(AccessibilityState.isAccessibilityEnabled());
    }

    /**
     * Remove {@link Observer} object.
     * @param observer Observer object monitoring a11y mode change.
     */
    @Deprecated
    public void removeObserver(Observer observer) {
        getObservers().removeObserver(observer);
    }

    @Override
    public void onAccessibilityStateChanged(
            State oldAccessibilityState, State newAccessibilityState) {
        notifyModeChange(AccessibilityState.isAccessibilityEnabled());
    }

    private ObserverList<Observer> getObservers() {
        if (mObservers == null) mObservers = new ObserverList<>();
        return mObservers;
    }

    /** Notify all the observers of the mode change. */
    private void notifyModeChange(boolean isAccessibilityEnabled) {
        for (Observer observer : getObservers()) {
            observer.onAccessibilityModeChanged(isAccessibilityEnabled);
        }
    }

    /**
     * Set whether the device has accessibility enabled. Should be reset back to null after the test
     * has finished.
     * @param isEnabled whether the device has accessibility enabled.
     */
    public void setAccessibilityEnabledForTesting(@Nullable Boolean isEnabled) {
        ThreadUtils.assertOnUiThread();
        notifyModeChange(Boolean.TRUE.equals(isEnabled));
    }
}
