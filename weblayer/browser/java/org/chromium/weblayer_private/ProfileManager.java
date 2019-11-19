// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import java.util.HashMap;
import java.util.Map;

/**
 * Creates and maintains the active Profiles.
 */
public class ProfileManager {
    private final Map<String, ProfileImpl> mProfiles = new HashMap<>();

    /** Returns existing or new Profile associated with the given path */
    public ProfileImpl getProfile(String path) {
        if (path == null) throw new IllegalArgumentException("Path shouldn't be null");
        ProfileImpl existingProfile = mProfiles.get(path);
        if (existingProfile != null) {
            return existingProfile;
        }

        ProfileImpl profile = new ProfileImpl(path, () -> mProfiles.remove(path));
        mProfiles.put(path, profile);
        return profile;
    }
}
