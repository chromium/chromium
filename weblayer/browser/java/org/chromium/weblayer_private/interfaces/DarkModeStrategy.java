// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({DarkModeStrategy.PREFER_WEB_THEME_OVER_USER_AGENT_DARKENING,
        DarkModeStrategy.WEB_THEME_DARKENING_ONLY, DarkModeStrategy.USER_AGENT_DARKENING_ONLY})
@Retention(RetentionPolicy.SOURCE)
public @interface DarkModeStrategy {
    int WEB_THEME_DARKENING_ONLY = 0;
    int USER_AGENT_DARKENING_ONLY = 1;
    int PREFER_WEB_THEME_OVER_USER_AGENT_DARKENING = 2;
}
