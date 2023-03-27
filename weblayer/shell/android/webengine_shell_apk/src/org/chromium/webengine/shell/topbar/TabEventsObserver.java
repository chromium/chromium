// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell.topbar;

import org.chromium.webengine.Tab;

/**
 * An interface for setting values in the Top Bar.
 */
public interface TabEventsObserver {
    void setUrlBar(String uri);

    void setProgress(double progress);

    void addTabToList(Tab tab);

    void removeTabFromList(Tab tab);

    void setTabListSelection(Tab tab);
}
