// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IRemoteFragment;

interface IBrowserFragment {
  IRemoteFragment asRemoteFragment() = 0;
  IBrowser getBrowser() = 1;
}
