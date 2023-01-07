// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

/**
 * Contains all of the command line switches that are specific to the ui/ portion of Chromium on
 * Android.
 */
public abstract class UiSwitches {
    // Enables the screenshot mode, which disables certain UI elements (e.g. dialogs) to facilitate
    // more easily scripting screenshots of web content.
    public static final String ENABLE_SCREENSHOT_UI_MODE = "enable-screenshot-ui-mode";

    // Prevent instantiation.
    private UiSwitches() {}
}
