// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import org.chromium.weblayer_private.interfaces.IRemoteFragment;

/**
 * Class to handle events forwarded by WebFragmentEventDelegate.
 */
final class WebFragmentEventHandler extends RemoteFragmentEventHandler {
    public WebFragmentEventHandler(IRemoteFragment remoteFragment) {
        super(remoteFragment);
    }
}
