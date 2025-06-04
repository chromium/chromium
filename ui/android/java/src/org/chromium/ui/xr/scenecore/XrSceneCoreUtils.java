// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.xr.scenecore;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Utility class for {@link XrSceneCoreSessionManager} . */
@NullMarked
public class XrSceneCoreUtils {
    /**
     * Get the {@link XrSceneCoreSessionManager} from the given context.
     *
     * @param context The context to get the {@link XrSceneCoreSessionManager} from.
     * @return The {@link XrSceneCoreSessionManager} or null if not found.
     */
    public static @Nullable XrSceneCoreSessionManager getXrSceneCoreSessionManagerFromContext(
            Context context) {
        XrSceneCoreSessionManager xrSceneCoreSessionManager = null;
        final Activity activity = ContextUtils.activityFromContext(context);
        if (activity instanceof XrSceneCoreSessionManagerProvider) {
            xrSceneCoreSessionManager =
                    ((XrSceneCoreSessionManagerProvider) activity).getXrSceneCoreSessionManager();
        }
        return xrSceneCoreSessionManager;
    }

    /**
     * Check if the given {@link XrSceneCoreSessionManager} is in FSM mode.
     *
     * @param xrScManager The {@link XrSceneCoreSessionManager} to check.
     * @return True if the {@link XrSceneCoreSessionManager} is in FSM mode, false otherwise.
     */
    public static boolean isSceneCoreSessionInFsm(@Nullable XrSceneCoreSessionManager xrScManager) {
        boolean isFsm = false;
        if (xrScManager != null) {
            Boolean value = xrScManager.getXrSpaceModeObservableSupplier().get();
            isFsm = Boolean.TRUE.equals(value);
        }
        return isFsm;
    }
}
