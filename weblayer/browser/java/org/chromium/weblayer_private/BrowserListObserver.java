// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

/**
 * Notified of changes to BrowserList.
 */
public interface BrowserListObserver {
    void onBrowserCreated(BrowserImpl browser);
    void onBrowserDestroyed(BrowserImpl browser);
}
