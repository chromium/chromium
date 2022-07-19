// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.view.SurfaceControlViewHost;
import android.view.WindowManager;
import android.webkit.WebView;

import org.chromium.browserfragment.interfaces.IBrowserSandboxCallback;
import org.chromium.browserfragment.interfaces.IBrowserSandboxService;

/**
 * Service running the browser process for a BrowserFragment outside of the hosting
 * application's process.
 */
public class BrowserSandboxService extends Service {
    private final Context mContext = this;

    private final IBrowserSandboxService.Stub mBinder = new IBrowserSandboxService.Stub() {
        @Override
        public void attachViewHierarchy(IBinder hostToken, IBrowserSandboxCallback callback) {
            new Handler(Looper.getMainLooper()).post(() -> {
                // TODO(rayankans): Attach ContentViewRenderView instead of a WebView after
                // WebLayer/BrowserProcess have been initialized.
                WebView webView = new WebView(mContext);

                WindowManager window =
                        (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
                SurfaceControlViewHost host =
                        new SurfaceControlViewHost(mContext, window.getDefaultDisplay(), hostToken);

                // TODO(rayankans): Use actual dimensions from the host app.
                host.setView(webView, window.getDefaultDisplay().getWidth(),
                        window.getDefaultDisplay().getHeight());
                try {
                    callback.onSurfacePackageReady(host.getSurfacePackage());
                } catch (RemoteException e) {
                }
                webView.loadUrl("https://google.com");
            });
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
