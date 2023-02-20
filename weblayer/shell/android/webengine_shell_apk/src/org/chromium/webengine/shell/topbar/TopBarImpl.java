// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell.topbar;

import android.content.Context;
import android.net.Uri;
import android.util.Patterns;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.webengine.Tab;
import org.chromium.webengine.TabManager;

/**
 * Sets the values in the Top Bar Views.
 */
public class TopBarImpl extends TopBar {
    private final Context mContext;
    private final TabManager mTabManager;
    private final EditText mUrlBar;
    private final ProgressBar mProgressBar;

    public TopBarImpl(
            Context context, TabManager tabManager, EditText urlBar, ProgressBar progressBar) {
        mContext = context;
        mTabManager = tabManager;
        mUrlBar = urlBar;
        mProgressBar = progressBar;

        urlBar.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                Uri query = Uri.parse(v.getText().toString());
                if (query.isAbsolute()) {
                    mTabManager.getActiveTab().getNavigationController().navigate(
                            query.normalizeScheme().toString());
                } else if (Patterns.DOMAIN_NAME.matcher(query.toString()).matches()) {
                    mTabManager.getActiveTab().getNavigationController().navigate(
                            "https://" + query);
                } else {
                    mTabManager.getActiveTab().getNavigationController().navigate(
                            "https://www.google.com/search?q="
                            + Uri.encode(v.getText().toString()));
                }
                // Hides keyboard on Enter key pressed
                InputMethodManager imm = (InputMethodManager) mContext.getSystemService(
                        Context.INPUT_METHOD_SERVICE);
                imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
                return true;
            }
        });
    }

    @Override
    public void setUrlBar(String uri) {
        mUrlBar.setText(uri);
    }

    @Override
    public void setProgress(double progress) {
        int progressValue = (int) Math.rint(progress * 100);
        if (progressValue != mProgressBar.getMax()) {
            mProgressBar.setVisibility(View.VISIBLE);
        } else {
            mProgressBar.setVisibility(View.INVISIBLE);
        }
        mProgressBar.setProgress(progressValue);
    }

    @Override
    public boolean isTabActive(Tab tab) {
        return mTabManager.getActiveTab() != null && mTabManager.getActiveTab().equals(tab);
    }
}
