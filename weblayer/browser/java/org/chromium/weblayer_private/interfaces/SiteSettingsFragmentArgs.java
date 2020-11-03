// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/** Keys for the Bundle of arguments with which SiteSettingsFragments are created. */
public interface SiteSettingsFragmentArgs {
    String ACTIVITY_CLASS_NAME = "org.chromium.weblayer.SiteSettingsActivity";

    // Argument names
    String PROFILE_NAME = "profile_name";
    String IS_INCOGNITO_PROFILE = "is_incognito_profile";
    String FRAGMENT_NAME = "fragment_name";
    String FRAGMENT_ARGUMENTS = "fragment_arguments";

    // FRAGMENT_NAME values
    String ALL_SITES = "all_sites";
    String CATEGORY_LIST = "category_list";
    String SINGLE_CATEGORY = "single_category";
    String SINGLE_WEBSITE = "single_website";

    // SINGLE_WEBSITE argument names
    String SINGLE_WEBSITE_URL = "url";

    // SINGLE_CATEGORY argument names
    String SINGLE_CATEGORY_TITLE = "title";
    String SINGLE_CATEGORY_TYPE = "type";

    // ALL_SITES argument names
    String ALL_SITES_TITLE = "title";
    String ALL_SITES_TYPE = "type";
}
