// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge.layout;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.graphics.RectF;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;

import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsCompat.Type;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.edge_to_edge.BaseSystemBarColorHelper;
import org.chromium.ui.edge_to_edge.EdgeToEdgeFieldTrial;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager.BackupNavbarInsetsCallSite;
import org.chromium.ui.edge_to_edge.R;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer;

/**
 * Coordinator used to adjust the padding and paint color for {@link EdgeToEdgeBaseLayout}. This is
 * a house-made version of a base edge to edge component, different than the AndroidX Coordinator
 * layout that applies some Chrome specific insets coordination.
 *
 * <p>Currently the layout supports systemBars / displayCutout / IME
 */
@NullMarked
public class EdgeToEdgeLayoutCoordinator extends BaseSystemBarColorHelper
        implements WindowInsetsConsumer {
    private static final String TAG = "E2E_Layout";
    private final Context mContext;
    private final @Nullable InsetObserver mInsetObserver;
    private @Nullable EdgeToEdgeBaseLayout mView;

    private final boolean mUseBackupNavbarInsetsEnabled;
    private final @Nullable EdgeToEdgeFieldTrial mUseBackupNavbarInsetsFieldTrial;
    private final boolean mCanUseMandatoryGesturesInsets;

    /**
     * Construct the coordinator used to handle padding and color for the Edge to edge layout. If an
     * {@link InsetObserver} is not passed, the coordinator will assume no inset coordination is
     * needed, and start observing insets over the e2e layout directly.
     *
     * @param context The Activity context.
     * @param insetObserver The inset observer of current window, if exists.
     * @param useBackupNavbarInsetsEnabled Whether backup insets can be used if the navbar insets
     *     seem to be missing.
     * @param useBackupNavbarInsetsFieldTrial The EdgeToEdgeFieldTrial to use to verify if backup
     *     insets are allowed on devices by the current manufacturer.
     * @param canUseMandatoryGesturesInsets Whether mandatory system gesture insets can be used as
     *     backup navbar insets.
     */
    public EdgeToEdgeLayoutCoordinator(
            Context context,
            @Nullable InsetObserver insetObserver,
            boolean useBackupNavbarInsetsEnabled,
            @Nullable EdgeToEdgeFieldTrial useBackupNavbarInsetsFieldTrial,
            boolean canUseMandatoryGesturesInsets) {
        mContext = context;
        mInsetObserver = insetObserver;
        mUseBackupNavbarInsetsEnabled = useBackupNavbarInsetsEnabled;
        mUseBackupNavbarInsetsFieldTrial = useBackupNavbarInsetsFieldTrial;
        mCanUseMandatoryGesturesInsets = canUseMandatoryGesturesInsets;
    }

    /**
     * Construct the coordinator used to handle padding and color for the Edge to edge layout. If an
     * {@link InsetObserver} is not passed, the coordinator will assume no inset coordination is
     * needed, and start observing insets over the e2e layout directly.
     *
     * <p>Note: with this constructor, the backup navbar insets feature will be fully disabled.
     *
     * @param context The Activity context.
     * @param insetObserver The inset observer of current window, if exists.
     */
    public EdgeToEdgeLayoutCoordinator(Context context, @Nullable InsetObserver insetObserver) {
        this(
                context,
                insetObserver,
                /* useBackupNavbarInsetsEnabled= */ false,
                /* useBackupNavbarInsetsFieldTrial= */ null,
                /* canUseMandatoryGesturesInsets= */ false);
    }

    /**
     * @see Activity#setContentView(View)
     */
    public View wrapContentView(View contentView) {
        return wrapContentView(
                contentView,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    /**
     * @see Activity#setContentView(View, LayoutParams)
     */
    public View wrapContentView(View contentView, @Nullable LayoutParams params) {
        ensureInitialized();
        if (params == null) {
            params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        }
        mView.addView(contentView, params);
        return mView;
    }

    // extends BaseSystemBarColorHelper
    @Override
    public void destroy() {
        if (mInsetObserver != null) {
            mInsetObserver.removeInsetsConsumer(this);
        }
        if (mView != null) {
            ViewCompat.setOnApplyWindowInsetsListener(mView, null);
            mView = null;
        }
    }

    @Override
    public void applyStatusBarColor() {
        if (mView == null) return;
        mView.setStatusBarColor(mStatusBarColor);
        updateStatusBarIconColor(mView.getRootView(), mStatusBarColor);
    }

    @Override
    public void applyNavBarColor() {
        if (mView == null) return;
        mView.setNavBarColor(mNavBarColor);
    }

    @Override
    public void applyNavigationBarDividerColor() {
        if (mView == null) return;
        mView.setNavBarDividerColor(mNavBarDividerColor);
    }

    @Override
    public WindowInsetsCompat onApplyWindowInsets(View view, WindowInsetsCompat windowInsets) {
        assumeNonNull(mView);
        Insets statusBarInsets = windowInsets.getInsets(Type.statusBars());
        mView.setStatusBarInsets(statusBarInsets);

        Insets navBarInsets = getNavigationBarInsets(windowInsets);
        mView.setNavigationBarInsets(navBarInsets);

        Insets cutout = windowInsets.getInsets(Type.displayCutout());
        mView.setDisplayCutoutInsetLeft(cutout.left > 0 ? cutout : Insets.NONE);
        mView.setDisplayCutoutInsetRight(cutout.right > 0 ? cutout : Insets.NONE);

        Insets captionBarInsets = windowInsets.getInsets(Type.captionBar());
        mView.setCaptionBarInsets(captionBarInsets);

        int paddingInsetTypes = Type.systemBars() + Type.ime();
        if (shouldPadDisplayCutout(windowInsets, mContext)) {
            paddingInsetTypes += Type.displayCutout();
            // Color display cutout padding to keep behaviour for Android 15-.
            mView.setDisplayCutoutTop(cutout.top > 0 ? cutout : Insets.NONE);
        } else {
            mView.setDisplayCutoutTop(Insets.NONE);
        }

        Insets overallInsets = windowInsets.getInsets(paddingInsetTypes);
        // Ensure backup navigation insets are also included.
        overallInsets = Insets.max(overallInsets, navBarInsets);
        mView.setPadding(
                overallInsets.left, overallInsets.top, overallInsets.right, overallInsets.bottom);

        // Consume the insets since the root view already adjusted their paddings.
        return new WindowInsetsCompat.Builder(windowInsets)
                .setInsets(Type.statusBars(), Insets.NONE)
                .setInsets(Type.navigationBars(), Insets.NONE)
                .setInsets(Type.captionBar(), Insets.NONE)
                .setInsets(Type.displayCutout(), Insets.NONE)
                .setInsets(Type.tappableElement(), Insets.NONE)
                .setInsets(Type.mandatorySystemGestures(), Insets.NONE)
                .setInsets(Type.ime(), Insets.NONE)
                .build();
    }

    private Insets getNavigationBarInsets(WindowInsetsCompat windowInsets) {
        Insets navBarInsets = windowInsets.getInsets(Type.navigationBars());
        Insets navBarInsetsIgnoringViz =
                windowInsets.getInsetsIgnoringVisibility(Type.navigationBars());
        // crbug.com/454781974 - if the navbar insets have been hidden, the device is in fullscreen
        // mode. tappableElement() insets should not be used, and are actively misleading as the OS
        // will continue to report non-zero tappable insets even if the system bars have been
        // hidden.
        if (!navBarInsets.equals(navBarInsetsIgnoringViz)) return navBarInsets;

        Insets tappableInsets = windowInsets.getInsets(WindowInsetsCompat.Type.tappableElement());
        Insets nonTopTappableInsets =
                Insets.of(tappableInsets.left, 0, tappableInsets.right, tappableInsets.bottom);
        Insets navBarAndTappableInsets = Insets.max(navBarInsets, nonTopTappableInsets);

        if (!mUseBackupNavbarInsetsEnabled) return navBarAndTappableInsets;
        if (mInsetObserver == null || mUseBackupNavbarInsetsFieldTrial == null) {
            return navBarAndTappableInsets;
        }

        if (navBarAndTappableInsets.left == 0
                && navBarAndTappableInsets.right == 0
                && navBarAndTappableInsets.bottom == 0) {
            @Nullable Insets backupNavbarInsets =
                    EdgeToEdgeManager.getBackupNavbarInsets(
                            mInsetObserver.hasSeenNonZeroNavigationBarInsets(),
                            windowInsets,
                            BackupNavbarInsetsCallSite.EDGE_TO_EDGE_LAYOUT,
                            mUseBackupNavbarInsetsFieldTrial,
                            mCanUseMandatoryGesturesInsets);
            // If applicable, apply backup navbar insets to the left, right, and bottom (not the
            // top, as that's always the status bar).
            if (backupNavbarInsets != null) {
                navBarAndTappableInsets =
                        Insets.of(
                                backupNavbarInsets.left,
                                navBarAndTappableInsets.top,
                                backupNavbarInsets.right,
                                backupNavbarInsets.bottom);
            }
        }
        return navBarAndTappableInsets;
    }

    /** Returns the edge-to-edge layout view. */
    public @Nullable EdgeToEdgeBaseLayout getView() {
        return mView;
    }

    @EnsuresNonNull("mView")
    private void ensureInitialized() {
        if (mView != null) return;

        mView =
                (EdgeToEdgeBaseLayout)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.edge_to_edge_base_layout, null, false);
        if (mInsetObserver != null) {
            mInsetObserver.addInsetsConsumer(
                    this, InsetConsumerSource.EDGE_TO_EDGE_LAYOUT_COORDINATOR);
        } else {
            ViewCompat.setOnApplyWindowInsetsListener(mView, this);
        }

        applyStatusBarColor();
        applyNavBarColor();
        applyNavigationBarDividerColor();
    }

    // Determine if padding is necessary according to WindowManager.LayoutParams. This is intended
    // to keep the behavior for Android 15-.
    // Ref: https://developer.android.com/develop/ui/views/layout/display-cutout
    private static boolean shouldPadDisplayCutout(WindowInsetsCompat insets, Context context) {
        if (insets == null) return true;

        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null) {
            Log.w(TAG, "should not receive window insets in non-activity context.");
            return false;
        }

        DisplayCutoutCompat cutout = insets.getDisplayCutout();
        if (cutout == null) return false;

        int cutoutMode = activity.getWindow().getAttributes().layoutInDisplayCutoutMode;
        switch (cutoutMode) {
            case WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS:
                return false;
            case WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER:
                return true;
            case WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES:
                // For web compatibility, we should add padding when insets does not overlap with
                // system bars.
                DisplayMetrics displayMetrics = activity.getResources().getDisplayMetrics();
                boolean isPortrait = displayMetrics.widthPixels < displayMetrics.heightPixels;

                if (isPortrait) {
                    // width < height, top / bottom are the short edge.
                    return cutout.getSafeInsetLeft() > 0 || cutout.getSafeInsetRight() > 0;
                }
                // else: height > width, left / right are the short edges
                return cutout.getSafeInsetTop() > 0 || cutout.getSafeInsetBottom() > 0;

            default: // LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT
                assert cutoutMode
                        == WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
                // From Android's doc: The window is allowed to extend into the DisplayCutout area,
                // only if the DisplayCutout is fully contained within a system bar or the
                // DisplayCutout is not deeper than 16 dp.

                Insets systemInsets = insets.getInsets(Type.systemBars());
                Insets systemAndCutoutInsets =
                        insets.getInsets(Type.systemBars() + Type.displayCutout());
                if (systemInsets.equals(systemAndCutoutInsets)) {
                    return false;
                }

                float density = activity.getResources().getDisplayMetrics().density;
                RectF rect =
                        new RectF(
                                cutout.getSafeInsetLeft() / density,
                                cutout.getSafeInsetTop() / density,
                                cutout.getSafeInsetRight() / density,
                                cutout.getSafeInsetBottom() / density);
                return (rect.left > 16 || rect.top > 16 || rect.right > 16 || rect.bottom > 16);
        }
    }
}
