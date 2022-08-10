// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import org.chromium.browserfragment.interfaces.ITabProxy;
import org.chromium.browserfragment.interfaces.ITabNavigationControllerProxy;

parcelable ITabParams {
    ITabProxy tabProxy;
    String tabGuid;
    ITabNavigationControllerProxy navigationControllerProxy;
}