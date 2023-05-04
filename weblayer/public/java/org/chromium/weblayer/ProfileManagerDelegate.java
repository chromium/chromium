// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import org.chromium.webengine.interfaces.IProfileManagerDelegate;

/**
 * This class acts as a proxy between the WebSandbox and Profiles.
 */
public class ProfileManagerDelegate extends IProfileManagerDelegate.Stub {
    private WebLayer mWeblayer;

    ProfileManagerDelegate(WebLayer webLayer) {
        mWeblayer = webLayer;
    }
}
