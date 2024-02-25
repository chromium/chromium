// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.push_apps_to_background;

import android.app.Activity;
import android.os.Bundle;

/**
 * This activity is used in performance tests to push other apps
 * to the background while running automated user stories.
 */
public class PushAppsToBackgroundActivity extends Activity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_push_apps_to_background);
    }
}
