// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * TestProfile is responsible for forwarding function calls to Profile.
 */
public final class TestProfile {
    public static void destroy(Profile profile) {
        profile.destroy();
    }
}