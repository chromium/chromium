// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.ITabListObserverDelegate;

oneway interface IWebEngineDelegate {
    // Tab operations.
    // TODO(swestphal): Move to TabManager;
    void setTabListObserverDelegate(ITabListObserverDelegate tabListObserverDelegate) = 15;

    void shutdown() = 1;
}
