// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell.topbar;

import org.chromium.webengine.Tab;

/**
 * An interface for setting the values in the Top Bar.
 */
public abstract class TopBar {
    public abstract void setUrlBar(String uri);

    public abstract void setProgress(double progress);

    public abstract boolean isTabActive(Tab tab);
}
