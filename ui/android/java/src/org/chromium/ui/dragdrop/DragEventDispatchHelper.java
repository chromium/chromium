// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.util.SparseBooleanArray;
import android.view.DragEvent;
import android.view.View;
import android.view.View.OnDragListener;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

/**
 * Wrapper class that accounts for drag event coordinate differences when forwarding a
 * {@link DragEvent} from View A to View B. This class currently only support destinations that
 * implement {@link DragEventDispatchDestination}, in order to pass the offset correctly.
 * Once an instance of this dispatcher helper is created, it will attach to the sourceView as an
 * {@link View.OnDragListener}.
 *
 * <br/><br/>Sample use:
 *
 * <pre>
 *
 * class DestinationView implements DragEventDispatchDestination extends View {
 *     &#64;Override
 *     View view() { return this; }
 *
 *     &#64;Override
 *     boolean onDragEventWithOffset(DragEvent event, int dx, int dy) { //... };
 * };
 *
 * void setup() {
 *    View origin;
 *    DestinationView destination;
 *    new DragEventDispatchHelper(origin, destination);
 * }
 * </pre>
 */
public class DragEventDispatchHelper implements OnDragListener {
    static final int[] ALL_DRAG_ACTIONS =
            new int[] {
                DragEvent.ACTION_DRAG_STARTED,
                DragEvent.ACTION_DRAG_LOCATION,
                DragEvent.ACTION_DROP,
                DragEvent.ACTION_DRAG_ENDED,
                DragEvent.ACTION_DRAG_ENTERED,
                DragEvent.ACTION_DRAG_EXITED
            };

    /**
     * Interface indicating this view accept drag events that dispatches from the other views.
     * Expected to be implemented by a view class.
     */
    public interface DragEventDispatchDestination {
        /**
         * @return The view instance this destination represents.
         */
        View view();

        /**
         * Receive drag event with coordinate offset to {@link DragEvent#getX()} /
         * {@link DragEvent#getY()}. This is used when dispatching drag event from a view that shows
         * on top of the view hierarchy and blocks the drag event from passing into views on the
         * back.
         *
         * To get the right X / Y for the drag events:
         *  X = {@link DragEvent#getX()} + dx
         *  Y = {@link DragEvent#getY()} + dy
         *
         * @param event DragEvent that is received by the foreground view.
         * @param dx X-axis offset for the drag events.
         * @param dy Y-axis offset for the drag events.
         */
        boolean onDragEventWithOffset(DragEvent event, int dx, int dy);

        /**
         * Helper function to get the DragEventDispatchDestination object from View, if it is an
         * DragEventDispatchDestination instance. Return null otherwise.
         */
        static @Nullable DragEventDispatchDestination from(View view) {
            if (view instanceof DragEventDispatchDestination) {
                return (DragEventDispatchDestination) view;
            }
            return null;
        }
    }

    private final View mSourceView;
    private final DragEventDispatchDestination mDestinationView;

    private final SparseBooleanArray mSupportedActionsMask =
            new SparseBooleanArray(ALL_DRAG_ACTIONS.length);

    /**
     * Create a helper OnDragListener that will redirect the drag event from the |sourceView| onto
     * the |destination|. The instance will be used as the OnDragListener of the |sourceView|.
     * @param sourceView The sourceView where the drag event needs to be redirected.
     * @param destination The destination to receive the redirected drag events.
     */
    public DragEventDispatchHelper(View sourceView, DragEventDispatchDestination destination) {
        mSourceView = sourceView;
        mDestinationView = destination;

        mSourceView.setOnDragListener(this);

        // By default support forwarding all drag actions.
        for (int action : ALL_DRAG_ACTIONS) {
            // Do no notify DRAG_STARTED or DRAG_ENDED since Android will dispatch such drag event
            // to all views in the current hierarchy.
            boolean supported =
                    (action != DragEvent.ACTION_DRAG_STARTED
                            && action != DragEvent.ACTION_DRAG_ENDED);
            markActionSupported(action, supported);
        }
    }

    /** Stop dispatching drag events to the destination. */
    public void stop() {
        mSourceView.setOnDragListener(null);
    }

    @Override
    public boolean onDrag(View v, DragEvent event) {
        // Always handle the DRAG_START to receive following updates from the OS.
        boolean isDragStart = event.getAction() == DragEvent.ACTION_DRAG_STARTED;

        View destinationView = mDestinationView.view();
        if (destinationView == null
                || !destinationView.isEnabled()
                || !destinationView.isAttachedToWindow()
                || !isActionSupported(event.getAction())) {
            return isDragStart;
        }

        // ACTION_DRAG_ENTERED / ACTION_DRAG_EXITED / ACTION_DRAG_ENDED do not have coordinates
        // associated. Offset is not necessary when forwarding those events.
        if (event.getAction() == DragEvent.ACTION_DRAG_ENTERED
                || event.getAction() == DragEvent.ACTION_DRAG_EXITED
                || event.getAction() == DragEvent.ACTION_DRAG_ENDED) {
            return mDestinationView.onDragEventWithOffset(event, 0, 0) || isDragStart;
        }

        int[] sourceLocation = new int[2];
        mSourceView.getLocationOnScreen(sourceLocation);

        int[] destLocation = new int[2];
        destinationView.getLocationOnScreen(destLocation);

        int dx = sourceLocation[0] - destLocation[0];
        int dy = sourceLocation[1] - destLocation[1];

        return mDestinationView.onDragEventWithOffset(event, dx, dy) || isDragStart;
    }

    @VisibleForTesting
    void markActionSupported(int action, boolean supported) {
        mSupportedActionsMask.put(action, supported);
    }

    @VisibleForTesting
    boolean isActionSupported(int action) {
        return mSupportedActionsMask.get(action);
    }
}
