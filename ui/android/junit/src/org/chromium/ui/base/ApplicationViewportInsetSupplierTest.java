// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.mojom.VirtualKeyboardMode;

/** Unit tests for the ApplicationViewportInsetSupplier. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class ApplicationViewportInsetSupplierTest {
    /** A callback with the ability to get the last value pushed to it. */
    private static class CapturingCallback<T> implements Callback<T> {
        private T mValue;

        @Override
        public void onResult(T result) {
            mValue = result;
        }

        public T getCapturedValue() {
            return mValue;
        }
    }

    private ApplicationViewportInsetSupplier mWindowApplicationInsetSupplier;
    private ObservableSupplierImpl<Integer> mKeyboardInsetSupplier;
    private CapturingCallback<ViewportInsets> mInsetObserver;

    @Before
    public void setUp() {
        mWindowApplicationInsetSupplier = new ApplicationViewportInsetSupplier();
        mKeyboardInsetSupplier = new ObservableSupplierImpl<>();

        mInsetObserver = new CapturingCallback<>();

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);
        mWindowApplicationInsetSupplier.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mWindowApplicationInsetSupplier.addObserver(mInsetObserver);

        // Clear the observer initially so tests can check whether it was called.
        mInsetObserver.onResult(null);
    }

    @Test
    public void testSupplierDidNotSetValue() {
        assertNotNull(
                "Supplier should provide a non-null value even when unset.",
                mWindowApplicationInsetSupplier.get());
        assertEquals(
                "Initial value for viewVisibleHeightInset is incorrect.",
                0,
                mWindowApplicationInsetSupplier.get().viewVisibleHeightInset);
        assertEquals(
                "Initial value for webContentsHeightInset is incorrect.",
                0,
                mWindowApplicationInsetSupplier.get().webContentsHeightInset);
        assertEquals(
                "Initial value for visualViewportBottomInset is incorrect.",
                0,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);
    }

    @Test
    public void testKeyboardTriggersObserver() {
        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);

        mKeyboardInsetSupplier.set(5);

        assertEquals(
                "Keyboard does not insets the View's visible height.",
                0,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
        assertEquals(
                "Keyboard outsets the WebContents' visible height in RESIZES_VISUAL.",
                -5,
                mInsetObserver.getCapturedValue().webContentsHeightInset);
        assertEquals(
                "Keyboard insets the visual viewport in RESIZES_VISUAL.",
                5,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(
                VirtualKeyboardMode.OVERLAYS_CONTENT);

        assertEquals(
                "Keyboard does not inset the View's visible height.",
                0,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
        assertEquals(
                "Keyboard outsets WebContents' visible height in OVERLAYS_CONTENT.",
                -5,
                mInsetObserver.getCapturedValue().webContentsHeightInset);
        assertEquals(
                "Keyboard does not inset the visual viewport in OVERLAYS_CONTENT.",
                0,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);

        assertEquals(
                "Keyboard does not insetsthe View's visible height.",
                0,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
        assertEquals(
                "Keyboard does not inset the WebContents' visible height.",
                0,
                mInsetObserver.getCapturedValue().webContentsHeightInset);
        assertEquals(
                "Keyboard does not inset the visual viewport in RESIZES_CONTENT.",
                0,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        mKeyboardInsetSupplier.set(10);

        assertEquals(
                "Keyboard does not inset the View's visible height.",
                0,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
        assertEquals(
                "Keyboard does not inset the WebContents' visible height.",
                0,
                mInsetObserver.getCapturedValue().webContentsHeightInset);
        assertEquals(
                "Keyboard does not inset the visual viewport in RESIZES_CONTENT.",
                0,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);
    }

    @Test
    public void testKeyboardWithAccessory() {
        ObservableSupplierImpl<Integer> accessorySupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(accessorySupplier);

        mKeyboardInsetSupplier.set(10);
        accessorySupplier.set(5);

        assertEquals(
                "Only accessory insets the View's visible height.",
                5,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
        assertEquals(
                "Keyboard outsets WebContents' height; accessory has no effect in RESIZES_VISUAL.",
                -10,
                mInsetObserver.getCapturedValue().webContentsHeightInset);
        assertEquals(
                "Both keyboard and accessory inset the visual viewport in RESIZES_VISUAL.",
                15,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);

        assertEquals(
                "Only accessory insets the View's visible height.",
                5,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
        assertEquals(
                "Only the accessory insets WebContents' visible height.",
                5,
                mInsetObserver.getCapturedValue().webContentsHeightInset);
        assertEquals(
                "Neither keyboard nor accessory resize the visual viewport in RESIZES_CONTENT.",
                0,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);
    }

    @Test
    public void testSupplierSetBeforeAddingTriggersObserverOnAdd() {
        ObservableSupplierImpl<Integer> supplier = new ObservableSupplierImpl<>();
        supplier.set(20);

        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(supplier);

        assertEquals(
                "The observer should have been triggered when the supplier was added.",
                20,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
    }

    @Test
    public void testRemovingSupplierTriggersObservers() {
        ObservableSupplierImpl<Integer> accessorySupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(accessorySupplier);

        mKeyboardInsetSupplier.set(20);
        accessorySupplier.set(5);

        assertEquals(
                "Observed value should come from accessory.",
                5,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);

        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(null);

        assertEquals(
                "Observed value should be 0 when accessory supplier removed.",
                0,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
    }

    @Test
    public void testAllSuppliersRemoved() {
        ObservableSupplierImpl<Integer> accessorySupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(accessorySupplier);

        mKeyboardInsetSupplier.set(10);
        accessorySupplier.set(5);

        assertEquals(
                "View inset is correct.",
                5,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
        assertEquals(
                "WebContents inset is correct.",
                -10,
                mInsetObserver.getCapturedValue().webContentsHeightInset);
        assertEquals(
                "VisualViewport inset is correct.",
                15,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setKeyboardInsetSupplier(null);
        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(null);

        assertEquals(
                "View inset should be 0 with no suppliers attached.",
                0,
                mInsetObserver.getCapturedValue().viewVisibleHeightInset);
        assertEquals(
                "WebContentsinset should be 0 with no suppliers attached.",
                0,
                mInsetObserver.getCapturedValue().webContentsHeightInset);
        assertEquals(
                "VisualViewport inset should be 0 with no suppliers attached.",
                0,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);
    }

    @Test
    public void testVisualViewportBottomInset() {
        ObservableSupplierImpl<Integer> accessorySupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(accessorySupplier);

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);

        assertEquals(
                "VisualViewport inset initially 0",
                0,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);

        mKeyboardInsetSupplier.set(10);

        assertEquals(
                "Keyboard insets visual viewport",
                10,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);

        accessorySupplier.set(20);

        assertEquals(
                "Accessory insets visual viewport",
                30,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(null);

        assertEquals(
                "Removing accessory removes visual viewport inset",
                10,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setKeyboardInsetSupplier(null);

        assertEquals(
                "Removing keyboard removes visual viewport inset",
                0,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);
    }

    @Test
    public void testVisualViewportInsetWithVirtualKeyboardModes() {
        ObservableSupplierImpl<Integer> accessorySupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(accessorySupplier);

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);

        assertEquals(
                "VisualViewport inset initially 0",
                0,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);

        mKeyboardInsetSupplier.set(10);
        assertEquals(
                "Keyboard doesn't inset visual viewport with RESIZES_CONTENT",
                0,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);

        accessorySupplier.set(20);
        assertEquals(
                "Accessory doesn't inset visual viewport with RESIZES_CONTENT",
                0,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(
                VirtualKeyboardMode.OVERLAYS_CONTENT);

        mKeyboardInsetSupplier.set(12);
        assertEquals(
                "Keyboard doesn't inset visual viewport with OVERLAYS_CONTENT",
                0,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);

        accessorySupplier.set(25);
        assertEquals(
                "Accessory doesn't inset visual viewport with OVERLAYS_CONTENT",
                0,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);
        assertEquals(
                "Accessory and keyboard inset visual viewport with RESIZES_VISUAL",
                37,
                mWindowApplicationInsetSupplier.get().visualViewportBottomInset);
    }

    @Test
    public void testTriggerVisualViewportObserver() {
        assertNull("Observer initially uncalled", mInsetObserver.getCapturedValue());

        ObservableSupplierImpl<Integer> accessorySupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(accessorySupplier);
        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);

        mKeyboardInsetSupplier.set(10);

        assertEquals(
                "Keyboard triggers visual viewport observer",
                10,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        accessorySupplier.set(20);

        assertEquals(
                "Accessory triggers visual viewport observer",
                30,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(null);

        assertEquals(
                "Removing accessory supplier triggers visual viewport observer",
                10,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        mWindowApplicationInsetSupplier.setKeyboardInsetSupplier(null);
        assertEquals(
                "Removing keyboard supplier triggers visual viewport observer",
                0,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);
    }

    @Test
    public void testTriggerObserverWithVirtualKeyboardModes() {
        ObservableSupplierImpl<Integer> accessorySupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.setKeyboardAccessoryInsetSupplier(accessorySupplier);
        mKeyboardInsetSupplier.set(30);
        accessorySupplier.set(15);

        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);

        assertEquals(
                "Initial webContentsHeightInset value is correct",
                15,
                mInsetObserver.getCapturedValue().webContentsHeightInset);
        assertEquals(
                "Initial visualViewportBottomInset value is correct",
                0,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        // Clear the observer so we can confirm it gets called.
        mInsetObserver.onResult(null);
        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(
                VirtualKeyboardMode.OVERLAYS_CONTENT);

        assertEquals(
                "Changing to OVERLAYS_CONTENT triggers observer",
                -30,
                mInsetObserver.getCapturedValue().webContentsHeightInset);

        mInsetObserver.onResult(null);
        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);

        assertEquals(
                "Changing to RESIZES_VISUAL triggers observer",
                45,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);

        mInsetObserver.onResult(null);
        mWindowApplicationInsetSupplier.setVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);

        assertEquals(
                "Changing from RESIZES_VISUAL triggers observer",
                0,
                mInsetObserver.getCapturedValue().visualViewportBottomInset);
    }
}
