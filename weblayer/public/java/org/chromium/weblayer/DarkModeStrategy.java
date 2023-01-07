// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({DarkModeStrategy.PREFER_WEB_THEME_OVER_USER_AGENT_DARKENING,
        DarkModeStrategy.WEB_THEME_DARKENING_ONLY, DarkModeStrategy.USER_AGENT_DARKENING_ONLY})
@Retention(RetentionPolicy.SOURCE)
@interface DarkModeStrategy {
    /**
     * Only render pages in dark mode if they provide a dark theme in their CSS. If no theme is
     * provided, the page will render with its default styling, which could be a light theme.
     */
    int WEB_THEME_DARKENING_ONLY =
            org.chromium.weblayer_private.interfaces.DarkModeStrategy.WEB_THEME_DARKENING_ONLY;

    /**
     * Always apply automatic user-agent darkening to pages, ignoring any dark theme that the
     * site provides. All pages will appear dark in this mode.
     */
    int USER_AGENT_DARKENING_ONLY =
            org.chromium.weblayer_private.interfaces.DarkModeStrategy.USER_AGENT_DARKENING_ONLY;

    /**
     * Render pages using their specified dark theme if available, otherwise fall back on automatic
     * user-agent darkening. All pages will appear dark in this mode.
     */
    int PREFER_WEB_THEME_OVER_USER_AGENT_DARKENING =
            org.chromium.weblayer_private.interfaces.DarkModeStrategy
                    .PREFER_WEB_THEME_OVER_USER_AGENT_DARKENING;
}
