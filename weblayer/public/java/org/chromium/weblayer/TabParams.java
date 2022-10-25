// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

import org.chromium.webengine.interfaces.ITabParams;

/**
 * Parameters for {@link Tab}.
 */
class TabParams {
    static ITabParams buildParcelable(@NonNull Tab tab) {
        ITabParams parcel = new ITabParams();
        parcel.tabProxy = tab.getTabProxy();
        parcel.tabGuid = tab.getGuid();
        parcel.navigationControllerProxy = tab.getTabNavigationControllerProxy();

        return parcel;
    }
}