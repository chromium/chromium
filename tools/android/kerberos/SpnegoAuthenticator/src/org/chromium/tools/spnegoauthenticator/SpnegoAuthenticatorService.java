// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.spnegoauthenticator;

import android.annotation.SuppressLint;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.Log;

/** Service exposing the dummy {@link SpnegoAuthenticator}. */
public class SpnegoAuthenticatorService extends Service {
    private static final String TAG = "tools_SpnegoAuth";

    @SuppressLint("StaticFieldLeak")
    private static SpnegoAuthenticator sAuthenticator;

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "Binding SpnegoAuthenticatorService");
        if (sAuthenticator == null) sAuthenticator = new SpnegoAuthenticator(this);
        return sAuthenticator.getIBinder();
    }
}
