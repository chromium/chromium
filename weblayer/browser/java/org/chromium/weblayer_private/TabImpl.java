// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.view.KeyEvent;
import android.view.MotionEvent;
import android.webkit.ValueCallback;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Implementation of ITab.
 */
@JNINamespace("weblayer")
public final class TabImpl extends ITab.Stub {
    private static int sNextId = 1;
    private long mNativeTab;

    private ProfileImpl mProfile;
    private WebContents mWebContents;
    private TabCallbackProxy mTabCallbackProxy;
    private NavigationControllerImpl mNavigationController;
    private DownloadCallbackProxy mDownloadCallbackProxy;
    private ErrorPageCallbackProxy mErrorPageCallbackProxy;
    private FullscreenCallbackProxy mFullscreenCallbackProxy;
    private ViewAndroidDelegate mViewAndroidDelegate;
    // BrowserImpl this TabImpl is in. This is only null during creation.
    private BrowserImpl mBrowser;
    private NewTabCallbackProxy mNewTabCallbackProxy;
    private ITabClient mClient;
    private final int mId;

    private static class InternalAccessDelegateImpl
            implements ViewEventSink.InternalAccessDelegate {
        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return false;
        }

        @Override
        public void onScrollChanged(int lPix, int tPix, int oldlPix, int oldtPix) {}
    }

    public TabImpl(ProfileImpl profile, WindowAndroid windowAndroid) {
        mId = ++sNextId;
        init(profile, windowAndroid, TabImplJni.get().createTab(profile.getNativeProfile(), this));
    }

    /**
     * This constructor is called when the native side triggers creation of a TabImpl
     * (as happens with popups).
     */
    public TabImpl(ProfileImpl profile, WindowAndroid windowAndroid, long nativeTab) {
        mId = ++sNextId;
        TabImplJni.get().setJavaImpl(nativeTab, TabImpl.this);
        init(profile, windowAndroid, nativeTab);
    }

    private void init(ProfileImpl profile, WindowAndroid windowAndroid, long nativeTab) {
        mProfile = profile;
        mNativeTab = nativeTab;
        mWebContents = TabImplJni.get().getWebContents(mNativeTab, TabImpl.this);
        mViewAndroidDelegate = new ViewAndroidDelegate(null) {
            @Override
            public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
                BrowserViewController viewController = getViewController();
                if (viewController != null) {
                    viewController.onTopControlsChanged(topControlsOffsetY, topContentOffsetY);
                }
            }
        };
        mWebContents.initialize("", mViewAndroidDelegate, new InternalAccessDelegateImpl(),
                windowAndroid, WebContents.createDefaultInternalsHolder());
    }

    public ProfileImpl getProfile() {
        return mProfile;
    }

    public ITabClient getClient() {
        return mClient;
    }

    /**
     * Sets the BrowserImpl this TabImpl is contained in.
     */
    public void attachToBrowser(BrowserImpl browser) {
        mBrowser = browser;
        mWebContents.setTopLevelNativeWindow(browser.getWindowAndroid());
        mViewAndroidDelegate.setContainerView(browser.getViewAndroidDelegateContainerView());
        SelectionPopupController.fromWebContents(mWebContents)
                .setActionModeCallback(new ActionModeCallback(mWebContents));
    }

    public BrowserImpl getBrowser() {
        return mBrowser;
    }

    @Override
    public void setNewTabsEnabled(boolean enable) {
        if (enable && mNewTabCallbackProxy == null) {
            mNewTabCallbackProxy = new NewTabCallbackProxy(this);
        } else if (!enable && mNewTabCallbackProxy != null) {
            mNewTabCallbackProxy.destroy();
            mNewTabCallbackProxy = null;
        }
    }

    @Override
    public int getId() {
        return mId;
    }

    /**
     * Called when this TabImpl becomes the active TabImpl.
     */
    public void onDidGainActive(long topControlsContainerViewHandle) {
        // attachToFragment() must be called before activate().
        assert mBrowser != null;
        TabImplJni.get().setTopControlsContainerView(
                mNativeTab, TabImpl.this, topControlsContainerViewHandle);
        mWebContents.onShow();
    }
    /**
     * Called when this TabImpl is no longer the active TabImpl.
     */
    public void onDidLoseActive() {
        mWebContents.onHide();
        TabImplJni.get().setTopControlsContainerView(mNativeTab, TabImpl.this, 0);
    }

    public WebContents getWebContents() {
        return mWebContents;
    }

    long getNativeTab() {
        return mNativeTab;
    }

    @Override
    public NavigationControllerImpl createNavigationController(INavigationControllerClient client) {
        // This should only be called once.
        assert mNavigationController == null;
        mNavigationController = new NavigationControllerImpl(this, client);
        return mNavigationController;
    }

    @Override
    public void setClient(ITabClient client) {
        mClient = client;
        mTabCallbackProxy = new TabCallbackProxy(mNativeTab, client);
    }

    @Override
    public void setDownloadCallbackClient(IDownloadCallbackClient client) {
        if (client != null) {
            if (mDownloadCallbackProxy == null) {
                mDownloadCallbackProxy = new DownloadCallbackProxy(mNativeTab, client);
            } else {
                mDownloadCallbackProxy.setClient(client);
            }
        } else if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }
    }

    @Override
    public void setErrorPageCallbackClient(IErrorPageCallbackClient client) {
        if (client != null) {
            if (mErrorPageCallbackProxy == null) {
                mErrorPageCallbackProxy = new ErrorPageCallbackProxy(mNativeTab, client);
            } else {
                mErrorPageCallbackProxy.setClient(client);
            }
        } else if (mErrorPageCallbackProxy != null) {
            mErrorPageCallbackProxy.destroy();
            mErrorPageCallbackProxy = null;
        }
    }

    @Override
    public void setFullscreenCallbackClient(IFullscreenCallbackClient client) {
        if (client != null) {
            if (mFullscreenCallbackProxy == null) {
                mFullscreenCallbackProxy = new FullscreenCallbackProxy(mNativeTab, client);
            } else {
                mFullscreenCallbackProxy.setClient(client);
            }
        } else if (mFullscreenCallbackProxy != null) {
            mFullscreenCallbackProxy.destroy();
            mFullscreenCallbackProxy = null;
        }
    }

    @Override
    public void executeScript(String script, boolean useSeparateIsolate, IObjectWrapper callback) {
        Callback<String> nativeCallback = new Callback<String>() {
            @Override
            public void onResult(String result) {
                ValueCallback<String> unwrappedCallback =
                        (ValueCallback<String>) ObjectWrapper.unwrap(callback, ValueCallback.class);
                if (unwrappedCallback != null) {
                    unwrappedCallback.onReceiveValue(result);
                }
            }
        };
        TabImplJni.get().executeScript(mNativeTab, script, useSeparateIsolate, nativeCallback);
    }

    public void destroy() {
        if (mTabCallbackProxy != null) {
            mTabCallbackProxy.destroy();
            mTabCallbackProxy = null;
        }
        if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }
        if (mErrorPageCallbackProxy != null) {
            mErrorPageCallbackProxy.destroy();
            mErrorPageCallbackProxy = null;
        }
        if (mFullscreenCallbackProxy != null) {
            mFullscreenCallbackProxy.destroy();
            mFullscreenCallbackProxy = null;
        }
        if (mNewTabCallbackProxy != null) {
            mNewTabCallbackProxy.destroy();
            mNewTabCallbackProxy = null;
        }
        mNavigationController = null;
        TabImplJni.get().deleteTab(mNativeTab);
        mNativeTab = 0;
    }

    @CalledByNative
    private boolean doBrowserControlsShrinkRendererSize() {
        BrowserViewController viewController = getViewController();
        return viewController != null && viewController.doBrowserControlsShrinkRendererSize();
    }

    /**
     * Returns the BrowserViewController for this TabImpl, but only if this
     * is the active TabImpl.
     */
    private BrowserViewController getViewController() {
        return (mBrowser.getActiveTab() == this) ? mBrowser.getViewController() : null;
    }

    @NativeMethods
    interface Natives {
        long createTab(long profile, TabImpl caller);
        void setJavaImpl(long nativeTabImpl, TabImpl impl);
        void setTopControlsContainerView(
                long nativeTabImpl, TabImpl caller, long nativeTopControlsContainerView);
        void deleteTab(long tab);
        WebContents getWebContents(long nativeTabImpl, TabImpl caller);
        void executeScript(long nativeTabImpl, String script, boolean useSeparateIsolate,
                Callback<String> callback);
    }
}
