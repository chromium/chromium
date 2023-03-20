// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IObjectWrapper;

// Next value: 1
interface IRemoteFragmentClient {
    boolean startActivityForResult(in IObjectWrapper intent, in int requestCode, in IObjectWrapper options) = 0;
}