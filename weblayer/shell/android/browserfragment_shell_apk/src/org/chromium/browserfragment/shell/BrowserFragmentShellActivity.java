// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.shell;

import android.content.Context;
import android.os.Bundle;
import android.view.SurfaceView;

import androidx.appcompat.app.AppCompatActivity;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.browserfragment.Browser;

/**
 * Activity for managing the Demo Shell.
 */
public class BrowserFragmentShellActivity extends AppCompatActivity {
    private Context mContext;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.main);

        mContext = getApplicationContext();

        ListenableFuture<Browser> browserFuture = Browser.create(mContext);
        Futures.addCallback(browserFuture, new FutureCallback<Browser>() {
            @Override
            public void onSuccess(Browser browser) {
                onBrowserReady(browser);
            }

            @Override
            public void onFailure(Throwable thrown) {}
        }, mContext.getMainExecutor());
    }

    private void onBrowserReady(Browser browser) {
        // TODO(rayankans): Abstract this away within a Fragment in the browserfragment library.
        browser.attachViewHierarchy((SurfaceView) findViewById(R.id.surfaceView));
    }
}
