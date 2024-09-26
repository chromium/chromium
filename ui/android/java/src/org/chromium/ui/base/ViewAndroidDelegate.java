// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;
import android.view.inputmethod.InputConnection;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.MarginLayoutParamsCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragAndDropDelegateImpl;
import org.chromium.ui.dragdrop.DragStateTracker;
import org.chromium.ui.dragdrop.DropDataAndroid;
import org.chromium.ui.mojom.CursorType;

/** Class to acquire, position, and remove anchor views from the implementing View. */
@JNINamespace("ui")
public class ViewAndroidDelegate {
    private static DragAndDropDelegate sDragAndDropDelegateForTesting;
    private final DragAndDropDelegateImpl mDragAndDropDelegateImpl;

    /**
     * The current container view. This view can be updated with
     * {@link #setContainerView()}.
     */
    protected ViewGroup mContainerView;

    // Temporary storage for use as a parameter of getLocationOnScreen().
    private int[] mTemporaryContainerLocation = new int[2];

    /** Notifies the observer when container view is updated. */
    public interface ContainerViewObserver {
        void onUpdateContainerView(ViewGroup view);
    }

    private ObserverList<ContainerViewObserver> mContainerViewObservers = new ObserverList<>();

    /** Notifies the listener of vertical scroll direction changes. */
    public interface VerticalScrollDirectionChangeListener {
        /**
         * Called when the vertical scroll direction changes.
         * @param directionUp Whether the scroll direction is up, i.e. swiping down.
         * @param currentScrollRatio The current scroll ratio of the page.
         */
        void onVerticalScrollDirectionChanged(boolean directionUp, float currentScrollRatio);
    }

    private final ObserverList<VerticalScrollDirectionChangeListener>
            mVerticalScrollDirectionChangeListeners = new ObserverList<>();

    private Callback<Boolean> mUpdateShouldShowStylusHoverIcon;

    /**
     * Sets a callback which should be called with the latest value of whether the element being
     * hovered over is editable.
     * @param callback the callback object.
     */
    public void setShouldShowStylusHoverIconCallback(Callback<Boolean> callback) {
        mUpdateShouldShowStylusHoverIcon = callback;
    }

    /**
     * Create and return a basic implementation of {@link ViewAndroidDelegate}.
     * @param containerView {@link ViewGroup} to be used as a container view.
     * @return a new instance of {@link ViewAndroidDelegate}.
     */
    public static ViewAndroidDelegate createBasicDelegate(ViewGroup containerView) {
        return new ViewAndroidDelegate(containerView);
    }

    protected ViewAndroidDelegate(ViewGroup containerView) {
        mContainerView = containerView;
        mDragAndDropDelegateImpl = new DragAndDropDelegateImpl();
    }

    /**
     * Adds observer that needs notification when container view is updated.
     *
     * @param observer {@link ContainerViewObserver} object. If {@code removObserver} is not used,
     *     then the object should have the lifetime same as this {@link ViewAndroidDelegate} to
     *     avoid gc issues.
     */
    public final void addObserver(ContainerViewObserver observer) {
        mContainerViewObservers.addObserver(observer);
    }

    /**
     * Removes observer that does not need notification when container view is updated anymore.
     *
     * @param observer {@link ContainerViewObserver} object.
     */
    public final void removeObserver(ContainerViewObserver observer) {
        mContainerViewObservers.removeObserver(observer);
    }

    /** Adds the provided {@link VerticalScrollDirectionChangeListener}. */
    public final void addVerticalScrollDirectionChangeListener(
            VerticalScrollDirectionChangeListener listener) {
        mVerticalScrollDirectionChangeListeners.addObserver(listener);
    }

    /** Removes the provided {@link VerticalScrollDirectionChangeListener}. */
    public final void removeVerticalScrollDirectionChangeListener(
            VerticalScrollDirectionChangeListener listener) {
        mVerticalScrollDirectionChangeListeners.removeObserver(listener);
    }

