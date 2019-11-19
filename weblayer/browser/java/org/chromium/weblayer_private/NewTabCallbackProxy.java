// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.NewTabType;

/**
 * Owns the c++ NewTabCallback class, which is responsible for forwarding all
 * NewTabCallback calls to this class, which in turn forwards to ITabClient.
 */
@JNINamespace("weblayer")
public final class NewTabCallbackProxy {
    private long mNativeNewTabCallbackProxy;
    private final TabImpl mTab;

    public NewTabCallbackProxy(TabImpl tab) {
        mTab = tab;
        mNativeNewTabCallbackProxy =
                NewTabCallbackProxyJni.get().createNewTabCallbackProxy(this, tab.getNativeTab());
    }

    public void destroy() {
        NewTabCallbackProxyJni.get().deleteNewTabCallbackProxy(mNativeNewTabCallbackProxy);
        mNativeNewTabCallbackProxy = 0;
    }

    @NewTabType
    private static int implTypeToJavaType(@ImplNewTabType int type) {
        switch (type) {
            case ImplNewTabType.FOREGROUND:
                return NewTabType.FOREGROUND_TAB;
            case ImplNewTabType.BACKGROUND:
                return NewTabType.BACKGROUND_TAB;
            case ImplNewTabType.NEW_POPUP:
                return NewTabType.NEW_POPUP;
            case ImplNewTabType.NEW_WINDOW:
                return NewTabType.NEW_WINDOW;
        }
        assert false;
        return NewTabType.FOREGROUND_TAB;
    }

    @CalledByNative
    public void onNewTab(long nativeTab, @ImplNewTabType int mode) throws RemoteException {
        // This class should only be created while the tab is attached to a fragment.
        assert mTab.getBrowser() != null;
        TabImpl tab =
                new TabImpl(mTab.getProfile(), mTab.getBrowser().getWindowAndroid(), nativeTab);
        mTab.getBrowser().addTab(tab);
        mTab.getClient().onNewTab(tab.getId(), mode);
    }

    @CalledByNative
    private void onCloseTab() throws RemoteException {
        mTab.getClient().onCloseTab();
    }

    @NativeMethods
    interface Natives {
        long createNewTabCallbackProxy(NewTabCallbackProxy proxy, long tab);
        void deleteNewTabCallbackProxy(long proxy);
    }
}
