// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentManager;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.WebFragment;
import org.chromium.webengine.WebSandbox;

/**
 * Activity for running instrumentation tests.
 */
public class InstrumentationActivity extends AppCompatActivity {
    private ListenableFuture<WebSandbox> mWebSandboxFuture;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        mWebSandboxFuture = WebSandbox.create(getApplicationContext());
    }

    public ListenableFuture<WebSandbox> getWebSandboxFuture() {
        return mWebSandboxFuture;
    }

    public void attachFragment(WebFragment fragment) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        fragmentManager.beginTransaction()
                .setReorderingAllowed(true)
                .add(R.id.fragment_container_view, fragment)
                .commitNow();
    }

    public void detachFragment(WebFragment fragment) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        fragmentManager.beginTransaction().setReorderingAllowed(true).remove(fragment).commitNow();
    }
}
