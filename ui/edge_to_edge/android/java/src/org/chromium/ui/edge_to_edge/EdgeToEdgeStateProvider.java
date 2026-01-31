// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge;

import android.view.Window;

import androidx.core.view.WindowCompat;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.TokenHolder;

/** Activity level coordinator that manages edge to edge related interactions. */
@NullMarked
public class EdgeToEdgeStateProvider {
    private static final UnownedUserDataKey<NonNullObservableSupplier<Boolean>> KEY =
            new UnownedUserDataKey<>();
    private final TokenHolder mTokenHolder = new TokenHolder(this::onTokenUpdate);
    private final Window mWindow;
    private final SettableNonNullObservableSupplier<Boolean> mSupplier =
            ObservableSuppliers.createNonNull(false);

    /** Attach the current instance to a WindowAndroid object. */
    public void attach(WindowAndroid windowAndroid) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), mSupplier);
    }

    /**
     * Retrieve the EdgeToEdgeStateProvider associated with the given WindowAndroid as boolean
     * supplier.
     */
    public static @Nullable NonNullObservableSupplier<Boolean> from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Whether edge to edge has been requested by a Chrome owned window. When true, the window has a
     * non-empty {@link EdgeToEdgeStateProvider} attached.
     */
    public static boolean isEdgeToEdgeEnabledForWindow(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return false;

        MonotonicObservableSupplier<Boolean> stateProvider = from(windowAndroid);
        return stateProvider != null && Boolean.TRUE.equals(stateProvider.get());
    }

    /** Detach this instance from all windows. */
    public void detach() {
        KEY.detachFromAllHosts(mSupplier);
    }

    /**
     * Create the state provider with the window it should associate with.
     *
     * @param window The activity window this provider is associated with.
     */
    public EdgeToEdgeStateProvider(Window window) {
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
        if (isEdgeToEdge == mSupplier.get()) {
            return;
        }

        // Edge to edge mode changed.
        WindowCompat.setDecorFitsSystemWindows(mWindow, !isEdgeToEdge);
        mSupplier.set(isEdgeToEdge);
    }

    public boolean isEdgeToEdgeEnabled() {
        return mSupplier.get();
    }

    public NonNullObservableSupplier<Boolean> getSupplier() {
        return mSupplier;
    }
}
