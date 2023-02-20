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
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;

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
    private final Button mTabCountButton;
    private final Spinner mTabListSpinner;
    private final ArrayAdapter<TabWrapper> mTabListAdapter;

    public TopBarImpl(Context context, TabManager tabManager, EditText urlBar,
            ProgressBar progressBar, Button tabCountButton, Spinner tabListSpinner) {
        mContext = context;
        mTabManager = tabManager;
        mUrlBar = urlBar;
        mProgressBar = progressBar;

        mTabCountButton = tabCountButton;
        mTabCountButton.setText(String.valueOf(getTabsCount()));

        mTabListAdapter = new ArrayAdapter<TabWrapper>(
                context, android.R.layout.simple_spinner_dropdown_item);
        for (Tab t : mTabManager.getAllTabs()) {
            mTabListAdapter.add(new TabWrapper(t));
        }
        mTabListSpinner = tabListSpinner;
        mTabListSpinner.setAdapter(mTabListAdapter);

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

        mTabCountButton.setOnClickListener(v -> mTabListSpinner.performClick());

        mTabListSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
                mTabListAdapter.getItem(pos).getTab().setActive();
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
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
    public void addTabToList(Tab tab) {
        mTabCountButton.setText(String.valueOf(getTabsCount()));
        mTabListAdapter.add(new TabWrapper(tab));
    }

    @Override
    public void removeTabFromList(Tab tab) {
        mTabCountButton.setText(String.valueOf(getTabsCount()));
        for (int position = 0; position < mTabListAdapter.getCount(); ++position) {
            TabWrapper tabAdapter = mTabListAdapter.getItem(position);
            if (tabAdapter.getTab().equals(tab)) {
                mTabListAdapter.remove(tabAdapter);
                return;
            }
        }
    }

    @Override
    public void setTabListSelection(Tab tab) {
        for (int position = 0; position < mTabListAdapter.getCount(); ++position) {
            TabWrapper tabWrapper = mTabListAdapter.getItem(position);
            if (tabWrapper.getTab().equals(tab)) {
                mTabListSpinner.setSelection(mTabListAdapter.getPosition(tabWrapper));
                return;
            }
        }
    }

    @Override
    public boolean isTabActive(Tab tab) {
        return mTabManager.getActiveTab() != null && mTabManager.getActiveTab().equals(tab);
    }

    @Override
    public int getTabsCount() {
        return mTabManager.getAllTabs().size();
    }

    static class TabWrapper {
        final Tab mTab;
        public TabWrapper(Tab tab) {
            mTab = tab;
        }

        public Tab getTab() {
            return mTab;
        }

        @NonNull
        @Override
        public String toString() {
            return mTab.getDisplayUri().getAuthority() + mTab.getDisplayUri().getPath();
        }
    }
}