    /**
     * Updates the current container view to which this class delegates.
     *
     * <p>WARNING: This method can also be used to replace the existing container view,
     * but you should only do it if you have a very good reason to. Replacing the
     * container view has been designed to support fullscreen in the Webview so it
     * might not be appropriate for other use cases.
     *
     * <p>This method only performs a small part of replacing the container view and
     * embedders are responsible for:
     * <ul>
     *     <li>Disconnecting the old container view from all the references</li>
     *     <li>Updating the InternalAccessDelegate</li>
     *     <li>Reconciling the state with the new container view</li>
     *     <li>Tearing down and recreating the native GL rendering where appropriate</li>
     *     <li>etc.</li>
     * </ul>
     */
    public final void setContainerView(ViewGroup containerView) {
        ViewGroup oldContainerView = mContainerView;
        mContainerView = containerView;
        updateAnchorViews(oldContainerView);
        for (ContainerViewObserver observer : mContainerViewObservers) {
            observer.onUpdateContainerView(containerView);
        }
    }

    protected DragAndDropDelegate getDragAndDropDelegate() {
        return sDragAndDropDelegateForTesting != null
                ? sDragAndDropDelegateForTesting
                : mDragAndDropDelegateImpl;
    }

    /**
     * Get the tracker that records the drag event on the view this delegate attached to. Will
     * return null if there's no {@link DragStateTracker} set up.
     */
    public @Nullable DragStateTracker getDragStateTracker() {
        return null;
    }

    /** Return the {@link DragAndDropDelegateImpl} instance for this delegate class. */
    protected DragStateTracker getDragStateTrackerInternal() {
        return mDragAndDropDelegateImpl;
    }

    /**
     * Transfer existing anchor views from the old to the new container view. Called by {@link
     * setContainerView} only.
     *
     * @param oldContainerView Old container view just replaced by a new one.
     */
    public void updateAnchorViews(ViewGroup oldContainerView) {}

    /**
     * @return An anchor view that can be used to anchor decoration views like Autofill popup.
     */
    @CalledByNative
    public View acquireView() {
        ViewGroup containerView = getContainerViewGroup();
        if (containerView == null || containerView.getParent() == null) return null;
        View anchorView = new View(containerView.getContext());
        containerView.addView(anchorView);
        return anchorView;
    }

    /**
     * Release given anchor view.
     * @param anchorView The anchor view that needs to be released.
     */
    @CalledByNative
    public void removeView(View anchorView) {
        ViewGroup containerView = getContainerViewGroup();
        if (containerView == null) return;
        containerView.removeView(anchorView);
    }

    /**
     * Set the anchor view to specified position and size (all units in px).
     *
     * @param anchorView The view that needs to be positioned. This must be the result of a previous
     *     call to {@link acquireView} which has not yet been removed via {@link removeView}.
     * @param x X coordinate of the top left corner of the anchor view.
     * @param y Y coordinate of the top left corner of the anchor view.
     * @param width The width of the anchor view.
     * @param height The height of the anchor view.
     */
    @CalledByNative
    public void setViewPosition(
            View anchorView,
            float x,
            float y,
            float width,
            float height,
            int leftMargin,
            int topMargin) {
        ViewGroup containerView = getContainerViewGroup();
        if (containerView == null) return;
        assert anchorView.getParent() == containerView;

        int widthInt = Math.round(width);
        int heightInt = Math.round(height);
        int startMargin;

        if (containerView.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL) {
            startMargin = containerView.getMeasuredWidth() - Math.round(width + x);
        } else {
            startMargin = leftMargin;
        }
        if (widthInt + startMargin > containerView.getWidth()) {
            widthInt = containerView.getWidth() - startMargin;
        }
        MarginLayoutParams mlp = (MarginLayoutParams) anchorView.getLayoutParams();
        mlp.width = widthInt;
        mlp.height = heightInt;
        MarginLayoutParamsCompat.setMarginStart(mlp, startMargin);
        mlp.topMargin = topMargin;
        anchorView.setLayoutParams(mlp);
    }

