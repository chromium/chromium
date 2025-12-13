// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import androidx.annotation.IntDef;
import androidx.annotation.StringDef;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.insets.WindowInsetsUtils;
import org.chromium.ui.util.TokenHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public class EdgeToEdgeManager {
    private final ObservableSupplierImpl<Boolean> mContentFitsWindowInsetsSupplier =
            new ObservableSupplierImpl<>();
    private @Nullable EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;
    private int mEdgeToEdgeToken = TokenHolder.INVALID_TOKEN;
    private final EdgeToEdgeSystemBarColorHelper mEdgeToEdgeSystemBarColorHelper;

    private static final String BACKUP_NAVBAR_INSETS_HISTOGRAM_BASE =
            "Android.EdgeToEdge.BackupNavbarInsets.";

    @StringDef({
        BackupNavbarInsetsCallSite.EDGE_TO_EDGE_CONTROLLER,
        BackupNavbarInsetsCallSite.EDGE_TO_EDGE_LAYOUT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BackupNavbarInsetsCallSite {
        String EDGE_TO_EDGE_CONTROLLER = "EdgeToEdgeController";
        String EDGE_TO_EDGE_LAYOUT = "EdgeToEdgeLayout";
    }

    /** The source of insets used as a backup for missing navbar insets. */
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        BackupNavbarInsetsSource.NO_APPLICABLE_BACKUP,
        BackupNavbarInsetsSource.TAPPABLE_ELEMENT,
        BackupNavbarInsetsSource.MANDATORY_SYSTEM_GESTURES,
        BackupNavbarInsetsSource.FILTERED_EXPLICITLY_DISABLED,
        BackupNavbarInsetsSource.FILTERED_WEAKER_SIGNALS,
        BackupNavbarInsetsSource.NUM_ENTRIES
    })
    public @interface BackupNavbarInsetsSource {
        int NO_APPLICABLE_BACKUP = 0;
        @Deprecated int TAPPABLE_ELEMENT = 1;
        int MANDATORY_SYSTEM_GESTURES = 2;
        int FILTERED_EXPLICITLY_DISABLED = 3;
        int FILTERED_WEAKER_SIGNALS = 4;

        int NUM_ENTRIES = 5;
    }

    /**
     * Creates an EdgeToEdgeManager for managing central edge-to-edge functionality.
     *
     * @param activity The {@link Activity} hosting the current window.
     * @param edgeToEdgeStateProvider The {@link EdgeToEdgeStateProvider} for drawing edge-to-edge.
     * @param systemBarColorHelperSupplier Supplies the {@link SystemBarColorHelper} that should be
     *     used to color the system bars when edge to edge is enabled.
     * @param shouldDrawEdgeToEdge Whether the host activity intends to draw edge-to-edge by
     *     default.
     * @param canColorStatusBarColor Whether the status bar color is able to be changed.
     */
    public EdgeToEdgeManager(
            Activity activity,
            EdgeToEdgeStateProvider edgeToEdgeStateProvider,
            OneshotSupplier<SystemBarColorHelper> systemBarColorHelperSupplier,
            boolean shouldDrawEdgeToEdge,
            boolean canColorStatusBarColor) {
        mContentFitsWindowInsetsSupplier.set(!shouldDrawEdgeToEdge);

        mEdgeToEdgeStateProvider = edgeToEdgeStateProvider;
        mEdgeToEdgeSystemBarColorHelper =
                new EdgeToEdgeSystemBarColorHelper(
                        activity.getWindow(),
                        getContentFitsWindowInsetsSupplier(),
                        systemBarColorHelperSupplier,
                        canColorStatusBarColor);

        if (shouldDrawEdgeToEdge) {
            mEdgeToEdgeToken = mEdgeToEdgeStateProvider.acquireSetDecorFitsSystemWindowToken();
        }
    }

    /** Destroys this instance and removes its dependencies. */
    public void destroy() {
        if (mEdgeToEdgeStateProvider != null) {
            mEdgeToEdgeStateProvider.releaseSetDecorFitsSystemWindowToken(mEdgeToEdgeToken);
            mEdgeToEdgeToken = TokenHolder.INVALID_TOKEN;
            mEdgeToEdgeStateProvider = null;
        }
        mEdgeToEdgeSystemBarColorHelper.destroy();
    }

    /**
     * Returns the {@link EdgeToEdgeStateProvider} for checking and requesting changes to the
     * edge-to-edge state.
     */
    public EdgeToEdgeStateProvider getEdgeToEdgeStateProvider() {
        assert mEdgeToEdgeStateProvider != null; // Ensure not destroyed.
        return mEdgeToEdgeStateProvider;
    }

    /**
     * Returns the {@link EdgeToEdgeSystemBarColorHelper} for setting the color of the system bars.
     */
    public EdgeToEdgeSystemBarColorHelper getEdgeToEdgeSystemBarColorHelper() {
        return mEdgeToEdgeSystemBarColorHelper;
    }

    /**
     * Sets whether the content should fit within the system's window insets, or if the content
     * should be drawn edge-to-edge (into the window insets).
     */
    public void setContentFitsWindowInsets(boolean contentFitsWindow) {
        mContentFitsWindowInsetsSupplier.set(contentFitsWindow);
    }

    /**
     * Returns true if the content should fit within the system's window insets, false if the
     * content should be drawn edge-to-edge (into the window insets).
     */
    public boolean shouldContentFitsWindowInsets() {
        return assumeNonNull(mContentFitsWindowInsetsSupplier.get());
    }

    /**
     * Returns the supplier informing whether the contents fit within the system's window insets.
     */
    public ObservableSupplier<Boolean> getContentFitsWindowInsetsSupplier() {
        return mContentFitsWindowInsetsSupplier;
    }

    /** Records the backup navbar insets histogram for the given callsite. */
    private static void recordBackupNavbarInsetsHistogram(
            @BackupNavbarInsetsCallSite String callSite, @BackupNavbarInsetsSource int source) {
        RecordHistogram.recordEnumeratedHistogram(
                BACKUP_NAVBAR_INSETS_HISTOGRAM_BASE + callSite,
                source,
                BackupNavbarInsetsSource.NUM_ENTRIES);
    }

    /**
     * Returns backup insets for the navigation bars, if possible. These backup insets will be
     * informed by other insets, such as the tappableElements insets or the system gestures insets.
     * This will return null if no backup insets are appropriate given the current window insets.
     *
     * @param hasSeenNonZeroNavigationBarInsets Whether a non-zero navigation bar has been seen.
     * @param windowInsets The window insets containing the most recent insets from the window.
     * @param callSite The caller requesting backup insets, used for metrics.
     * @param useBackupNavbarInsetsFieldTrial The EdgeToEdgeFieldTrial for determining whether the
     *     useBackupNavbarInsets feature is allowed on devices by the current device's manufacturer.
     */
    public static @Nullable Insets getBackupNavbarInsets(
            boolean hasSeenNonZeroNavigationBarInsets,
            WindowInsetsCompat windowInsets,
            @BackupNavbarInsetsCallSite String callSite,
            EdgeToEdgeFieldTrial useBackupNavbarInsetsFieldTrial,
            boolean canUseMandatoryGesturesInsets) {
        if (!useBackupNavbarInsetsFieldTrial.isEnabledForManufacturerVersion()) {
            recordBackupNavbarInsetsHistogram(
                    callSite, BackupNavbarInsetsSource.FILTERED_EXPLICITLY_DISABLED);
            return null;
        }

        // Restrict weak signals, like the system gestures, if non-zero navigation bar insets
        // have previously been seen during the session for this Activity / window.
        if (hasSeenNonZeroNavigationBarInsets) {
            recordBackupNavbarInsetsHistogram(
                    callSite, BackupNavbarInsetsSource.FILTERED_WEAKER_SIGNALS);
            return null;
        }

        Insets mandatorySystemGesturesInsets =
                windowInsets.getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        // A single non-zero mandatory system gestures inset likely indicates the navigation bar.
        // This is not as clear a signal as the tappable element, though, since mandatory system
        // gestures represent the area of a window where system gestures have priority and may
        // consume touch input, but they aren't intended to be used by the app for padding. Thus,
        // these mandatory system gestures only imply the presence of a navigation bar, they don't
        // definitively indicate it. Note, however, that the mandatory system gestures are not
        // always consistent with the system bar insets - the gesture insets may exceed the
        // typical navigation bar insets, leading to somewhat different UX.
        if (WindowInsetsUtils.hasOneNonZeroInsetExcludingTop(mandatorySystemGesturesInsets)
                && canUseMandatoryGesturesInsets) {
            recordBackupNavbarInsetsHistogram(
                    callSite, BackupNavbarInsetsSource.MANDATORY_SYSTEM_GESTURES);
            return Insets.of(
                    mandatorySystemGesturesInsets.left,
                    0,
                    mandatorySystemGesturesInsets.right,
                    mandatorySystemGesturesInsets.bottom);
        }
        recordBackupNavbarInsetsHistogram(callSite, BackupNavbarInsetsSource.NO_APPLICABLE_BACKUP);
        return null;
    }
}
