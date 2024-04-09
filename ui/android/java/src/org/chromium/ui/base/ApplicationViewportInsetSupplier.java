// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.ui.mojom.VirtualKeyboardMode;

/**
 * A class responsible for managing multiple users of UI insets over the application viewport.
 *
 * <p>UI insets are complicated. The application viewport is provided by CompositorViewHolder but
 * the browser provides various UI controls which overlay the viewport. How these UI controls
 * interact with the underlying WebContents varies between controls and can depend on web-APIs.
 *
 * <p>For example, browser controls cause the WebContents to resize so that the web page reflows in
 * response to showing/hiding (although the timing of when this happens is non-straightforward). On
 * the other hand, the virtual keyboard should resize only the page's visualViewport, without
 * affecting layout. Chrome provides "keyboard accessories" that appear to the user to be part of
 * the keyboard but are actually separate UI components. To make matters even more complicated, the
 * page can change how the virtual keyboard affects the page (to affect page layout).
 *
 * <p>This class aims to centralize and encapsulate all these complex interactions so clients don't
 * have to worry about the details. This class currently handles only the keyboard and keyboard
 * accessory but there are plans to move browser controls into here as well
 * (https://crbug.com/1211066).
 *
 * <p>Features needing to know if anything is obscuring part of the screen listen to this class via
 * {@link #addObserver(Callback)} which observes changes to a {@link ViewportInsets} object which
 * has various inset types clients can use. See that class for more detials about the inset types.
 *
 * <pre>
 * In general:
 *  - Features that want to modify the inset should pass around the {@link
 *    ApplicationViewportInsetSupplier} object.
 *  - Features only interested in what the current inset is should pass around an {@link
 *    ObservableSupplier<ViewportInsets>} object.
 * </pre>
 */
