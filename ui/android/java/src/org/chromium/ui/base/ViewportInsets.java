// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

/**
 * Information about various kinds of insets on the application viewport.
 *
 * Objects of this class are vended by ApplicationViewportInsetSupplier.
 */
public class ViewportInsets {
    /**
     * The total vertical inset on the application viewport coming from all visible UI controls.
     * TODO(bokan): This will have to be split up into top/bottom when browser controls are
     * added.
     */
    public int viewVisibleHeightInset;

    /**
     * The bottom inset applied to the WebContents to get the visual viewport rect.
     */
    public int visualViewportBottomInset;
}
