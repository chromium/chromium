// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

import org.chromium.browserfragment.interfaces.ITabParams;

/**
 * Parameters for {@link Tab}.
 */
class TabParams {
    static ITabParams buildParcelable(@NonNull Tab tab) {
        ITabParams parcel = new ITabParams();
        parcel.tabProxy = new TabProxy(tab);
        parcel.tabGuid = tab.getGuid();
        parcel.navigationControllerProxy =
                new TabNavigationControllerProxy(tab.getNavigationController());

        return parcel;
    }
}