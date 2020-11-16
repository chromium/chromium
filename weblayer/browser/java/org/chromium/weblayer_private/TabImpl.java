// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.RectF;
import android.os.Build;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.AndroidRuntimeException;
import android.util.Pair;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;
import android.webkit.ValueCallback;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill.AutofillActionModeCallback;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.components.browser_ui.media.MediaSessionHelper;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.find_in_page.FindInPageBridge;
import org.chromium.components.find_in_page.FindMatchRectsDetails;
import org.chromium.components.find_in_page.FindResultBar;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListenerWithScroll;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFaviconFetcher;
import org.chromium.weblayer_private.interfaces.IFaviconFetcherClient;
import org.chromium.weblayer_private.interfaces.IFindInPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.IGoogleAccountsCallbackClient;
import org.chromium.weblayer_private.interfaces.IMediaCaptureCallbackClient;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.IWebMessageCallbackClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.ScrollNotificationType;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;
import org.chromium.weblayer_private.media.MediaSessionManager;
import org.chromium.weblayer_private.media.MediaStreamManager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Implementation of ITab.
 */
@JNINamespace("weblayer")
public final class TabImpl extends ITab.Stub {
    private static int sNextId = 1;
    // Map from id to TabImpl.
    private static final Map<Integer, TabImpl> sTabMap = new HashMap<Integer, TabImpl>();
    private long mNativeTab;

    private ProfileImpl mProfile;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private TabCallbackProxy mTabCallbackProxy;
    private NavigationControllerImpl mNavigationController;
    private ErrorPageCallbackProxy mErrorPageCallbackProxy;
    private FullscreenCallbackProxy mFullscreenCallbackProxy;
    private TabViewAndroidDelegate mViewAndroidDelegate;
    private GoogleAccountsCallbackProxy mGoogleAccountsCallbackProxy;
    // BrowserImpl this TabImpl is in. This is null before attached to a Browser. While this is null
    // before attached, there are code paths that may trigger calling methods before set.
    @Nullable
    private BrowserImpl mBrowser;
    /**
     * The AutofillProvider that integrates with system-level autofill. This is null until
     * updateFromBrowser() is invoked.
     */
    private AutofillProvider mAutofillProvider;
    private MediaStreamManager mMediaStreamManager;
    private NewTabCallbackProxy mNewTabCallbackProxy;
    private ITabClient mClient;
    private final int mId;

    // A list of browser control visibility constraints, indexed by ImplControlsVisibilityReason.
    private List<BrowserControlsVisibilityDelegate> mBrowserControlsDelegates;
    // Computes a net browser control visibility constraint from constituent constraints.
    private ComposedBrowserControlsVisibilityDelegate mComposedBrowserControlsVisibility;
    // Which BrowserControlsVisibilityDelegate is currently controlling the visibility. The active
    // delegate changes from mComposedBrowserControlsVisibility to the delegate for visibility
    // reason RENDERER_UNAVAILABLE if onlyExpandControlsAtPageTop is enabled, in which case we don't
    // want to ever force the controls to be visible unless the renderer isn't responsive.
    private BrowserControlsVisibilityDelegate mActiveBrowserControlsVisibilityDelegate;
    // Invoked when the computed visibility constraint changes.
    private Callback<Integer> mConstraintsUpdatedCallback;

    private IFindInPageCallbackClient mFindInPageCallbackClient;
    private FindInPageBridge mFindInPageBridge;
    private FindResultBar mFindResultBar;
    // See usage note in {@link #onFindResultAvailable}.
    private boolean mWaitingForMatchRects;
    private InterceptNavigationDelegateClientImpl mInterceptNavigationDelegateClient;
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;
    private InfoBarContainer mInfoBarContainer;
    private MediaSessionHelper mMediaSessionHelper;
    private DisplayCutoutController mDisplayCutoutController;

    private boolean mPostContainerViewInitDone;

    private WebLayerAccessibilityUtil.Observer mAccessibilityObserver;

    private Set<FaviconCallbackProxy> mFaviconCallbackProxies = new HashSet<>();

    // Only non-null if scroll offsets have been requested.
    private @Nullable GestureStateListenerWithScroll mGestureStateListenerWithScroll;

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

    private class TabViewAndroidDelegate extends ViewAndroidDelegate {
        private boolean mIgnoreRenderer;

        TabViewAndroidDelegate() {
            super(null);
        }

        /**
         * Causes {@link onTopControlsChanged()} and {@link onBottomControlsChanged()} to be
         * ignored.
         * @param ignoreRenderer whether to ignore renderer-initiated updates to the controls state.
         */
        public void setIgnoreRendererUpdates(boolean ignoreRenderer) {
            mIgnoreRenderer = ignoreRenderer;
        }

