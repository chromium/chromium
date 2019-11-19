// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.RemoteException;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.ValueCallback;

import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of {@link IBrowser}.
 */
public class BrowserImpl extends IBrowser.Stub {
    private final ProfileImpl mProfile;
    private BrowserViewController mViewController;
    private FragmentWindowAndroid mWindowAndroid;
    private ArrayList<TabImpl> mTabs = new ArrayList<TabImpl>();
    private IBrowserClient mClient;

    public BrowserImpl(ProfileImpl profile, Bundle savedInstanceState) {
        mProfile = profile;
        // Restore tabs etc from savedInstanceState here.
    }

    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    public ViewGroup getViewAndroidDelegateContainerView() {
        return mViewController.getContentView();
    }

    public void onFragmentAttached(Context context, FragmentWindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        mViewController = new BrowserViewController(context, windowAndroid);
        TabImpl tab = new TabImpl(mProfile, windowAndroid);
        addTab(tab);
        boolean set_active_result = setActiveTab(tab);
        assert set_active_result;
    }

    public void onFragmentDetached() {
        destroy(); // For now we don't retain anything between detach and attach.
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (mWindowAndroid != null) {
            mWindowAndroid.onActivityResult(requestCode, resultCode, data);
        }
    }

    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (mWindowAndroid != null) {
            mWindowAndroid.handlePermissionResult(requestCode, permissions, grantResults);
        }
    }

    @Override
    public void setTopView(IObjectWrapper viewWrapper) {
        getViewController().setTopView(ObjectWrapper.unwrap(viewWrapper, View.class));
    }

    @Override
    public void setSupportsEmbedding(boolean enable, IObjectWrapper valueCallback) {
        getViewController().setSupportsEmbedding(enable,
                (ValueCallback<Boolean>) ObjectWrapper.unwrap(valueCallback, ValueCallback.class));
    }

    public BrowserViewController getViewController() {
        if (mViewController == null) {
            throw new RuntimeException("Currently Tab requires Activity context, so "
                    + "it exists only while BrowserFragment is attached to an Activity");
        }
        return mViewController;
    }

    @Override
    public IProfile getProfile() {
        return mProfile;
    }

    @Override
    public void addTab(ITab iTab) {
        TabImpl tab = (TabImpl) iTab;
        if (tab.getBrowser() == this) return;
        addTabImpl(tab);
    }

    private void addTabImpl(TabImpl tab) {
        // Null case is only during initial creation.
        if (tab.getBrowser() != this && tab.getBrowser() != null) {
            tab.getBrowser().detachTab(tab);
        }
        mTabs.add(tab);
        tab.attachToBrowser(this);
        try {
            if (mClient != null) mClient.onTabAdded(tab);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void detachTab(ITab iTab) {
        TabImpl tab = (TabImpl) iTab;
        if (tab.getBrowser() != this) return;
        if (getActiveTab() == tab) setActiveTab(null);
        mTabs.remove(tab);
        try {
            if (mClient != null) mClient.onTabRemoved(tab.getId());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        // This doesn't reset state on TabImpl as |browser| is either about to be
        // destroyed, or switching to a different fragment.
    }

    @Override
    public boolean setActiveTab(ITab controller) {
        TabImpl tab = (TabImpl) controller;
        if (tab != null && tab.getBrowser() != this) return false;
        mViewController.setActiveTab(tab);
        try {
            if (mClient != null) {
                mClient.onActiveTabChanged(tab != null ? tab.getId() : 0);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        return true;
    }

    public TabImpl getActiveTab() {
        return mViewController.getTab();
    }

    @Override
    public List getTabs() {
        return new ArrayList(mTabs);
    }

    @Override
    public int getActiveTabId() {
        return getActiveTab() != null ? getActiveTab().getId() : 0;
    }

    @Override
    public void setClient(IBrowserClient client) {
        mClient = client;
    }

    @Override
    public void destroyTab(ITab iTab) {
        detachTab(iTab);
        ((TabImpl) iTab).destroy();
    }

    public View getFragmentView() {
        return getViewController().getView();
    }

    public void destroy() {
        if (mViewController != null) {
            mViewController.destroy();
            for (TabImpl tab : mTabs) {
                tab.destroy();
            }
            mViewController = null;
        }
        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
            mWindowAndroid = null;
        }
    }
}
