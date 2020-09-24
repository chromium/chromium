// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.Context;

/**
 * Util class for dealing with resources.
 */
public class ResourceUtil {
    public static final int REQUIRED_PACKAGE_IDENTIFIER = 12;

    private ResourceUtil() {}

    /** Gets the ID of a resource in a remote context. */
    public static int getIdentifier(Context context, String name, String packageName) {
        int id = context.getResources().getIdentifier(name, null, packageName);
        // This was build with app_as_shared_lib, no need to modify package ID.
        if ((id & 0xff000000) == 0x7f000000) {
            return id;
        }

        // Force the returned ID to use our magic package ID.
        id &= 0x00ffffff;
        id |= (0x01000000 * REQUIRED_PACKAGE_IDENTIFIER);
        return id;
    }
}