        @Override
        public void onTopControlsChanged(
                int topControlsOffsetY, int topContentOffsetY, int topControlsMinHeightOffsetY) {
            BrowserViewController viewController = getViewController();
            if (viewController != null && !mIgnoreRenderer) {
                viewController.onTopControlsChanged(topControlsOffsetY, topContentOffsetY);
            }
        }
        @Override
        public void onBottomControlsChanged(
                int bottomControlsOffsetY, int bottomControlsMinHeightOffsetY) {
            BrowserViewController viewController = getViewController();
            if (viewController != null && !mIgnoreRenderer) {
                viewController.onBottomControlsChanged(bottomControlsOffsetY);
            }
        }

        @Override
        public void onBackgroundColorChanged(int color) {
            if (WebLayerFactoryImpl.getClientMajorVersion() >= 85) {
                try {
                    mClient.onBackgroundColorChanged(color);
                } catch (RemoteException e) {
                    throw new APICallException(e);
                }
            }
        }

        @Override
        protected void onVerticalScrollDirectionChanged(
                boolean directionUp, float currentScrollRatio) {
            if (WebLayerFactoryImpl.getClientMajorVersion() >= 85) {
                try {
                    mClient.onScrollNotification(directionUp
                                    ? ScrollNotificationType.DIRECTION_CHANGED_UP
                                    : ScrollNotificationType.DIRECTION_CHANGED_DOWN,
                            currentScrollRatio);
                } catch (RemoteException e) {
                    throw new APICallException(e);
                }
            }
        }
    }

