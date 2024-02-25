// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabs.test;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ProgressBar;

/** Very basic WebView based activity for benchmarking. */
public class WebViewActivity extends Activity {
    private final Handler mHandler = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_web_view);

        mHandler.post(
                new Runnable() {
                    @Override
                    public void run() {
                        go();
                    }
                });
    }

    private void go() {
        String url = getIntent().getData().toString();
        final long intentSentMs = getIntent().getLongExtra(MainActivity.INTENT_SENT_EXTRA, -1);

        WebView webView = (WebView) findViewById(R.id.webview);
        final ProgressBar progressBar = (ProgressBar) findViewById(R.id.progress_bar);

        setTitle(url);

        webView.setVisibility(View.VISIBLE);
        webView.getSettings().setJavaScriptEnabled(true);
        webView.setWebViewClient(
                new WebViewClient() {
                    private long mPageStartedOffsetMs = -1;

                    @Override
                    public boolean shouldOverrideUrlLoading(WebView view, String url) {
                        return false;
                    }

                    @Override
                    public void onPageStarted(WebView view, String url, Bitmap favicon) {
                        long offsetMs = MainActivity.now() - intentSentMs;
                        Log.w(
                                MainActivity.TAG,
                                "navigationStarted = " + offsetMs + " url = " + url);
                        // Can be called several times (redirects).
                        if (mPageStartedOffsetMs == -1) mPageStartedOffsetMs = offsetMs;
                    }

                    @Override
                    public void onPageFinished(WebView view, String url) {
                        long offsetMs = MainActivity.now() - intentSentMs;
                        Log.w(MainActivity.TAG, "navigationFinished = " + offsetMs);
                        Log.w(MainActivity.TAG, "WEBVIEW," + mPageStartedOffsetMs + "," + offsetMs);
                    }
                });
        webView.setWebChromeClient(
                new WebChromeClient() {
                    @Override
                    public void onProgressChanged(WebView view, int newProgress) {
                        if (progressBar.getVisibility() == View.GONE) {
                            progressBar.setVisibility(View.VISIBLE);
                        }
                        progressBar.setProgress(newProgress);
                    }
                });

        webView.loadUrl(url);
    }
}
