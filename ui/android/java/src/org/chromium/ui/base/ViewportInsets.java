// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

/**
 * Information about various kinds of insets on the application viewport.
 *
 * Objects of this class are vended by ApplicationViewportInsetSupplier. Here's a diagram explaining
 * each inset type and its purpose:
 *
 *
 *                            ┌─     ┌─ ┌─────────────────────────┐ ─┐
 *                            │    ┌─┤  │                         │  │
 *           ┌──────────────► │    │ │  │       Top Controls      │  │
 *           │ (inset)        └─   │ └─ ├─────────────────────────┤  │  ─┐
 *           │                     │    │                         │  │   │
 *           │                     │    │                         │  │
 *           │         viewVisible │    │                         │  │◄─CompositorViewHolder
 *           │         HeightInset │    │         Content         │  │         height
 *                                 │    │                         │  │   │
 *        webContents              │    │                         │  │   │
 *        HeightInset          ┌─  │ ┌─ ├┬───────────────────────┬┤  │   │
 *                             │   └─┤  ││                       ││  │   │
 *           │                 │     │  ││  Keyboard Accessory   ││  │   │
 *           │      ┌─         │     └─ │┼───────────────────────┼│ ─┘   │ WebContents
 *           │      │   visualViewport  ││                       ││      │ height
 *           └─────►│    BottomInset    ││       Keyboard        ││      │
 *         (outset) │          │        ││                       ││      │
 *                  │          │        │┼───────────────────────┼│      │
 *                  └─         └─       └─────────────────────────┘    ──┘
 *
 * The right side shows the size of the CompositorViewHolder,
 * which is part of the View hierarchy, as well as the WebContents, whose size is set from
 * CompositorViewHolder. (This picture shows the desired sizes in the RESIZES_VISUAL
 * VirtualKeyboardMode).
 *
 * viewVisibleHeightInset - This is the total vertical inset applied to the CompositorViewHolder
 * View height by any UI controls. Note that the keyboard is *not* included because it already
 * resizes the CompositorViewHolder as part of View layout. This inset describes what part of the
 * CompositorViewHolder is obscured to the user. It is unaffected by the VirtualKeyboardMode.
 *
 * webContentsHeightInset - This is the total  vertical inset applied to the CompositorViewHolder
 * View height to derive the WebContents' height. Which controls apply depends on the
 * VirtualKeyboardMode. In RESIZES_VISUAL and OVERLAYS_CONTENT modes, the keyboard should not resize
 * the WebContents so we *outset* CompositorViewHolder by the keyboard height (thus,
 * webContentsHeightInset is negative). The keyboard accessory is overlaying the
 * CompositorViewHolder so it has no effect.
 *
 * visualViewportBottomInset - This is the inset applied to the WebContents' bottom to derive the
 * visual viewport rect as provided by the window.visualViewport web API. In RESIZES_VISUAL the
 * keyboard resizes the visual viewport (rather than the full page contents) so this value will
 * inset by both the keyboard and the keyboard accessory.
 *
 * In RESIZES_CONTENT mode, the keyboard and accessory are expected to resize the web content so the
 * CompositorViewHolder height can simply be insetted by all the View-insetting UI - this is the
 * same value as viewVisibleInset.
 *
 *                           ┌─ ┌─────────────────────────┐        ─┐
 *                         ┌─┤  │                         │         │
 *                         │ │  │       Top Controls      │         │
 *                         │ └─ ├─────────────────────────┤ ─┐      │
 *                         │    │                         │  │      │
 *             viewVisible │    │                         │  │      │CompositorViewHolder
 *             HeightInset │    │         Content         │  │◄──┐  │       height
 *                         │    │                         │  │   │  │
 *             webContents │    │                         │  │   │  │
 *             HeightInset │    │                         │  │   │  │
 *                         │ ┌─ ├┬───────────────────────┬┤ ─┘   │  │
 *                         └─┤  ││                       ││      │  │
 *                           │  ││  Keyboard Accessory   ││      │  │
 *                           └─ │┼───────────────────────┼│      │ ─┘
 *                              ││                       ││    WebContents
 *                              ││       Keyboard        ││     height
 *                              ││                       ││
 *                              │┼───────────────────────┼│
 *                              └─────────────────────────┘
 */
public class ViewportInsets {
    /**
     * The total vertical inset on the application viewport coming from all visible UI controls.
     * TODO(bokan): This will have to be split up into top/bottom when browser controls are
     * added.
     */
    public int viewVisibleHeightInset;

    /**
     * The inset applied to the view height to derive the WebContents height.
     * TODO(bokan): If we can prevent the keyboard from resizing ContentViewHolder this can be
     * removed.
     */
    public int webContentsHeightInset;

    /** The bottom inset applied to the WebContents to derive the visual viewport rect. */
    public int visualViewportBottomInset;
}
