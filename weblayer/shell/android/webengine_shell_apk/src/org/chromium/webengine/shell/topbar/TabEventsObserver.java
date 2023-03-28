// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell.topbar;

import org.chromium.webengine.Tab;

/**
 * An interface for setting values in the Top Bar.
 */
public interface TabEventsObserver {
    void onVisibleUriChanged(String uri);

    void onActiveTabChanged(Tab activeTab);

    void onTabAdded(Tab tab);

    void onTabRemoved(Tab tab);

    void onLoadProgressChanged(double progress);
}
