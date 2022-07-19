// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import org.chromium.browserfragment.interfaces.IBrowserSandboxCallback;

interface IBrowserSandboxService {
    // TODO(rayankans): Move this to a more appropriate interface once more of the browserfragment
    // library is defined.
    void attachViewHierarchy(in IBinder hostToken, in IBrowserSandboxCallback callback) = 1;
}