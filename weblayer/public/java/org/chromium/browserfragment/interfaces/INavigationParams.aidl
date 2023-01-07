// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.interfaces;

import android.net.Uri;

import org.chromium.browserfragment.interfaces.ITabProxy;
import org.chromium.browserfragment.interfaces.ITabNavigationControllerProxy;

parcelable INavigationParams {
    Uri uri;
    int statusCode;
    boolean isSameDocument;
}