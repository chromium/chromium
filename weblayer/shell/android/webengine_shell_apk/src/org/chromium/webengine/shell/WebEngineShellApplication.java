// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell;

import android.app.Application;

import androidx.core.content.ContextCompat;

import com.google.common.base.Function;
import com.google.common.util.concurrent.AsyncFunction;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;
import org.chromium.webengine.shell.topbar.TabEventsDelegate;

/**
 * Application for managing WebSandbox and WebEngine in the Demo Shell.
 */
public class WebEngineShellApplication extends Application {
    private ListenableFuture<WebEngine> mWebEngineFuture;
    private ListenableFuture<TabEventsDelegate> mTabEventsDelegateFuture;

    private TabEventsDelegate mTabEventsDelegate;

    public ListenableFuture<WebEngine> getWebEngine() {
        return mWebEngineFuture;
    }

    public ListenableFuture<TabEventsDelegate> getTabEventsDelegate() {
        if (mTabEventsDelegate != null) {
            return Futures.immediateFuture(mTabEventsDelegate);
        }

        if (mTabEventsDelegateFuture != null) {
            return mTabEventsDelegateFuture;
        }

        Function<WebEngine, TabEventsDelegate> getTabEventsDelegateTask =
                webEngine -> new TabEventsDelegate(webEngine.getTabManager());

        mTabEventsDelegateFuture = Futures.transform(
                getWebEngine(), getTabEventsDelegateTask, ContextCompat.getMainExecutor(this));

        return mTabEventsDelegateFuture;
    }

    @Override
    public void onCreate() {
        super.onCreate();

        AsyncFunction<WebSandbox, WebEngine> getWebEngineTask =
                webSandbox -> webSandbox.createWebEngine("shell-engine");
        mWebEngineFuture = Futures.transformAsync(
                WebSandbox.create(this), getWebEngineTask, ContextCompat.getMainExecutor(this));
        Futures.addCallback(mWebEngineFuture, new FutureCallback<WebEngine>() {
            @Override
            public void onSuccess(WebEngine webEngine) {
                mTabEventsDelegate = new TabEventsDelegate(webEngine.getTabManager());
            }
            @Override
            public void onFailure(Throwable thrown) {}
        }, ContextCompat.getMainExecutor(this));
    }
}
