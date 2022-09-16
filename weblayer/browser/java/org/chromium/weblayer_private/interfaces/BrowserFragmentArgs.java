// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/** Keys for the Bundle of arguments with which BrowserFragments are created. */
public interface BrowserFragmentArgs {
    String PROFILE_NAME = "profile_name";
    String PERSISTENCE_ID = "persistence_id";
    /**
     * A boolean value indicating whether the profile is incognito.
     */
    String IS_INCOGNITO = "is_incognito";
    String USE_VIEW_MODEL = "use_view_model";
}
