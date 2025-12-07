// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import org.chromium.build.annotations.NullMarked;

/**
 * Contains all of the command line switches that are specific to the ui/ portion of Chromium on
 * Android.
 */
@NullMarked
public abstract class UiSwitches {
    // Enables the screenshot mode, which disables certain UI elements (e.g. dialogs) to facilitate
    // more easily scripting screenshots of web content.
    public static final String ENABLE_SCREENSHOT_UI_MODE = "enable-screenshot-ui-mode";

    // Enables drawing debug layers for edge-to-edge components to highlight the system insets
    // those components are drawing into.
    // LINT.IfChange(EnableEdgeToEdgeDebugLayers)
    public static final String ENABLE_EDGE_TO_EDGE_DEBUG_LAYERS =
            "enable-edge-to-edge-debug-layers";

    // LINT.ThenChange(//ui/base/ui_base_switches.h:EnableEdgeToEdgeDebugLayers)

    // Prevent instantiation.
    private UiSwitches() {}
}
