// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import org.chromium.browserfragment.interfaces.IBrowserFragmentDelegate;
import org.chromium.browserfragment.interfaces.IBrowserSandboxCallback;
import org.chromium.browserfragment.interfaces.IFragmentParams;

interface IBrowserSandboxService {
    void initializeBrowserProcess(in IBrowserSandboxCallback callback) = 1;

    IBrowserFragmentDelegate createFragmentDelegate(in IFragmentParams params) = 2;

    void setRemoteDebuggingEnabled(in boolean enabled) = 3;
}