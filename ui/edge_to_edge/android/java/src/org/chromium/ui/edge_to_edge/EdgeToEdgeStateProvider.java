// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.Window;

import androidx.core.view.WindowCompat;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.TokenHolder;

/**
 * Activity level coordinator that manages edge to edge related interactions.
 *
 * <p>{@link #get()} never returns null for this class.
 */
@NullMarked
public class EdgeToEdgeStateProvider extends ObservableSupplierImpl<Boolean> {
    private static final UnownedUserDataKey<EdgeToEdgeStateProvider> KEY =
            new UnownedUserDataKey<>();
    private final TokenHolder mTokenHolder = new TokenHolder(this::onTokenUpdate);
    private final Window mWindow;

    /** Attach the current instance to a WindowAndroid object. */
    public void attach(WindowAndroid windowAndroid) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), this);
    }

    /**
     * Retrieve the EdgeToEdgeStateProvider associated with the given WindowAndroid as boolean
     * supplier.
     */
    public static @Nullable ObservableSupplier<Boolean> from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Whether edge to edge has been requested by a Chrome owned window. When true, the window has a
     * non-empty {@link EdgeToEdgeStateProvider} attached.
     */
    public static boolean isEdgeToEdgeEnabledForWindow(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return false;

        ObservableSupplier<Boolean> stateProvider = from(windowAndroid);
        return stateProvider != null && Boolean.TRUE.equals(stateProvider.get());
    }

    /** Detach this instance from all windows. */
    public void detach() {
        KEY.detachFromAllHosts(this);
    }

    /**
     * Create the state provider with the window it should associate with.
     *
     * @param window The activity window this provider is associated with.
     */
    public EdgeToEdgeStateProvider(Window window) {
        super(/* initialValue= */ false);
        mWindow = window;
    }

    /**
     * Request a call to draw edge to edge, equivalent to {@code
     * Window.setDecorFitsSystemWindows(false)}.
     *
     * @return A token to release the edge to edge state
     */
    public int acquireSetDecorFitsSystemWindowToken() {
        return mTokenHolder.acquireToken();
    }

    /**
     * Release a token to edge to edge. When the token holder is empty, trigger a call to {@code
     * Window.setDecorFitsSystemWindows(true)}.
     */
    public void releaseSetDecorFitsSystemWindowToken(int token) {
        mTokenHolder.releaseToken(token);
    }

    private void onTokenUpdate() {
        boolean isEdgeToEdge = mTokenHolder.hasTokens();
        if (isEdgeToEdge == assumeNonNull(get())) {
            return;
        }

        // Edge to edge mode changed.
        WindowCompat.setDecorFitsSystemWindows(mWindow, !isEdgeToEdge);
        set(isEdgeToEdge);
    }
}
