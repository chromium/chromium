// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.permissions;

import org.chromium.build.annotations.NullMarked;

/** Callback for permission requests. */
@NullMarked
public interface PermissionCallback {
    /**
     * Called upon completing a permission request.
     *
     * @param permissions The list of permissions in the result.
     * @param grantResults The grant results for the corresponding permissions which is either
     *         {@link android.content.pm.PackageManager#PERMISSION_GRANTED} or {@link
     *         android.content.pm.PackageManager#PERMISSION_DENIED}.
     */
    void onRequestPermissionsResult(String[] permissions, int[] grantResults);
}
