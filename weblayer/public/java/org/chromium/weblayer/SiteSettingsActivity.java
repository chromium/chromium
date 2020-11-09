// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.view.MenuItem;

import androidx.appcompat.app.AppCompatActivity;

import org.chromium.weblayer_private.interfaces.SiteSettingsIntentHelper;

/**
 * An Activity that displays various Site Settings UIs.
 */
public class SiteSettingsActivity extends AppCompatActivity {
    private static final String FRAGMENT_TAG = "siteSettingsFragment";

    /** The current instance of SiteSettingsActivity in the resumed state, if any. */
    private static SiteSettingsActivity sResumedInstance;

    /** Whether this activity has been created for the first time but not yet resumed. */
    private boolean mIsNewlyCreated;

    private static boolean sActivityNotExportedChecked;

    /**
     * Creates an Intent that will launch the root/full Site Settings UI, which displays a list of
     * settings categories.
     */
    public static Intent createIntentForCategoryList(Context context, String profileName) {
        return SiteSettingsIntentHelper.createIntentForCategoryList(
                context, profileName, "".equals(profileName));
    }

    /**
     * Creates an Intent that will launch the root/full Site Settings UI, which displays a list of
     * settings categories.
     *
     * @param profileName The name of the profile.
     * @param isIncognito If the profile is incognito.
     *
     * @since 87
     */
    public static Intent createIntentForCategoryList(
            Context context, String profileName, boolean isIncognito) {
        return SiteSettingsIntentHelper.createIntentForCategoryList(
                context, profileName, isIncognito);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ensureActivityNotExported();

        super.onCreate(savedInstanceState);

        mIsNewlyCreated = savedInstanceState == null;

        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        getSupportActionBar().setElevation(0);

        if (getSupportFragmentManager().findFragmentByTag(FRAGMENT_TAG) == null) {
            SiteSettingsFragment siteSettingsFragment = new SiteSettingsFragment();
            siteSettingsFragment.setArguments(getIntent().getExtras());
            getSupportFragmentManager()
                    .beginTransaction()
                    .add(android.R.id.content, siteSettingsFragment, FRAGMENT_TAG)
                    .commitNow();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Prevent the user from interacting with multiple instances of SiteSettingsActivity at the
        // same time (e.g. in multi-instance mode on a Samsung device), which would cause many fun
        // bugs.
        if (sResumedInstance != null && sResumedInstance.getTaskId() != getTaskId()) {
            if (mIsNewlyCreated) {
                // This activity was newly created and takes precedence over sResumedInstance.
                sResumedInstance.finish();
            } else {
                // This activity was unpaused or recreated while another instance of
                // SiteSettingsActivity was already showing. The existing instance takes precedence.
                finish();
                return;
            }
        }
        sResumedInstance = this;
        mIsNewlyCreated = false;
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (sResumedInstance == this) {
            sResumedInstance = null;
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void ensureActivityNotExported() {
        if (sActivityNotExportedChecked) return;
        sActivityNotExportedChecked = true;
        try {
            ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), 0);
            // If SiteSettingsActivity is exported, then it's vulnerable to a fragment injection
            // exploit:
            // http://securityintelligence.com/new-vulnerability-android-framework-fragment-injection
            if (activityInfo.exported) {
                throw new IllegalStateException("SiteSettingsActivity must not be exported.");
            }
        } catch (NameNotFoundException ex) {
            // Something terribly wrong has happened.
            throw new RuntimeException(ex);
        }
    }
}
