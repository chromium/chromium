// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.ITabProxy;
import org.chromium.webengine.interfaces.ITabNavigationControllerProxy;

parcelable ITabParams {
    ITabProxy tabProxy;
    String tabGuid;
    ITabNavigationControllerProxy navigationControllerProxy;
}