    /**
     * Start {@link View#startDragAndDrop(ClipData, DragShadowBuilder, Object, int)} with {@link
     * DropDataAndroid} from the web content.
     *
     * @param shadowImage The shadow image for the dragged object.
     * @param dropData The drop data presenting the drag target.
     * @param windowAndroid The WindowAndroid used to retrieve a relevant Context.
     * @param cursorOffsetX The x offset of the cursor w.r.t. to top-left corner of the drag-image.
     * @param cursorOffsetY The y offset of the cursor w.r.t. to top-left corner of the drag-image.
     * @param dragObjRectWidth The width of the drag object.
     * @param dragObjRectHeight The height of the drag object.
     */
    @CalledByNative
    private boolean startDragAndDrop(
            Bitmap shadowImage,
            DropDataAndroid dropData,
            WindowAndroid windowAndroid,
            int cursorOffsetX,
            int cursorOffsetY,
            int dragObjRectWidth,
            int dragObjRectHeight) {
        ViewGroup containerView = getContainerViewGroup();
        if (containerView == null || windowAndroid == null) return false;

        return getDragAndDropDelegate()
                .startDragAndDrop(
                        containerView,
                        shadowImage,
                        dropData,
                        windowAndroid.getContext().get(),
                        cursorOffsetX,
                        cursorOffsetY,
                        dragObjRectWidth,
                        dragObjRectHeight);
    }

    @VisibleForTesting
    @CalledByNative
    public void onCursorChangedToCustom(Bitmap customCursorBitmap, int hotspotX, int hotspotY) {
        PointerIcon icon = PointerIcon.create(customCursorBitmap, hotspotX, hotspotY);

        getContainerViewGroup().setPointerIcon(icon);
    }

    @VisibleForTesting
    @CalledByNative
    public void onCursorChanged(int cursorType) {
        int pointerIconType = PointerIcon.TYPE_ARROW;
        switch (cursorType) {
            case CursorType.NONE:
                pointerIconType = PointerIcon.TYPE_NULL;
                break;
            case CursorType.POINTER:
                pointerIconType = PointerIcon.TYPE_ARROW;
                break;
            case CursorType.CONTEXT_MENU:
                pointerIconType = PointerIcon.TYPE_CONTEXT_MENU;
                break;
            case CursorType.HAND:
                pointerIconType = PointerIcon.TYPE_HAND;
                break;
            case CursorType.HELP:
                pointerIconType = PointerIcon.TYPE_HELP;
                break;
            case CursorType.WAIT:
                pointerIconType = PointerIcon.TYPE_WAIT;
                break;
            case CursorType.CELL:
                pointerIconType = PointerIcon.TYPE_CELL;
                break;
            case CursorType.CROSS:
                pointerIconType = PointerIcon.TYPE_CROSSHAIR;
                break;
            case CursorType.I_BEAM:
                pointerIconType = PointerIcon.TYPE_TEXT;
                break;
            case CursorType.VERTICAL_TEXT:
                pointerIconType = PointerIcon.TYPE_VERTICAL_TEXT;
                break;
            case CursorType.ALIAS:
                pointerIconType = PointerIcon.TYPE_ALIAS;
                break;
            case CursorType.COPY:
                pointerIconType = PointerIcon.TYPE_COPY;
                break;
            case CursorType.NO_DROP:
                pointerIconType = PointerIcon.TYPE_NO_DROP;
                break;
            case CursorType.COLUMN_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case CursorType.ROW_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case CursorType.NORTH_EAST_SOUTH_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW;
                break;
            case CursorType.NORTH_WEST_SOUTH_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW;
                break;
            case CursorType.ZOOM_IN:
                pointerIconType = PointerIcon.TYPE_ZOOM_IN;
                break;
            case CursorType.ZOOM_OUT:
                pointerIconType = PointerIcon.TYPE_ZOOM_OUT;
                break;
            case CursorType.GRAB:
                pointerIconType = PointerIcon.TYPE_GRAB;
                break;
            case CursorType.GRABBING:
                pointerIconType = PointerIcon.TYPE_GRABBING;
                break;
                // TODO(jaebaek): set types correctly after fixing http://crbug.com/584424.
            case CursorType.EAST_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case CursorType.NORTH_SOUTH_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case CursorType.EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case CursorType.NORTH_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case CursorType.NORTH_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW;
                break;
            case CursorType.NORTH_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW;
                break;
            case CursorType.SOUTH_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case CursorType.SOUTH_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW;
                break;
            case CursorType.SOUTH_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW;
                break;
            case CursorType.WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case CursorType.PROGRESS:
                pointerIconType = PointerIcon.TYPE_WAIT;
                break;
            case CursorType.NOT_ALLOWED:
                pointerIconType = PointerIcon.TYPE_NO_DROP;
                break;
            case CursorType.MOVE:
            case CursorType.MIDDLE_PANNING:
                pointerIconType = PointerIcon.TYPE_ALL_SCROLL;
                break;
            case CursorType.EAST_PANNING:
            case CursorType.NORTH_PANNING:
            case CursorType.NORTH_EAST_PANNING:
            case CursorType.NORTH_WEST_PANNING:
            case CursorType.SOUTH_PANNING:
            case CursorType.SOUTH_EAST_PANNING:
            case CursorType.SOUTH_WEST_PANNING:
            case CursorType.WEST_PANNING:
                assert false : "These pointer icon types are not supported";
                break;
            case CursorType.CUSTOM:
                assert false : "onCursorChangedToCustom must be called instead";
                break;
        }
        ViewGroup containerView = getContainerViewGroup();
        PointerIcon icon = PointerIcon.getSystemIcon(containerView.getContext(), pointerIconType);

        containerView.setPointerIcon(icon);
    }

