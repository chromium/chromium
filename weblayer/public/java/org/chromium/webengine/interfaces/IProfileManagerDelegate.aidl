// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.IStringListCallback;

oneway interface IProfileManagerDelegate {
    void getAllProfileNames(IStringListCallback callback) = 1;
}
