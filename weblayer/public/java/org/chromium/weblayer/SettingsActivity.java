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

import org.chromium.weblayer_private.interfaces.SettingsIntentHelper;

/**
 * An Activity that displays a Settings UI for WebLayer.
 *
 * This class is an implementation detail. To start an instance of this Activity, use one of the
 * SettingsActivity#createIntentFor* methods.
 *
 * @since 89
 */
public class SettingsActivity extends AppCompatActivity {
    private static final String FRAGMENT_TAG = "settingsFragment";

    /**
     * Tracks whether we've checked that this Activity isn't exported.
     *
     * @see SettingsActivity#ensureActivityNotExported()
     */
    private static boolean sActivityNotExportedChecked;

    /** The current instance of SettingsActivity in the resumed state, if any. */
    private static SettingsActivity sResumedInstance;

    /** Whether this activity has been created for the first time but not yet resumed. */
    private boolean mIsNewlyCreated;

    /**
     * Creates an Intent that will launch the root/full Site Settings UI, which displays a list of
     * settings categories.
     *
     * @param profileName The name of the profile.
     *
     * @since 89
     */
    public static Intent createIntentForSiteSettingsCategoryList(
            Context context, String profileName) {
        return SettingsIntentHelper.createIntentForSiteSettingsCategoryList(
                context, profileName, "".equals(profileName));
    }

    /**
     * Creates an Intent that will launch the root/full Site Settings UI, which displays a list of
     * settings categories.
     *
     * @param profileName The name of the profile.
     * @param isIncognito If the profile is incognito.
     *
     * @since 89
     */
    public static Intent createIntentForSiteSettingsCategoryList(
            Context context, String profileName, boolean isIncognito) {
        return SettingsIntentHelper.createIntentForSiteSettingsCategoryList(
                context, profileName, isIncognito);
    }

    /**
     * Creates an Intent that will launch Accessibility Settings UI.
     *
     * @param profileName The name of the profile.
     * @param isIncognito If the profile is incognito.
     * @return the Intent to start the settings.
     *
     * @since 100
     */
    public static Intent createIntentForAccessibilitySettings(
            Context context, String profileName, boolean isIncognito) {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 100) {
            throw new UnsupportedOperationException();
        }
        return SettingsIntentHelper.createIntentForAccessibilitySettings(
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
            SettingsFragment settingsFragment = new SettingsFragment();
            settingsFragment.setArguments(getIntent().getExtras());
            getSupportFragmentManager()
                    .beginTransaction()
                    .add(android.R.id.content, settingsFragment, FRAGMENT_TAG)
                    .commitNow();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        // Prevent the user from interacting with multiple instances of SettingsActivity at the
        // same time (e.g. in multi-instance mode on a Samsung device), which would cause many fun
        // bugs.
        if (sResumedInstance != null && sResumedInstance.getTaskId() != getTaskId()) {
            if (mIsNewlyCreated) {
                // This activity was newly created and takes precedence over sResumedInstance.
                sResumedInstance.finish();
            } else {
                // This activity was unpaused or recreated while another instance of
                // SettingsActivity was already showing. The existing instance takes precedence.
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

    /**
     * Records that the Activity export check has occurred, and returns whether this method has been
     * previously called or not.
     *
     * This method is overridden by subclasses to allow them to save their own static state, but
     * should be removed and inlined into ensureActivityNotExported() once SiteSettingsActivity is
     * removed (it was deprecated in M89).
     */
    protected boolean markActivityExportChecked() {
        boolean oldValue = sActivityNotExportedChecked;
        sActivityNotExportedChecked = true;
        return oldValue;
    }

    private void ensureActivityNotExported() {
        if (markActivityExportChecked()) return;
        try {
            ActivityInfo activityInfo = getPackageManager().getActivityInfo(getComponentName(), 0);
            // If SettingsActivity is exported, then it's vulnerable to a fragment injection
            // exploit:
            // http://securityintelligence.com/new-vulnerability-android-framework-fragment-injection
            if (activityInfo.exported) {
                throw new IllegalStateException("SettingsActivity must not be exported.");
            }
        } catch (NameNotFoundException ex) {
            // Something terribly wrong has happened.
            throw new RuntimeException(ex);
        }
    }
}
