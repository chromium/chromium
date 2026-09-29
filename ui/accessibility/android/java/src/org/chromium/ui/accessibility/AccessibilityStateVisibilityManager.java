// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface for managing accessibility state visibility. */
@NullMarked
public interface AccessibilityStateVisibilityManager {
    /** Observer for visibility changes. */
    interface Observer {
        /**
         * Called when either the application is foregrounded or an Activity belonging to the
         * application is foregrounded.
         *
         * <p>Calling the method when an Activity has been foregrounded but Chromium is already in
         * the foreground enables updating accessibility state when Chromium is running in
         * multi-window mode and an additional Chromium window is brought to the foreground after
         * modifying accessibility settings in the settings app.
         *
         * <p>This method initiates recomputing accessibility settings. {@link
         * onActivityOrApplicationForegrounded()} should be called whenever an activity is
         * foregrounded in order to trigger requerying accessibility settings more frequently.
         * Calling {@link onActivityOrApplicationForegrounded()} only when the app is foregrounded
         * is acceptable.
         */
        void onActivityOrApplicationForegrounded();

        /** Called when the application is moved to the background. */
        void onApplicationBackgrounded();
    }

    void setObserver(@Nullable Observer observer);
}