    public static TabImpl fromWebContents(WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) return null;
        return TabImplJni.get().fromWebContents(webContents);
    }

    public static TabImpl getTabById(int tabId) {
        return sTabMap.get(tabId);
    }

    public TabImpl(ProfileImpl profile, WindowAndroid windowAndroid) {
        mId = ++sNextId;
        init(profile, windowAndroid, TabImplJni.get().createTab(profile.getNativeProfile(), this));
    }

    /**
     * This constructor is called when the native side triggers creation of a TabImpl
     * (as happens with popups and other scenarios).
     */
    public TabImpl(ProfileImpl profile, WindowAndroid windowAndroid, long nativeTab) {
        mId = ++sNextId;
        TabImplJni.get().setJavaImpl(nativeTab, TabImpl.this);
        init(profile, windowAndroid, nativeTab);
    }

    private void init(ProfileImpl profile, WindowAndroid windowAndroid, long nativeTab) {
        mProfile = profile;
        mNativeTab = nativeTab;
        mWebContents = TabImplJni.get().getWebContents(mNativeTab);
        mViewAndroidDelegate = new TabViewAndroidDelegate();
        mWebContents.initialize("", mViewAndroidDelegate, new InternalAccessDelegateImpl(),
                windowAndroid, WebContents.createDefaultInternalsHolder());

        mWebContentsObserver = new WebContentsObserver() {
            @Override
            public void didStartNavigation(NavigationHandle navigationHandle) {
                if (navigationHandle.isInMainFrame() && !navigationHandle.isSameDocument()) {
                    hideFindInPageUiAndNotifyClient();
                }
            }
            @Override
            public void viewportFitChanged(@WebContentsObserver.ViewportFitType int value) {
                ensureDisplayCutoutController();
                mDisplayCutoutController.setViewportFit(value);
            }
        };
        mWebContents.addObserver(mWebContentsObserver);

        mMediaStreamManager = new MediaStreamManager(this);

        mBrowserControlsDelegates = new ArrayList<BrowserControlsVisibilityDelegate>();
        mComposedBrowserControlsVisibility = new ComposedBrowserControlsVisibilityDelegate();
        for (int i = 0; i < ImplControlsVisibilityReason.REASON_COUNT; ++i) {
            BrowserControlsVisibilityDelegate delegate =
                    new BrowserControlsVisibilityDelegate(BrowserControlsState.BOTH);
            mBrowserControlsDelegates.add(delegate);
            mComposedBrowserControlsVisibility.addDelegate(delegate);
        }
        mConstraintsUpdatedCallback =
                (constraint) -> onBrowserControlsConstraintUpdated(constraint);
        mActiveBrowserControlsVisibilityDelegate = mComposedBrowserControlsVisibility;
        mActiveBrowserControlsVisibilityDelegate.addObserver(mConstraintsUpdatedCallback);

        mInterceptNavigationDelegateClient = new InterceptNavigationDelegateClientImpl(this);
        mInterceptNavigationDelegate =
                new InterceptNavigationDelegateImpl(mInterceptNavigationDelegateClient);
        mInterceptNavigationDelegateClient.initializeWithDelegate(mInterceptNavigationDelegate);
        sTabMap.put(mId, this);

        mInfoBarContainer = new InfoBarContainer(this);
        mAccessibilityObserver = (boolean enabled) -> {
            setBrowserControlsVisibilityConstraint(ImplControlsVisibilityReason.ACCESSIBILITY,
                    enabled ? BrowserControlsState.SHOWN : BrowserControlsState.BOTH);
        };
        // addObserver() calls to observer when added.
        WebLayerAccessibilityUtil.get().addObserver(mAccessibilityObserver);

        // MediaSession only works if the client is new enough. Sadly, passing
        // kDisableMediaSessionAPI does not fully disable the API, so a check is also necessary
        // before installing this observer.
        if (WebLayerFactoryImpl.getClientMajorVersion() >= 85) {
            mMediaSessionHelper = new MediaSessionHelper(
                    mWebContents, MediaSessionManager.createMediaSessionHelperDelegate(mId));
        }
    }

    private void doInitAfterSettingContainerView() {
        if (mPostContainerViewInitDone) return;

        mPostContainerViewInitDone = true;
        SelectionPopupController controller =
                SelectionPopupController.fromWebContents(mWebContents);
        controller.setActionModeCallback(new ActionModeCallback(mWebContents));
        controller.setSelectionClient(SelectionClient.createSmartSelectionClient(mWebContents));
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
        updateFromBrowser();
    }

    public void updateFromBrowser() {
        mWebContents.setTopLevelNativeWindow(mBrowser.getWindowAndroid());
        mViewAndroidDelegate.setContainerView(mBrowser.getViewAndroidDelegateContainerView());
        doInitAfterSettingContainerView();
        updateViewAttachedStateFromBrowser();

        boolean attached = (mBrowser.getContext() != null);
        mInterceptNavigationDelegateClient.onActivityAttachmentChanged(attached);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            SelectionPopupController selectionController =
                    SelectionPopupController.fromWebContents(mWebContents);
            if (mBrowser.getContext() == null) {
                // The Context and ViewContainer in which Autofill was previously operating have
                // gone away, so tear down |mAutofillProvider|.
                mAutofillProvider = null;
                TabImplJni.get().onAutofillProviderChanged(mNativeTab, null);
                selectionController.setNonSelectionActionModeCallback(null);
            } else {
                if (mAutofillProvider == null) {
                    // Set up |mAutofillProvider| to operate in the new Context. It's safe to assume
                    // the context won't change unless it is first nulled out, since the fragment
                    // must be detached before it can be reattached to a new Context.
                    mAutofillProvider = new AutofillProvider(mBrowser.getContext(),
                            mBrowser.getViewAndroidDelegateContainerView(), "WebLayer");
                    TabImplJni.get().onAutofillProviderChanged(mNativeTab, mAutofillProvider);
                }
                mAutofillProvider.onContainerViewChanged(
                        mBrowser.getViewAndroidDelegateContainerView());
                mAutofillProvider.setWebContents(mWebContents);

                selectionController.setNonSelectionActionModeCallback(
                        new AutofillActionModeCallback(mBrowser.getContext(), mAutofillProvider));
            }
        }
    }

    public void updateViewAttachedStateFromBrowser() {
        updateWebContentsVisibility();
        updateDisplayCutoutController();
    }

    public void onProvideAutofillVirtualStructure(ViewStructure structure, int flags) {
        if (mAutofillProvider == null) return;
        mAutofillProvider.onProvideAutoFillVirtualStructure(structure, flags);
    }

    public void autofill(final SparseArray<AutofillValue> values) {
        if (mAutofillProvider == null) return;
        mAutofillProvider.autofill(values);
    }

    public BrowserImpl getBrowser() {
        return mBrowser;
    }

    @Override
    public void setNewTabsEnabled(boolean enable) {
        StrictModeWorkaround.apply();
        if (enable && mNewTabCallbackProxy == null) {
            mNewTabCallbackProxy = new NewTabCallbackProxy(this);
        } else if (!enable && mNewTabCallbackProxy != null) {
            mNewTabCallbackProxy.destroy();
            mNewTabCallbackProxy = null;
        }
    }

    @Override
    public int getId() {
        StrictModeWorkaround.apply();
        return mId;
    }

    /**
     * Called when this TabImpl is attached to the BrowserViewController.
     */
    public void onAttachedToViewController(
            long topControlsContainerViewHandle, long bottomControlsContainerViewHandle) {
        // attachToFragment() must be called before activate().
        assert mBrowser != null;
        TabImplJni.get().setBrowserControlsContainerViews(
                mNativeTab, topControlsContainerViewHandle, bottomControlsContainerViewHandle);
        mInfoBarContainer.onTabAttachedToViewController();
        updateWebContentsVisibility();
        updateDisplayCutoutController();
    }

    /**
     * Called when this TabImpl is detached from the BrowserViewController.
     */
    public void onDetachedFromViewController() {
        if (mAutofillProvider != null) {
            mAutofillProvider.hidePopup();
        }

        if (mFullscreenCallbackProxy != null) mFullscreenCallbackProxy.destroyToast();

        hideFindInPageUiAndNotifyClient();
        updateWebContentsVisibility();
        updateDisplayCutoutController();

        // This method is called as part of the final phase of TabImpl destruction, at which
        // point mInfoBarContainer has already been destroyed.
        if (mInfoBarContainer != null) {
            mInfoBarContainer.onTabDetachedFromViewController();
        }

        TabImplJni.get().setBrowserControlsContainerViews(mNativeTab, 0, 0);
    }

    /**
     * Returns whether this Tab is visible.
     */
    public boolean isVisible() {
        return isActiveTab()
                && ((mBrowser.isStarted() && mBrowser.isViewAttachedToWindow())
                        || mBrowser.isFragmentStoppedForConfigurationChange());
    }

    @CalledByNative
    public boolean willAutomaticallyReloadAfterCrashImpl() {
        return !isVisible();
    }

    @Override
    public boolean willAutomaticallyReloadAfterCrash() {
        StrictModeWorkaround.apply();
        return willAutomaticallyReloadAfterCrashImpl();
    }

    public boolean isActiveTab() {
        return mBrowser != null && mBrowser.getActiveTab() == this;
    }

    private void updateWebContentsVisibility() {
        boolean visibleNow = isVisible();
        boolean webContentsVisible = mWebContents.getVisibility() == Visibility.VISIBLE;
        if (visibleNow) {
            if (!webContentsVisible) mWebContents.onShow();
        } else {
            if (webContentsVisible) mWebContents.onHide();
        }
    }

    private void updateDisplayCutoutController() {
        if (mDisplayCutoutController == null) return;

        mDisplayCutoutController.onActivityAttachmentChanged(mBrowser.getWindowAndroid());
        mDisplayCutoutController.maybeUpdateLayout();
    }

    public void loadUrl(LoadUrlParams loadUrlParams) {
        String url = loadUrlParams.getUrl();
        if (url == null || url.isEmpty()) return;

        GURL fixedUrl = UrlFormatter.fixupUrl(url);
        if (!fixedUrl.isValid()) return;

        loadUrlParams.setUrl(fixedUrl.getSpec());
        getWebContents().getNavigationController().loadUrl(loadUrlParams);
    }

    public WebContents getWebContents() {
        return mWebContents;
    }

    // Public for tests.
    @VisibleForTesting
    public long getNativeTab() {
        return mNativeTab;
    }

    @VisibleForTesting
    public InfoBarContainer getInfoBarContainerForTesting() {
        return mInfoBarContainer;
    }

    @Override
    public NavigationControllerImpl createNavigationController(INavigationControllerClient client) {
        StrictModeWorkaround.apply();
        // This should only be called once.
        assert mNavigationController == null;
        mNavigationController = new NavigationControllerImpl(this, client);
        return mNavigationController;
    }

    @Override
    public void setClient(ITabClient client) {
        StrictModeWorkaround.apply();
        mClient = client;
        mTabCallbackProxy = new TabCallbackProxy(mNativeTab, client);
    }

    @Override
    public void setDownloadCallbackClient(IDownloadCallbackClient client) {
        StrictModeWorkaround.apply();
        mProfile.setDownloadCallbackClient(client);
    }

    @Override
    public void setErrorPageCallbackClient(IErrorPageCallbackClient client) {
        StrictModeWorkaround.apply();
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
        StrictModeWorkaround.apply();
        if (client != null) {
            if (mFullscreenCallbackProxy == null) {
                mFullscreenCallbackProxy = new FullscreenCallbackProxy(this, mNativeTab, client);
            } else {
                mFullscreenCallbackProxy.setClient(client);
            }
        } else if (mFullscreenCallbackProxy != null) {
            mFullscreenCallbackProxy.destroy();
            mFullscreenCallbackProxy = null;
        }
    }

    @Override
    public void setGoogleAccountsCallbackClient(IGoogleAccountsCallbackClient client) {
        StrictModeWorkaround.apply();
        if (client != null) {
            if (mGoogleAccountsCallbackProxy == null) {
                mGoogleAccountsCallbackProxy = new GoogleAccountsCallbackProxy(mNativeTab, client);
            } else {
                mGoogleAccountsCallbackProxy.setClient(client);
            }
        } else if (mGoogleAccountsCallbackProxy != null) {
            mGoogleAccountsCallbackProxy.destroy();
            mGoogleAccountsCallbackProxy = null;
        }
    }

    public GoogleAccountsCallbackProxy getGoogleAccountsCallbackProxy() {
        return mGoogleAccountsCallbackProxy;
    }

    @Override
    public IFaviconFetcher createFaviconFetcher(IFaviconFetcherClient client) {
        StrictModeWorkaround.apply();
        FaviconCallbackProxy proxy = new FaviconCallbackProxy(this, mNativeTab, client);
        mFaviconCallbackProxies.add(proxy);
        return proxy;
    }

    @Override
    public void setTranslateTargetLanguage(String targetLanguage) {
        TabImplJni.get().setTranslateTargetLanguage(mNativeTab, targetLanguage);
    }

    @Override
    public void setScrollOffsetsEnabled(boolean enabled) {
        if (enabled) {
            if (mGestureStateListenerWithScroll == null) {
                mGestureStateListenerWithScroll = new GestureStateListenerWithScroll() {
                    @Override
                    public void onScrollOffsetOrExtentChanged(
                            int scrollOffsetY, int scrollExtentY) {
                        try {
                            mClient.onVerticalScrollOffsetChanged(scrollOffsetY);
                        } catch (RemoteException e) {
                            throw new APICallException(e);
                        }
                    }
                };
                GestureListenerManager.fromWebContents(mWebContents)
                        .addListener(mGestureStateListenerWithScroll);
            }
        } else if (mGestureStateListenerWithScroll != null) {
            GestureListenerManager.fromWebContents(mWebContents)
                    .removeListener(mGestureStateListenerWithScroll);
            mGestureStateListenerWithScroll = null;
        }
    }

    public void removeFaviconCallbackProxy(FaviconCallbackProxy proxy) {
        mFaviconCallbackProxies.remove(proxy);
    }

    @Override
    public void executeScript(String script, boolean useSeparateIsolate, IObjectWrapper callback) {
        StrictModeWorkaround.apply();
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

    @Override
    public boolean setFindInPageCallbackClient(IFindInPageCallbackClient client) {
        StrictModeWorkaround.apply();
        if (client == null) {
            // Null now to avoid calling onFindEnded.
            mFindInPageCallbackClient = null;
            hideFindInPageUiAndNotifyClient();
            return true;
        }

        if (mFindInPageCallbackClient != null) return false;

        BrowserViewController controller = getViewController();
        if (controller == null) return false;

        // Refuse to start a find session when the browser controls are forced hidden.
        if (mActiveBrowserControlsVisibilityDelegate.get() == BrowserControlsState.HIDDEN) {
            return false;
        }

        setBrowserControlsVisibilityConstraint(
                ImplControlsVisibilityReason.FIND_IN_PAGE, BrowserControlsState.SHOWN);

        mFindInPageCallbackClient = client;
        assert mFindInPageBridge == null;
        mFindInPageBridge = new FindInPageBridge(mWebContents);
        assert mFindResultBar == null;
        mFindResultBar =
                new FindResultBar(mBrowser.getContext(), controller.getWebContentsOverlayView(),
                        mBrowser.getWindowAndroid(), mFindInPageBridge);
        return true;
    }

    @Override
    public void findInPage(String searchText, boolean forward) {
        StrictModeWorkaround.apply();
        if (mFindInPageBridge == null) return;

        if (searchText.length() > 0) {
            mFindInPageBridge.startFinding(searchText, forward, false);
        } else {
            mFindInPageBridge.stopFinding(true);
        }
    }

    private void hideFindInPageUiAndNotifyClient() {
        if (mFindInPageBridge == null) return;
        mFindInPageBridge.stopFinding(true);

        mFindResultBar.dismiss();
        mFindResultBar = null;
        mFindInPageBridge.destroy();
        mFindInPageBridge = null;

        setBrowserControlsVisibilityConstraint(
                ImplControlsVisibilityReason.FIND_IN_PAGE, BrowserControlsState.BOTH);

        try {
            if (mFindInPageCallbackClient != null) mFindInPageCallbackClient.onFindEnded();
            mFindInPageCallbackClient = null;
        } catch (RemoteException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    @Override
    public void dispatchBeforeUnloadAndClose() {
        StrictModeWorkaround.apply();
        mWebContents.dispatchBeforeUnload(false);
    }

    @Override
    public boolean dismissTransientUi() {
        StrictModeWorkaround.apply();
        BrowserViewController viewController = getViewController();
        if (viewController != null && viewController.dismissTabModalOverlay()) return true;

        if (mWebContents.isFullscreenForCurrentTab()) {
            mWebContents.exitFullscreen();
            return true;
        }

        SelectionPopupController popup = SelectionPopupController.fromWebContents(mWebContents);
        if (popup != null && popup.isSelectActionBarShowing()) {
            popup.clearSelection();
            return true;
        }

        return false;
    }

    @Override
    public String getGuid() {
        StrictModeWorkaround.apply();
        return TabImplJni.get().getGuid(mNativeTab);
    }

    @Override
    public boolean setData(Map data) {
        StrictModeWorkaround.apply();
        String[] flattenedMap = new String[data.size() * 2];
        int i = 0;
        for (Map.Entry<String, String> entry : ((Map<String, String>) data).entrySet()) {
            flattenedMap[i++] = entry.getKey();
            flattenedMap[i++] = entry.getValue();
        }
        return TabImplJni.get().setData(mNativeTab, flattenedMap);
    }

    @Override
    public Map getData() {
        StrictModeWorkaround.apply();
        String[] data = TabImplJni.get().getData(mNativeTab);
        Map<String, String> map = new HashMap<>();
        for (int i = 0; i < data.length; i += 2) {
            map.put(data[i], data[i + 1]);
        }
        return map;
    }

    @Override
    public void captureScreenShot(float scale, IObjectWrapper valueCallback) {
        StrictModeWorkaround.apply();
        ValueCallback<Pair<Bitmap, Integer>> unwrappedCallback =
                (ValueCallback<Pair<Bitmap, Integer>>) ObjectWrapper.unwrap(
                        valueCallback, ValueCallback.class);
        TabImplJni.get().captureScreenShot(mNativeTab, scale, unwrappedCallback);
    }

    @Override
    public boolean canTranslate() {
        StrictModeWorkaround.apply();
        return TabImplJni.get().canTranslate(mNativeTab);
    }

    @Override
    public void showTranslateUi() {
        StrictModeWorkaround.apply();
        TabImplJni.get().showTranslateUi(mNativeTab);
    }

    @CalledByNative
    private static void runCaptureScreenShotCallback(
            ValueCallback<Pair<Bitmap, Integer>> callback, Bitmap bitmap, int errorCode) {
        callback.onReceiveValue(Pair.create(bitmap, errorCode));
    }

    @CalledByNative
    private static RectF createRectF(float x, float y, float right, float bottom) {
        return new RectF(x, y, right, bottom);
    }

    @CalledByNative
    private static FindMatchRectsDetails createFindMatchRectsDetails(
            int version, int numRects, RectF activeRect) {
        return new FindMatchRectsDetails(version, numRects, activeRect);
    }

    @CalledByNative
    private static void setMatchRectByIndex(
            FindMatchRectsDetails findMatchRectsDetails, int index, RectF rect) {
        findMatchRectsDetails.rects[index] = rect;
    }

    @CalledByNative
    private void onFindResultAvailable(
            int numberOfMatches, int activeMatchOrdinal, boolean finalUpdate) {
        try {
            if (mFindInPageCallbackClient != null) {
                // The WebLayer API deals in indices instead of ordinals.
                mFindInPageCallbackClient.onFindResult(
                        numberOfMatches, activeMatchOrdinal - 1, finalUpdate);
            }
        } catch (RemoteException e) {
            throw new AndroidRuntimeException(e);
        }

        if (mFindResultBar != null) {
            mFindResultBar.onFindResult();
            if (finalUpdate) {
                if (numberOfMatches > 0) {
                    mWaitingForMatchRects = true;
                    mFindInPageBridge.requestFindMatchRects(mFindResultBar.getRectsVersion());
                } else {
                    // Match rects results that correlate to an earlier call to
                    // requestFindMatchRects might still come in, so set this sentinel to false to
                    // make sure we ignore them instead of showing stale results.
                    mWaitingForMatchRects = false;
                    mFindResultBar.clearMatchRects();
                }
            }
        }
    }

    @CalledByNative
    private void onFindMatchRectsAvailable(FindMatchRectsDetails matchRects) {
        if (mFindResultBar != null && mWaitingForMatchRects) {
            mFindResultBar.setMatchRects(
                    matchRects.version, matchRects.rects, matchRects.activeRect);
        }
    }

    @Override
    public void setMediaCaptureCallbackClient(IMediaCaptureCallbackClient client) {
        mMediaStreamManager.setClient(client);
    }

    @Override
    public void stopMediaCapturing() {
        mMediaStreamManager.stopStreaming();
    }

    @CalledByNative
    private void handleCloseFromWebContents() throws RemoteException {
        // On clients < 84 WebContents-initiated tab closing was delegated to the client; this flow
        // should not be used, as the client will not be expecting it.
        assert WebLayerFactoryImpl.getClientMajorVersion() >= 84;

        if (getBrowser() == null) return;
        getBrowser().destroyTab(this);
    }

    @Override
    public void registerWebMessageCallback(
            String jsObjectName, List<String> allowedOrigins, IWebMessageCallbackClient client) {
        if (jsObjectName.isEmpty()) {
            throw new IllegalArgumentException("JS object name must not be empty");
        }
        if (allowedOrigins.isEmpty()) {
            throw new IllegalArgumentException("At least one origin must be specified");
        }
        for (String origin : allowedOrigins) {
            if (TextUtils.isEmpty(origin)) {
                throw new IllegalArgumentException("Origin must not be non-empty");
            }
        }
        String registerError = TabImplJni.get().registerWebMessageCallback(mNativeTab, jsObjectName,
                allowedOrigins.toArray(new String[allowedOrigins.size()]), client);
        if (!TextUtils.isEmpty(registerError)) {
            throw new IllegalArgumentException(registerError);
        }
    }

    @Override
    public void unregisterWebMessageCallback(String jsObjectName) {
        TabImplJni.get().unregisterWebMessageCallback(mNativeTab, jsObjectName);
    }

    public void destroy() {
        // Ensure that this method isn't called twice.
        assert mInterceptNavigationDelegate != null;

        TabImplJni.get().removeTabFromBrowserBeforeDestroying(mNativeTab);

        if (WebLayerFactoryImpl.getClientMajorVersion() >= 84) {
            // Notify the client that this instance is being destroyed to prevent it from calling
            // back into this object if the embedder mistakenly tries to do so.
            try {
                mClient.onTabDestroyed();
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }

        if (mDisplayCutoutController != null) {
            mDisplayCutoutController.destroy();
            mDisplayCutoutController = null;
        }

        // This is called to ensure a listener is removed from the WebContents.
        setScrollOffsetsEnabled(false);

        if (mTabCallbackProxy != null) {
            mTabCallbackProxy.destroy();
            mTabCallbackProxy = null;
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
        if (mGoogleAccountsCallbackProxy != null) {
            mGoogleAccountsCallbackProxy.destroy();
            mGoogleAccountsCallbackProxy = null;
        }

        mInterceptNavigationDelegateClient.destroy();
        mInterceptNavigationDelegateClient = null;
        mInterceptNavigationDelegate = null;

        mInfoBarContainer.destroy();
        mInfoBarContainer = null;

        mMediaStreamManager.destroy();
        mMediaStreamManager = null;

        // Destroying FaviconCallbackProxy removes from mFaviconCallbackProxies. Copy to avoid
        // problems.
        Set<FaviconCallbackProxy> faviconCallbackProxies = mFaviconCallbackProxies;
        mFaviconCallbackProxies = new HashSet<>();
        for (FaviconCallbackProxy proxy : faviconCallbackProxies) {
            proxy.destroy();
        }
        assert mFaviconCallbackProxies.isEmpty();

        sTabMap.remove(mId);

        // ObservableSupplierImpl.addObserver() posts a task to notify the observer, ensure the
        // callback isn't run after destroy() is called (otherwise we'll get crashes as the native
        // tab has been deleted).
        mActiveBrowserControlsVisibilityDelegate.removeObserver(mConstraintsUpdatedCallback);
        hideFindInPageUiAndNotifyClient();
        mFindInPageCallbackClient = null;
        mNavigationController = null;
        mWebContents.removeObserver(mWebContentsObserver);
        TabImplJni.get().deleteTab(mNativeTab);
        mNativeTab = 0;

        WebLayerAccessibilityUtil.get().removeObserver(mAccessibilityObserver);
    }

    @CalledByNative
    private boolean doBrowserControlsShrinkRendererSize() {
        BrowserViewController viewController = getViewController();
        return viewController != null && viewController.doBrowserControlsShrinkRendererSize();
    }

    @CalledByNative
    public void setBrowserControlsVisibilityConstraint(
            @ImplControlsVisibilityReason int reason, @BrowserControlsState int constraint) {
        mBrowserControlsDelegates.get(reason).set(constraint);
    }

    @BrowserControlsState
    /* package */ int getBrowserControlsVisibilityConstraint(
            @ImplControlsVisibilityReason int reason) {
        return mBrowserControlsDelegates.get(reason).get();
    }

    public void setOnlyExpandTopControlsAtPageTop(boolean onlyExpandControlsAtPageTop) {
        BrowserControlsVisibilityDelegate activeDelegate = onlyExpandControlsAtPageTop
                ? mBrowserControlsDelegates.get(ImplControlsVisibilityReason.RENDERER_UNAVAILABLE)
                : mComposedBrowserControlsVisibility;
        if (activeDelegate == mActiveBrowserControlsVisibilityDelegate) return;

        mActiveBrowserControlsVisibilityDelegate.removeObserver(mConstraintsUpdatedCallback);
        mActiveBrowserControlsVisibilityDelegate = activeDelegate;
        mActiveBrowserControlsVisibilityDelegate.addObserver(mConstraintsUpdatedCallback);
    }

    @CalledByNative
    public void showRepostFormWarningDialog() {
        BrowserViewController viewController = getViewController();
        if (viewController == null) {
            mWebContents.getNavigationController().cancelPendingReload();
        } else {
            viewController.showRepostFormWarningDialog();
        }
    }

    private static String nonEmptyOrNull(String s) {
        return TextUtils.isEmpty(s) ? null : s;
    }

    @CalledByNative
    private void showContextMenu(ContextMenuParams params) throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 82) return;
        mClient.showContextMenu(ObjectWrapper.wrap(params.getPageUrl()),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getLinkUrl())),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getLinkText())),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getTitleText())),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getSrcUrl())));
    }

    @VisibleForTesting
    public boolean canBrowserControlsScrollForTesting() {
        return mActiveBrowserControlsVisibilityDelegate.get() == BrowserControlsState.BOTH;
    }

    @VisibleForTesting
    public boolean didShowFullscreenToast() {
        return mFullscreenCallbackProxy != null
                && mFullscreenCallbackProxy.didShowFullscreenToast();
    }

    private void onBrowserControlsConstraintUpdated(int constraint) {
        // WARNING: this may be called before attached. This means |mBrowser| may be null.

        // If something has overridden the FIP's SHOWN constraint, cancel FIP. This causes FIP to
        // dismiss when entering fullscreen.
        if (constraint != BrowserControlsState.SHOWN) {
            hideFindInPageUiAndNotifyClient();
        }

        // Don't animate when hiding the controls unless an animation was requested by
        // BrowserControlsContainerView.
        BrowserViewController viewController = getViewController();
        boolean animate = constraint != BrowserControlsState.HIDDEN
                || (viewController != null
                        && viewController.shouldAnimateBrowserControlsHeightChanges());

        // If the renderer is not controlling the offsets (possibly hung or crashed). Then this
        // needs to force the controls to show (because notification from the renderer will not
        // happen). For js dialogs, the renderer's update will come when the dialog is hidden, and
        // since that animates from 0 height, it causes a flicker since the override is already set
        // to fully show. Thus, disable animation.
        if (constraint == BrowserControlsState.SHOWN && isActiveTab()
                && !TabImplJni.get().isRendererControllingBrowserControlsOffsets(mNativeTab)) {
            mViewAndroidDelegate.setIgnoreRendererUpdates(true);
            if (viewController != null) viewController.showControls();
            animate = false;
        } else {
            mViewAndroidDelegate.setIgnoreRendererUpdates(false);
        }

        TabImplJni.get().updateBrowserControlsConstraint(mNativeTab, constraint, animate);
    }

    private void ensureDisplayCutoutController() {
        if (mDisplayCutoutController != null) return;

        mDisplayCutoutController =
                new DisplayCutoutController(new DisplayCutoutController.Delegate() {
                    @Override
                    public Activity getAttachedActivity() {
                        WindowAndroid window = mBrowser.getWindowAndroid();
                        return window == null ? null : window.getActivity().get();
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }

                    @Override
                    public InsetObserverView getInsetObserverView() {
                        return mBrowser.getViewController().getInsetObserverView();
                    }

                    @Override
                    public boolean isInteractable() {
                        return isVisible();
                    }
                });
    }

    /**
     * Returns the BrowserViewController for this TabImpl, but only if this
     * is the active TabImpl. Can also return null if in the middle of shutdown
     * or Browser is not attached to any activity.
     */
    @Nullable
    private BrowserViewController getViewController() {
        if (!isActiveTab()) return null;
        return mBrowser.getPossiblyNullViewController();
    }

    @VisibleForTesting
    public boolean canInfoBarContainerScrollForTesting() {
        return mInfoBarContainer.getContainerViewForTesting().isAllowedToAutoHide();
    }

    @VisibleForTesting
    public String getTranslateInfoBarTargetLanguageForTesting() {
        if (!mInfoBarContainer.hasInfoBars()) return null;

        ArrayList<InfoBar> infobars = mInfoBarContainer.getInfoBarsForTesting();
        TranslateCompactInfoBar translateInfoBar = (TranslateCompactInfoBar) infobars.get(0);

        return translateInfoBar.getTargetLanguageForTesting();
    }

    @NativeMethods
    interface Natives {
        TabImpl fromWebContents(WebContents webContents);
        long createTab(long tab, TabImpl caller);
        void removeTabFromBrowserBeforeDestroying(long nativeTabImpl);
        void deleteTab(long tab);
        void setJavaImpl(long nativeTabImpl, TabImpl impl);
        void onAutofillProviderChanged(long nativeTabImpl, AutofillProvider autofillProvider);
        void setBrowserControlsContainerViews(long nativeTabImpl,
                long nativeTopBrowserControlsContainerView,
                long nativeBottomBrowserControlsContainerView);
        WebContents getWebContents(long nativeTabImpl);
        void executeScript(long nativeTabImpl, String script, boolean useSeparateIsolate,
                Callback<String> callback);
        void updateBrowserControlsConstraint(
                long nativeTabImpl, int newConstraint, boolean animate);
        String getGuid(long nativeTabImpl);
        void captureScreenShot(long nativeTabImpl, float scale,
                ValueCallback<Pair<Bitmap, Integer>> valueCallback);
        boolean setData(long nativeTabImpl, String[] data);
        String[] getData(long nativeTabImpl);
        boolean isRendererControllingBrowserControlsOffsets(long nativeTabImpl);
        String registerWebMessageCallback(long nativeTabImpl, String jsObjectName,
                String[] allowedOrigins, IWebMessageCallbackClient client);
        void unregisterWebMessageCallback(long nativeTabImpl, String jsObjectName);
        boolean canTranslate(long nativeTabImpl);
        void showTranslateUi(long nativeTabImpl);
        void setTranslateTargetLanguage(long nativeTabImpl, String targetLanguage);
    }
}
