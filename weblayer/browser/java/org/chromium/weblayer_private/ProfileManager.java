// Copyright 2019 The Chromium Authors
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
    private final Map<String, ProfileImpl> mIncognitoProfiles = new HashMap<>();

    /** Returns existing or new Profile associated with the given name. */
    public ProfileImpl getProfile(String name, boolean isIncognito) {
        if (name == null) throw new IllegalArgumentException("Name shouldn't be null");
        Map<String, ProfileImpl> nameToProfileMap = getMapForProfileType(isIncognito);
        ProfileImpl existingProfile = nameToProfileMap.get(name);
        if (existingProfile != null) {
            return existingProfile;
        }

        ProfileImpl profile =
                new ProfileImpl(name, isIncognito, () -> nameToProfileMap.remove(name));
        nameToProfileMap.put(name, profile);
        return profile;
    }

    private Map<String, ProfileImpl> getMapForProfileType(boolean isIncognito) {
        return isIncognito ? mIncognitoProfiles : mProfiles;
    }
}
