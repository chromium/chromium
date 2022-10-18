// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.shell;

import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.browserfragment.Browser;

/**
 * Activity for running instrumentation tests.
 */
public class InstrumentationActivity extends AppCompatActivity {
    private ListenableFuture<Browser> mBrowserFuture;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        mBrowserFuture = Browser.create(getApplicationContext());
    }

    public ListenableFuture<Browser> getBrowserFuture() {
        return mBrowserFuture;
    }
}