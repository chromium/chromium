// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test.external.intents;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;

/**
 * Used by the external intents test. The problem is that the external intents
 * only be launched for packages outside of the package the tests are being
 * run from so this needs to be in a separate application.
 *
 * This exists to start the activity that called it.
 *
 * This activity will kill itself to clean up after it has been backgrounded.
 */
public class ExternalIntentsTestActivity extends Activity {
    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String returnTo = getIntent().getData().getQueryParameter("return_to");

        if (returnTo != null) {
            ComponentName componentCaller = ComponentName.unflattenFromString(returnTo);

            Intent intent = new Intent(Intent.ACTION_MAIN);
            intent.addCategory(Intent.CATEGORY_LAUNCHER);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.setComponent(componentCaller);
            intent.putExtra("LAUNCHED_EXTERNAL", true);

            // We are just returning back because the instrumentation activity is
            // set to single task.
            startActivity(intent);
        }
    }

    @Override
    protected void onPause() {
        // We want to clean up, but only when we're leaving to ensure that it
        // starts the previous activity with its intent.
        finish();
    }
}
