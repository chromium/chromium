// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/**
 * Service running the browser process for a BrowserFragment outside of the hosting
 * application's process.
 */
public class BrowserSandboxService extends Service {
    private IBinder mBinder = new BrowserProcessBinder(this);

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