    @VisibleForTesting
    @CalledByNative
    public void notifyHoverActionStylusWritable(boolean stylusWritable) {
        if (mUpdateShouldShowStylusHoverIcon != null) {
            mUpdateShouldShowStylusHoverIcon.onResult(stylusWritable);
        }
    }

    /**
     * Called whenever the background color of the page changes as notified by Blink.
     * @param color The new ARGB color of the page background.
     */
    @CalledByNative
    public void onBackgroundColorChanged(int color) {}

    /**
     * Notify the client of the position of the top controls.
     *
     * @param topControlsOffsetY The Y offset of the top controls in physical pixels.
     * @param topContentOffsetY The Y offset of the content in physical pixels.
     * @param topControlsMinHeightOffsetY The current top controls min-height in physical pixels.
     * @param bottomControlsOffsetY The Y offset of the bottom controls in physical pixels.
     * @param bottomControlsMinHeightOffsetY The current bottom controls min-height in physical
     *     pixels.
     */
    @CalledByNative
    public void onControlsChanged(
            int topControlsOffsetY,
            int topContentOffsetY,
            int topControlsMinHeightOffsetY,
            int bottomControlsOffsetY,
            int bottomControlsMinHeightOffsetY) {}

    /**
     * @return The Visual Viewport bottom inset in pixels.
     */
    @CalledByNative
    protected int getViewportInsetBottom() {
        return 0;
    }

    /**
     * Called when root scroll direction changes.
     *
     * @param directionUp whether the new scroll direction is up (true) or down (false).
     * @param currentScrollRatio the ratio of vertical scroll in [0, 1] range. Scroll at top of page
     *     is 0, and bottom of page is 1. It is defined as 0 if page is not scrollable, though this
     *     should not be called in that case.
     */
    @CalledByNative
    @CallSuper
    protected void onVerticalScrollDirectionChanged(boolean directionUp, float currentScrollRatio) {
        notifyVerticalScrollDirectionChangeListeners(directionUp, currentScrollRatio);
    }

    /**
     * While ViewAndroidDelegate takes a ViewGroup, and internally adds Views to it, all other
     * consumers should *not* be manipulating child Views. This is particularly important as the
     * container view is usually ContentView, and ContentView only supports children directly added
     * by this class. See ContentView for details on this.
     *
     * @return container view that the anchor views are added to. May be null.
     */
    @CalledByNative
    public final View getContainerView() {
        return mContainerView;
    }

