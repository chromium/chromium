// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.os.SystemClock;
import android.view.DragEvent;
import android.view.View.DragShadowBuilder;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;

/**
 * Drag-Drop objects to be shared across instances. General usage:
 * <li>When drag starts, the client that puts the global instance will do {@link #store} and
 *     generate a tracker token for later use.
 * <li>During the drag, client that starts the drag can {@link #getState(TrackerToken)} to access /
 *     modify the global state.
 * <li>When dropped, view that receive the drop can use {@link #getState(DragEvent)} to access /
 *     modify the global state.
 * <li>When drag ends, client that starts the drag need to do {@link #clear(TrackerToken)} to reset
 *     the global state.
 */
public class DragDropGlobalState {
    private static final String TAG = "DnDGlobalState";
    private @Nullable static GlobalStateHolder sGlobalStateHolder;

    /** Token used to retrieve from {@link DragDropGlobalState#getState(TrackerToken)}. */
    public static final class TrackerToken {
        final long mKey;

        TrackerToken() {
            this.mKey = SystemClock.elapsedRealtime();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof TrackerToken that)) {
                return false;
            }
            return that.mKey == this.mKey;
        }
    }

    /**
     * Build a {@link DragDropGlobalState} to be stored in the static instance, and return the
     * {@link TrackerToken} to retrieve and modify the state.
     *
     * @param dragSourceInstanceId Instance Id from the source window
     * @param dropData {@link DropDataAndroid} used to start this drag and drop.
     * @param dragShadowBuilder {@link DragShadowBuilder} used for starting the drag and drop.
     * @return Token used to retrieve and clear the global state.
     */
    public static TrackerToken store(
            int dragSourceInstanceId,
            @NonNull DropDataAndroid dropData,
            @Nullable DragShadowBuilder dragShadowBuilder) {
        if (sGlobalStateHolder != null) {
            Log.w(
                    TAG,
                    "Holder isn't cleared before store. "
                            + sGlobalStateHolder.mInstance.toString());
        }
        sGlobalStateHolder =
                new GlobalStateHolder(
                        new DragDropGlobalState(dragSourceInstanceId, dropData), dragShadowBuilder);
        return sGlobalStateHolder.mToken;
    }

    /** Returns boolean indicating if static instance was created. */
    public static boolean hasValue() {
        return sGlobalStateHolder != null;
    }

    /**
     * Get the global state using tracker token without cleaning the global state.
     *
     * @param token Token returned by {@link #store(int, ChromeDropDataAndroid, DragShadowBuilder)}
     * @return The stored global state.
     */
    public static @Nullable DragDropGlobalState getState(TrackerToken token) {
        if (sGlobalStateHolder == null || !sGlobalStateHolder.mToken.equals(token)) return null;
        return sGlobalStateHolder.mInstance;
    }

    /** Get the global state using DragEvent with {@link DragEvent#ACTION_DROP}. */
    public static @Nullable DragDropGlobalState getState(@NonNull DragEvent dropEvent) {
        if (sGlobalStateHolder == null || dropEvent.getAction() != DragEvent.ACTION_DROP) {
            return null;
        }
        return sGlobalStateHolder.mInstance;
    }

    /**
     * Return the drag shadow builder during this drag and drop process, if provided to the
     * DragDropGlobalState.
     */
    public static DragShadowBuilder getDragShadowBuilder() {
        if (sGlobalStateHolder == null) return null;
        return sGlobalStateHolder.mDragShadowBuilder;
    }

    /**
     * Tokens are released when startDragAndDrop fails or by listeners on drag end event. If a
     * caller fails to release token, sHolder is not cleared and the next build call will fail.
     */
    public static void clear(@NonNull TrackerToken token) {
        assert sGlobalStateHolder == null || sGlobalStateHolder.mToken.equals(token)
                : "Token mismatch.";
        sGlobalStateHolder = null;
    }

    private final int mDragSourceInstanceId;
    private final @NonNull DropDataAndroid mDropData;

    DragDropGlobalState(int dragSourceInstanceId, @NonNull DropDataAndroid dropData) {
        mDragSourceInstanceId = dragSourceInstanceId;
        mDropData = dropData;
    }

    /** Return whether the drag state started by |instanceId|. */
    public boolean isDragSourceInstance(int instanceId) {
        return mDragSourceInstanceId == instanceId;
    }

    /** Return the Chrome instance id of the drag source held by the global state. */
    public int getDragSourceInstance() {
        return mDragSourceInstanceId;
    }

    /** Return the {@link DropDataAndroid} held by the global state. */
    public @NonNull DropDataAndroid getData() {
        return mDropData;
    }

    @NonNull
    @Override
    public String toString() {
        return "DragDropGlobalState" + " sourceId:" + mDragSourceInstanceId;
    }

    /** Helper class to store the static instance of global state. */
    private static class GlobalStateHolder {
        final DragDropGlobalState mInstance;
        final TrackerToken mToken;
        final @Nullable DragShadowBuilder mDragShadowBuilder;

        private GlobalStateHolder(
                DragDropGlobalState instance, @Nullable DragShadowBuilder dragShadowBuilder) {
            mInstance = instance;
            mToken = new TrackerToken();
            mDragShadowBuilder = dragShadowBuilder;
        }
    }

    public static void setInstanceForTesting(DragDropGlobalState instance) {
        sGlobalStateHolder = new GlobalStateHolder(instance, null);
        ResettersForTesting.register(() -> sGlobalStateHolder = null);
    }

    public static DragDropGlobalState getForTesting() {
        assert sGlobalStateHolder != null;
        return sGlobalStateHolder.mInstance;
    }

    public static void clearForTesting() {
        sGlobalStateHolder = null;
    }
}
