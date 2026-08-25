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
    public interface Observer {
        /**
         * Called when an activity is foregrounded. In multi-window mode, this enables updating the
         * accessibility state when an additional Chromium window is brought to the foreground after
         * modifying accessibility settings in the settings app.
         *
         * This method initiates recomputing accessibility settings. Calling {@link
         * onActivityMadeVisible()} should be called whenever an activity is foregrounded in order
         * to trigger requerying accessibility settings more frequently. Calling {@link
         * onActivityMadeVisible()} only when the app is foregrounded is acceptable.
         */
        void onAnyActivityMadeVisible();

        /** Called when the application is moved to the background. */
        void onApplicationBackgrounded();

        /** Called when the application is moved to the foreground. */
        void onApplicationForegrounded();
    }

    public void setObserver(@Nullable Observer observer);
}