    protected final ViewGroup getContainerViewGroup() {
        return mContainerView;
    }

    /** Return the X location of our container view. */
    @CalledByNative
    private int getXLocationOfContainerViewInWindow() {
        View container = getContainerView();
        if (container == null) return 0;

        container.getLocationInWindow(mTemporaryContainerLocation);
        return mTemporaryContainerLocation[0];
    }

    /** Return the Y location of our container view. */
    @CalledByNative
    private int getYLocationOfContainerViewInWindow() {
        View container = getContainerView();
        if (container == null) return 0;

        container.getLocationInWindow(mTemporaryContainerLocation);
        return mTemporaryContainerLocation[1];
    }

    /** Return the X location of our container view on screen. */
    @CalledByNative
    private int getXLocationOnScreen() {
        View container = getContainerView();
        if (container == null) return 0;

        container.getLocationOnScreen(mTemporaryContainerLocation);
        return mTemporaryContainerLocation[0];
    }

    /** Return the Y location of our container view on screen. */
    @CalledByNative
    private int getYLocationOnScreen() {
        View container = getContainerView();
        if (container == null) return 0;

        container.getLocationOnScreen(mTemporaryContainerLocation);
        return mTemporaryContainerLocation[1];
    }

    @CalledByNative
    private void requestDisallowInterceptTouchEvent() {
        ViewGroup container = getContainerViewGroup();
        if (container != null) container.requestDisallowInterceptTouchEvent(true);
    }

    @CalledByNative
    private void requestUnbufferedDispatch(MotionEvent event) {
        ViewGroup container = getContainerViewGroup();
        if (container != null) {
            for (int i = 0; i < event.getPointerCount(); i++) {
                // This is a workaround for crbug.com/1064161.
                // TODO(smaier) remove this if LG fixes the stylus bug.
                if (event.getToolType(i) == MotionEvent.TOOL_TYPE_STYLUS) {
                    return;
                }
            }
            container.requestUnbufferedDispatch(event);
        }
    }

    @CalledByNative
    private boolean hasFocus() {
        View containerView = getContainerView();
        return containerView == null ? false : ViewUtils.hasFocus(containerView);
    }

    @CalledByNative
    private void requestFocus() {
        View containerView = getContainerViewGroup();
        if (containerView != null) ViewUtils.requestFocus(containerView);
    }

    /**
     * @see InputConnection#performPrivateCommand(java.lang.String, android.os.Bundle)
     */
    public void performPrivateImeCommand(String action, Bundle data) {}

    private void notifyVerticalScrollDirectionChangeListeners(
            boolean directionUp, float currentScrollRatio) {
        for (VerticalScrollDirectionChangeListener listener :
                mVerticalScrollDirectionChangeListeners) {
            listener.onVerticalScrollDirectionChanged(directionUp, currentScrollRatio);
        }
    }

    /**
     * Forwards requests for a ViewStructure from the Android Autofill API to the implementing View.
     *
     * @see View#onProvideAutofillVirtualStructure(ViewStructure structure, int flags)
     */
    public void onProvideAutofillVirtualStructure(ViewStructure structure, int flags) {}

    /**
     * Forwards autofillable values from the Android Autofill API to the implementing View.
     *
     * @see View#autofill(SparseArray)
     */
    public void autofill(final SparseArray<AutofillValue> values) {}

    /**
     * Check whether the Android Autofill Framework can request a ViewStructure for Autofill.
     *
     * @return true iff an AutofillProvider provides a ViewStructure when prompted.
     */
    public boolean providesAutofillStructure() {
        return false;
    }

    /** Destroy and clean up dependencies (e.g. drag state tracker if set). */
    public void destroy() {
        // TODO(crbug.com/40215126): Call this in when destroying WebContents.
        mDragAndDropDelegateImpl.destroy();
    }

    public static void setDragAndDropDelegateForTest(DragAndDropDelegate testDelegate) {
        sDragAndDropDelegateForTesting = testDelegate;
        ResettersForTesting.register(() -> sDragAndDropDelegateForTesting = null);
    }
}