public class ApplicationViewportInsetSupplier extends ObservableSupplierImpl<ViewportInsets>
        implements Destroyable {
    /** Keyboard related suppliers */
    private ObservableSupplier<Integer> mKeyboardInsetSupplier;

    private ObservableSupplier<Integer> mKeyboardAccessoryInsetSupplier;

    private ObservableSupplier<Integer> mBottomSheetInsetSupplier;

    /** The observer that gets attached to all inset suppliers. */
    private final Callback<Integer> mInsetSupplierObserver = (unused) -> computeInsets();

    /**
     * By default, the virtual keyboard overlays content, only resizing the visual viewport.
     *
     * Web content has APIs that change how the virtual keyboard interacts with content. This class
     * needs to know which mode we're in to determine how different kinds of insets are computed.
     */
    @VirtualKeyboardMode.EnumType
    private int mVirtualKeyboardMode = VirtualKeyboardMode.RESIZES_VISUAL;

    /** Default constructor. */
    ApplicationViewportInsetSupplier() {
        super();
        // Make sure this is initialized to 0 since "Integer" is an object and would be null
        // otherwise.
        super.set(new ViewportInsets());
    }

    public static ApplicationViewportInsetSupplier createForTests() {
        return new ApplicationViewportInsetSupplier();
    }

    @Override
    public void set(ViewportInsets value) {
        throw new IllegalStateException(
                "#set(...) should not be called directly on ApplicationViewportInsetSupplier.");
    }

    /** Clean up observers and suppliers. */
    @Override
    public void destroy() {
        setKeyboardInsetSupplier(null);
        setKeyboardAccessoryInsetSupplier(null);
    }

    /**
     * Notifies this object when the VirtualKeyboardMode of the currently active WebContents is
     * changed.
     *
     * This can happen as a result of a web content API call or swapping a WebContents or Tab.
     */
    public void setVirtualKeyboardMode(@VirtualKeyboardMode.EnumType int mode) {
        if (mVirtualKeyboardMode == mode) return;

        mVirtualKeyboardMode = mode;

        computeInsets();
    }

    /**
     * Sets the inset supplier for the soft keyboard itself.
     *
     * Pass null to unset the current supplier.
     */
    public void setKeyboardInsetSupplier(ObservableSupplier<Integer> insetSupplier) {
        boolean didRemove = false;

        if (mKeyboardInsetSupplier != null) {
            mKeyboardInsetSupplier.removeObserver(mInsetSupplierObserver);
            didRemove = true;
        }

        mKeyboardInsetSupplier = insetSupplier;

        if (mKeyboardInsetSupplier != null) {
            mKeyboardInsetSupplier.addObserver(mInsetSupplierObserver);
        } else if (didRemove) {
            // If a supplier was removed, removeObserver will not have notified observers (unlike
            // addObserver) so make sure insets get recomputed in this case.
            computeInsets();
        }
    }

    /**
     * Sets the inset supplier for the keyboard accessory.
     *
     * Pass null to unset the current supplier.
     */
    public void setKeyboardAccessoryInsetSupplier(ObservableSupplier<Integer> insetSupplier) {
        boolean didRemove = false;
        if (mKeyboardAccessoryInsetSupplier != null) {
            mKeyboardAccessoryInsetSupplier.removeObserver(mInsetSupplierObserver);
            didRemove = true;
        }

        mKeyboardAccessoryInsetSupplier = insetSupplier;

        if (mKeyboardAccessoryInsetSupplier != null) {
            mKeyboardAccessoryInsetSupplier.addObserver(mInsetSupplierObserver);
        } else if (didRemove) {
            // If a supplier was removed, removeObserver will not have notified observers (unlike
            // addObserver) so make sure insets get recomputed in this case.
            computeInsets();
        }
    }

    public void setBottomSheetInsetSupplier(ObservableSupplier<Integer> insetSupplier) {
        boolean didRemove = false;
        if (mBottomSheetInsetSupplier != null) {
            mBottomSheetInsetSupplier.removeObserver(mInsetSupplierObserver);
            didRemove = true;
        }

        mBottomSheetInsetSupplier = insetSupplier;

        if (mBottomSheetInsetSupplier != null) {
            mBottomSheetInsetSupplier.addObserver(mInsetSupplierObserver);
        } else if (didRemove) {
            // If a supplier was removed, removeObserver will not have notified observers (unlike
            // addObserver) so make sure insets get recomputed in this case.
            computeInsets();
        }
    }

    /** Compute the new total inset based on all registered suppliers. */
    private void computeInsets() {
        ViewportInsets newValues = new ViewportInsets();

        int keyboardInset = intFromSupplier(mKeyboardInsetSupplier);
        int accessoryInset = intFromSupplier(mKeyboardAccessoryInsetSupplier);
        int totalKeyboardInset = keyboardInset + accessoryInset;

        int bottomSheetInset = intFromSupplier(mBottomSheetInsetSupplier);
        newValues.viewVisibleHeightInset = accessoryInset;
        newValues.visualViewportBottomInset =
                mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL ? totalKeyboardInset : 0;

        // If the VirtualKeyboardMode is set to OVERLAYS_CONTENT or RESIZES_VISUAL, the
        // WebContents size will not match the View size when the keyboard is showing (these
        // modes mean "keyboard doesn't resize web content"). In that case, *outset* by the shown
        // keyboard height to keep the WebContents from being resized.
        boolean vkModeOutsetsWebContentsHeight =
                mVirtualKeyboardMode == VirtualKeyboardMode.OVERLAYS_CONTENT
                        || mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_VISUAL;
        int webContentsInset = 0;
        if (vkModeOutsetsWebContentsHeight) {
            // Avoid insetting by the accessory and outset by the keyboard to counter the View
            // resize.
            webContentsInset = -keyboardInset;
        } else {
            // The keyboard should cause the WebContents to resize. The keyboard itself is already
            // accounted for in the View height so just add the accessory.
            webContentsInset = accessoryInset;
        }
        webContentsInset += bottomSheetInset;

        newValues.webContentsHeightInset = webContentsInset;

        super.set(newValues);
    }

    private int intFromSupplier(ObservableSupplier<Integer> supplier) {
        if (supplier == null || supplier.get() == null) return 0;
        return supplier.get();
    }

    public boolean insetsAffectWebContentsSize() {
        return mVirtualKeyboardMode == VirtualKeyboardMode.RESIZES_CONTENT
                || mBottomSheetInsetSupplier != null;
    }
}
