// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import static org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.ALL_UPDATES;

import android.Manifest.permission;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillSelectionMenuItemProvider;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.components.browser_ui.media.MediaSessionHelper;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.find_in_page.FindInPageBridge;
import org.chromium.components.find_in_page.FindMatchRectsDetails;
import org.chromium.components.find_in_page.FindResultBar;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePayloadType;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.MessagePort.MessageCallback;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.ExceptionType;
import org.chromium.weblayer_private.interfaces.IContextMenuParams;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IExternalIntentInIncognitoCallbackClient;
import org.chromium.weblayer_private.interfaces.IFaviconFetcher;
import org.chromium.weblayer_private.interfaces.IFaviconFetcherClient;
import org.chromium.weblayer_private.interfaces.IFindInPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.IGoogleAccountsCallbackClient;
import org.chromium.weblayer_private.interfaces.IMediaCaptureCallbackClient;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IStringCallback;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.ScrollNotificationType;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;
import org.chromium.weblayer_private.media.MediaSessionManager;
import org.chromium.weblayer_private.media.MediaStreamManager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
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
    private ExternalIntentInIncognitoCallbackProxy mExternalIntentInIncognitoCallbackProxy;
    private MessagePort[] mChannel;
    // BrowserImpl this TabImpl is in.
    @NonNull
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
    private ActionModeCallback mActionModeCallback;

    private Set<FaviconCallbackProxy> mFaviconCallbackProxies = new HashSet<>();

    // Only non-null if scroll offsets have been requested.
    private @Nullable GestureStateListener mGestureStateListener;

    private HeaderVerificationStatus mHeaderVerification;

    enum HeaderVerificationStatus {
        PENDING,
        NOT_VALIDATED,
        VALIDATED,
    }

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
        public void onBackgroundColorChanged(int color) {
            try {
                mClient.onBackgroundColorChanged(color);
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }

        @Override
        protected void onVerticalScrollDirectionChanged(
                boolean directionUp, float currentScrollRatio) {
            super.onVerticalScrollDirectionChanged(directionUp, currentScrollRatio);
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

    class NativeStringCallback implements Callback<String> {
        private IStringCallback mCallback;

        NativeStringCallback(IStringCallback callback) {
            mCallback = callback;
        }

        @Override
        public void onResult(String result) {
            try {
                mCallback.onResult(result);
            } catch (RemoteException e) {
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

    public TabImpl(BrowserImpl browser, ProfileImpl profile, WindowAndroid windowAndroid) {
        mBrowser = browser;
        mId = ++sNextId;
        init(profile, windowAndroid, TabImplJni.get().createTab(profile.getNativeProfile(), this));
    }

    /**
     * This constructor is called when the native side triggers creation of a TabImpl
     * (as happens with popups and other scenarios).
     */
    public TabImpl(
            BrowserImpl browser, ProfileImpl profile, WindowAndroid windowAndroid, long nativeTab) {
        mId = ++sNextId;
        mBrowser = browser;
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
            public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {
                if (!navigationHandle.isSameDocument()) {
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

        mHeaderVerification = HeaderVerificationStatus.PENDING;

        mMediaStreamManager = new MediaStreamManager(this);

        mInterceptNavigationDelegateClient = new InterceptNavigationDelegateClientImpl(this);
        mInterceptNavigationDelegate =
                new InterceptNavigationDelegateImpl(mInterceptNavigationDelegateClient);
        mInterceptNavigationDelegateClient.initializeWithDelegate(mInterceptNavigationDelegate);
        sTabMap.put(mId, this);

        mInfoBarContainer = new InfoBarContainer(this);

        mMediaSessionHelper = new MediaSessionHelper(
                mWebContents, MediaSessionManager.createMediaSessionHelperDelegate(this));

        GestureListenerManager.fromWebContents(mWebContents)
                .addListener(new GestureStateListener() {
                    @Override
                    public void didOverscroll(
                            float accumulatedOverscrollX, float accumulatedOverscrollY) {
                        if (WebLayerFactoryImpl.getClientMajorVersion() < 101) return;
                        try {
                            mClient.onVerticalOverscroll(accumulatedOverscrollY);
                        } catch (RemoteException e) {
                            throw new APICallException(e);
                        }
                    }
                }, ALL_UPDATES);
    }

    private void doInitAfterSettingContainerView() {
        if (mPostContainerViewInitDone) return;

        if (mBrowser.getBrowserFragment().getViewAndroidDelegateContainerView() != null) {
            mPostContainerViewInitDone = true;

            SelectionPopupController selectionPopupController =
                    SelectionPopupController.fromWebContents(mWebContents);
            mActionModeCallback = new ActionModeCallback(mWebContents);
            mActionModeCallback.setTabClient(mClient);
            selectionPopupController.setActionModeCallback(mActionModeCallback);
            selectionPopupController.setSelectionClient(
                    SelectionClient.createSmartSelectionClient(mWebContents));
        }
    }

    public ProfileImpl getProfile() {
        return mProfile;
    }

    public ITabClient getClient() {
        return mClient;
    }

    public void setHeaderVerification(HeaderVerificationStatus headerVerification) {
        mHeaderVerification = headerVerification;
    }

    /**
     * Sets the BrowserImpl this TabImpl is contained in.
     */
    public void attachToBrowser(BrowserImpl browser) {
        // NOTE: during tab creation this is called with |browser| set to |mBrowser|. This happens
        // because the tab is created with |mBrowser| already set (to avoid having a bunch of null
        // checks).
        mBrowser = browser;
        updateFromBrowser();
    }

    public void updateFromBrowser() {
        mViewAndroidDelegate.setContainerView(
                mBrowser.getBrowserFragment().getViewAndroidDelegateContainerView());
        doInitAfterSettingContainerView();
        updateViewAttachedStateFromBrowser();

        boolean attached = mBrowser.getBrowserFragment().isAttached();
        mInterceptNavigationDelegateClient.onActivityAttachmentChanged(attached);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            SelectionPopupController selectionController =
                    SelectionPopupController.fromWebContentsNoCreate(mWebContents);
            if (!attached) {
                // The Context and ViewContainer in which Autofill was previously operating have
                // gone away, so tear down |mAutofillProvider|.
                if (mAutofillProvider != null) {
                    mAutofillProvider.destroy();
                    mAutofillProvider = null;
                }
                if (selectionController != null) {
                    selectionController.setNonSelectionAdditionalMenuItemProvider(null);
                }
            } else {
                if (mAutofillProvider == null) {
                    // Set up |mAutofillProvider| to operate in the new Context. It's safe to assume
                    // the context won't change unless it is first nulled out, since the fragment
                    // must be detached before it can be reattached to a new Context.
                    mAutofillProvider = new AutofillProvider(mBrowser.getContext(),
                            mBrowser.getBrowserFragment().getViewAndroidDelegateContainerView(),
                            mWebContents, "WebLayer");
                    TabImplJni.get().initializeAutofillIfNecessary(mNativeTab);
                }
                mAutofillProvider.onContainerViewChanged(
                        mBrowser.getBrowserFragment().getViewAndroidDelegateContainerView());
                mAutofillProvider.setWebContents(mWebContents);
                if (selectionController != null) {
                    selectionController.setNonSelectionAdditionalMenuItemProvider(
                            new AutofillSelectionMenuItemProvider(
                                    mBrowser.getContext(), mAutofillProvider));
                }
            }
        }
    }

    @VisibleForTesting
    public AutofillProvider getAutofillProviderForTesting() {
        // The test needs to make sure the |mAutofillProvider| is not null.
        return mAutofillProvider;
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

    @Override
    public String getUri() {
        StrictModeWorkaround.apply();
        return mWebContents.getVisibleUrl().getSpec();
    }

    /**
     * Called when this TabImpl is attached to the BrowserViewController.
     */
    public void onAttachedToViewController() {
        mWebContents.setTopLevelNativeWindow(mBrowser.getBrowserFragment().getWindowAndroid());
        doInitAfterSettingContainerView();
        mInfoBarContainer.onTabAttachedToViewController();
        updateWebContentsVisibility();
        updateDisplayCutoutController();
    }

    /**
     * Called when this TabImpl is detached from the BrowserViewController.
     */
    public void onDetachedFromViewController() {
        mWebContents.setTopLevelNativeWindow(null);
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
    }

    /**
     * Returns whether this Tab is visible.
     */
    public boolean isVisible() {
        return isActiveTab() && mBrowser.getBrowserFragment().isVisible();
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
        return mBrowser.getActiveTab() == this;
    }

    private void updateWebContentsVisibility() {
        if (SelectionPopupController.fromWebContentsNoCreate(mWebContents) == null) return;
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

        mDisplayCutoutController.onActivityAttachmentChanged(
                mBrowser.getBrowserFragment().getWindowAndroid());
        mDisplayCutoutController.maybeUpdateLayout();
    }

    public void loadUrl(LoadUrlParams loadUrlParams) {
        String url = loadUrlParams.getUrl();
        if (url == null || url.isEmpty()) return;

        // TODO(https://crbug.com/783819): Don't fix up all URLs. Documentation on FixupURL
        // explicitly says not to use it on URLs coming from untrustworthy sources, like other apps.
        // Once migrations of Java code to GURL are complete and incoming URLs are converted to
        // GURLs at their source, we can make decisions of whether or not to fix up GURLs on a
        // case-by-case basis based on trustworthiness of the incoming URL.
        GURL fixedUrl = UrlFormatter.fixupUrl(url);
        if (!fixedUrl.isValid()) return;

        loadUrlParams.setUrl(fixedUrl.getSpec());
        getWebContents().getNavigationController().loadUrl(loadUrlParams);
    }

    public WebContents getWebContents() {
        return mWebContents;
    }

    public NavigationControllerImpl getNavigationControllerImpl() {
        return mNavigationController;
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
        StrictModeWorkaround.apply();
        TabImplJni.get().setTranslateTargetLanguage(mNativeTab, targetLanguage);
    }

    @Override
    public void setScrollOffsetsEnabled(boolean enabled) {
        StrictModeWorkaround.apply();
        if (enabled) {
            if (mGestureStateListener == null) {
                mGestureStateListener = new GestureStateListener() {
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
                        .addListener(mGestureStateListener, ALL_UPDATES);
            }
        } else if (mGestureStateListener != null) {
            GestureListenerManager.fromWebContents(mWebContents)
                    .removeListener(mGestureStateListener);
            mGestureStateListener = null;
        }
    }

    @Override
    public void setFloatingActionModeOverride(int actionModeItemTypes) {
        StrictModeWorkaround.apply();
        mActionModeCallback.setOverride(actionModeItemTypes);
    }

    @Override
    public void setDesktopUserAgentEnabled(boolean enable) {
        StrictModeWorkaround.apply();
        TabImplJni.get().setDesktopUserAgentEnabled(mNativeTab, enable);
    }

    @Override
    public boolean isDesktopUserAgentEnabled() {
        StrictModeWorkaround.apply();
        return TabImplJni.get().isDesktopUserAgentEnabled(mNativeTab);
    }

    @Override
    public void download(IContextMenuParams contextMenuParams) {
        StrictModeWorkaround.apply();
        NativeContextMenuParamsHolder nativeContextMenuParamsHolder =
                (NativeContextMenuParamsHolder) contextMenuParams;

        WindowAndroid window = getBrowser().getBrowserFragment().getWindowAndroid();
        if (window.hasPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            continueDownload(nativeContextMenuParamsHolder);
            return;
        }

        String[] requestPermissions = new String[] {permission.WRITE_EXTERNAL_STORAGE};
        window.requestPermissions(requestPermissions, (permissions, grantResults) -> {
            if (grantResults.length == 1 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                continueDownload(nativeContextMenuParamsHolder);
            }
        });
    }

    private void continueDownload(NativeContextMenuParamsHolder nativeContextMenuParamsHolder) {
        TabImplJni.get().download(
                mNativeTab, nativeContextMenuParamsHolder.mNativeContextMenuParams);
    }

    @Override
    public void addToHomescreen() {
        StrictModeWorkaround.apply();
        // TODO(estade): should it be verified that |this| is the active tab?

        // This is used for UMA, and is only meaningful for Chrome. TODO(estade): remove.
        Bundle menuItemData = new Bundle();
        menuItemData.putInt(AppBannerManager.MENU_TITLE_KEY, 0);
        // TODO(estade): simplify these parameters.
        AddToHomescreenCoordinator.showForAppMenu(mBrowser.getContext(),
                mBrowser.getBrowserFragment().getWindowAndroid(),
                mBrowser.getBrowserFragment().getWindowAndroid().getModalDialogManager(),
                mWebContents, menuItemData);
    }

    @Override
    public void setExternalIntentInIncognitoCallbackClient(
            IExternalIntentInIncognitoCallbackClient client) {
        StrictModeWorkaround.apply();
        if (client != null) {
            if (mExternalIntentInIncognitoCallbackProxy == null) {
                mExternalIntentInIncognitoCallbackProxy =
                        new ExternalIntentInIncognitoCallbackProxy(client);
            } else {
                mExternalIntentInIncognitoCallbackProxy.setClient(client);
            }
        } else if (mExternalIntentInIncognitoCallbackProxy != null) {
            mExternalIntentInIncognitoCallbackProxy = null;
        }
    }

    public ExternalIntentInIncognitoCallbackProxy getExternalIntentInIncognitoCallbackProxy() {
        return mExternalIntentInIncognitoCallbackProxy;
    }

    public void removeFaviconCallbackProxy(FaviconCallbackProxy proxy) {
        mFaviconCallbackProxies.remove(proxy);
    }

    @Override
    public void executeScript(String script, boolean useSeparateIsolate, IStringCallback callback) {
        StrictModeWorkaround.apply();
        if (mHeaderVerification == HeaderVerificationStatus.VALIDATED) {
            TabImplJni.get().executeScript(
                    mNativeTab, script, useSeparateIsolate, new NativeStringCallback(callback));
            return;
        }

        WebLayerOriginVerificationScheduler originVerifier =
                WebLayerOriginVerificationScheduler.getInstance();
        String url = mWebContents.getVisibleUrl().getSpec();
        originVerifier.verify(url, (verified) -> {
            // Make sure the page hasn't changed since we started verification.
            if (!url.equals(mWebContents.getVisibleUrl().getSpec())) {
                return;
            }

            if (!verified) {
                try {
                    callback.onException(ExceptionType.RESTRICTED_API,
                            "Application does not have permissions to modify " + url);
                } catch (RemoteException e) {
                }
            }
            TabImplJni.get().executeScript(
                    mNativeTab, script, useSeparateIsolate, new NativeStringCallback(callback));
        });
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

        mFindInPageCallbackClient = client;
        assert mFindInPageBridge == null;
        mFindInPageBridge = new FindInPageBridge(mWebContents);
        assert mFindResultBar == null;
        mFindResultBar =
                new FindResultBar(mBrowser.getContext(), controller.getWebContentsOverlayView(),
                        mBrowser.getBrowserFragment().getWindowAndroid(), mFindInPageBridge);
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

        SelectionPopupController popup =
                SelectionPopupController.fromWebContentsNoCreate(mWebContents);
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
    private void onFindResultAvailable(int numberOfMatches, int activeMatchOrdinal,
            boolean finalUpdate) throws RemoteException {
        if (mFindInPageCallbackClient != null) {
            // The WebLayer API deals in indices instead of ordinals.
            mFindInPageCallbackClient.onFindResult(
                    numberOfMatches, activeMatchOrdinal - 1, finalUpdate);
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
        StrictModeWorkaround.apply();
        mMediaStreamManager.setClient(client);
    }

    @Override
    public void stopMediaCapturing() {
        StrictModeWorkaround.apply();
        mMediaStreamManager.stopStreaming();
    }

    @CalledByNative
    private void handleCloseFromWebContents() throws RemoteException {
        if (getBrowser() == null) return;
        getBrowser().destroyTab(this);
    }

    private String getAppOrigin() {
        // TODO(rayankans): Consider exposing the embedder app's fingerprints as well.
        return "app://" + mBrowser.getContext().getPackageName();
    }

    @Override
    public void postMessage(String message, String targetOrigin) {
        StrictModeWorkaround.apply();

        if (mChannel == null || mChannel[0].isClosed() || mChannel[0].isTransferred()
                || mChannel[1].isClosed() || mChannel[1].isTransferred()) {
            mChannel = mWebContents.createMessageChannel();
            mChannel[0].setMessageCallback(new MessageCallback() {
                @Override
                public void onMessage(MessagePayload messagePayload, MessagePort[] sentPorts) {
                    try {
                        if (messagePayload.getType() == MessagePayloadType.ARRAY_BUFFER) {
                            // TODO(rayankans): Consider supporting passing array buffers.
                            return;
                        }
                        mClient.onPostMessage(messagePayload.getAsString(),
                                mWebContents.getVisibleUrl().getOrigin().getSpec());
                    } catch (RemoteException e) {
                    }
                }
            }, null);
        }

        mWebContents.postMessageToMainFrame(new MessagePayload(message), getAppOrigin(),
                targetOrigin, new MessagePort[] {mChannel[1]});
    }

    public void destroy() {
        // Ensure that this method isn't called twice.
        assert mInterceptNavigationDelegate != null;

        TabImplJni.get().removeTabFromBrowserBeforeDestroying(mNativeTab);

        // Notify the client that this instance is being destroyed to prevent it from calling
        // back into this object if the embedder mistakenly tries to do so.
        try {
            mClient.onTabDestroyed();
        } catch (RemoteException e) {
            throw new APICallException(e);
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

        if (mMediaSessionHelper != null) {
            mMediaSessionHelper.destroy();
            mMediaSessionHelper = null;
        }

        if (mAutofillProvider != null) {
            mAutofillProvider.destroy();
            mAutofillProvider = null;
        }

        // Destroying FaviconCallbackProxy removes from mFaviconCallbackProxies. Copy to avoid
        // problems.
        Set<FaviconCallbackProxy> faviconCallbackProxies = mFaviconCallbackProxies;
        mFaviconCallbackProxies = new HashSet<>();
        for (FaviconCallbackProxy proxy : faviconCallbackProxies) {
            proxy.destroy();
        }
        assert mFaviconCallbackProxies.isEmpty();

        sTabMap.remove(mId);

        hideFindInPageUiAndNotifyClient();
        mFindInPageCallbackClient = null;
        mNavigationController = null;
        mWebContents.removeObserver(mWebContentsObserver);
        TabImplJni.get().deleteTab(mNativeTab);
        mNativeTab = 0;
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

    private static class NativeContextMenuParamsHolder extends IContextMenuParams.Stub {
        // Note: avoid adding more members since an object with a finalizer will delay GC of any
        // object it references.
        private final long mNativeContextMenuParams;

        NativeContextMenuParamsHolder(long nativeContextMenuParams) {
            mNativeContextMenuParams = nativeContextMenuParams;
        }

        /**
         * A finalizer is required to ensure that the native object associated with
         * this object gets destructed, otherwise there would be a memory leak.
         *
         * This is safe because it makes a simple call into C++ code that is both
         * thread-safe and very fast.
         *
         * @see java.lang.Object#finalize()
         */
        @Override
        protected final void finalize() throws Throwable {
            super.finalize();
            TabImplJni.get().destroyContextMenuParams(mNativeContextMenuParams);
        }
    }

    @CalledByNative
    private void showContextMenu(ContextMenuParams params, long nativeContextMenuParams)
            throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 88) {
            mClient.showContextMenu(ObjectWrapper.wrap(params.getPageUrl().getSpec()),
                    ObjectWrapper.wrap(nonEmptyOrNull(params.getLinkUrl().getSpec())),
                    ObjectWrapper.wrap(nonEmptyOrNull(params.getLinkText())),
                    ObjectWrapper.wrap(nonEmptyOrNull(params.getTitleText())),
                    ObjectWrapper.wrap(nonEmptyOrNull(params.getSrcUrl().getSpec())));
            return;
        }

        boolean canDownload =
                (params.isImage() && UrlUtilities.isDownloadableScheme(params.getSrcUrl()))
                || (params.isVideo() && UrlUtilities.isDownloadableScheme(params.getSrcUrl())
                        && params.canSaveMedia())
                || (params.isAnchor() && UrlUtilities.isDownloadableScheme(params.getLinkUrl()));
        mClient.showContextMenu2(ObjectWrapper.wrap(params.getPageUrl().getSpec()),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getLinkUrl().getSpec())),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getLinkText())),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getTitleText())),
                ObjectWrapper.wrap(nonEmptyOrNull(params.getSrcUrl().getSpec())), params.isImage(),
                params.isVideo(), canDownload,
                new NativeContextMenuParamsHolder(nativeContextMenuParams));
    }

    @VisibleForTesting
    public boolean didShowFullscreenToast() {
        return mFullscreenCallbackProxy != null
                && mFullscreenCallbackProxy.didShowFullscreenToast();
    }

    private void ensureDisplayCutoutController() {
        if (mDisplayCutoutController != null) return;

        mDisplayCutoutController =
                new DisplayCutoutController(new DisplayCutoutController.Delegate() {
                    @Override
                    public Activity getAttachedActivity() {
                        WindowAndroid window = mBrowser.getBrowserFragment().getWindowAndroid();
                        return window == null ? null : window.getActivity().get();
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }

                    @Override
                    public InsetObserverView getInsetObserverView() {
                        BrowserViewController controller =
                                mBrowser.getBrowserFragment().getPossiblyNullViewController();
                        return controller != null ? controller.getInsetObserverView() : null;
                    }

                    @Override
                    public ObservableSupplier<Integer> getBrowserDisplayCutoutModeSupplier() {
                        // No activity-wide display cutout mode override.
                        return null;
                    }

                    @Override
                    public boolean isInteractable() {
                        return isVisible();
                    }

                    @Override
                    public boolean isInBrowserFullscreen() {
                        return false;
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
        // During rotation it's possible for this to be called before BrowserViewController has been
        // updated. Verify BrowserViewController reflects this is the active tab before returning
        // it.
        BrowserViewController viewController =
                mBrowser.getBrowserFragment().getPossiblyNullViewController();
        return viewController != null && viewController.getTab() == this ? viewController : null;
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

    /** Called by {@link FaviconCallbackProxy} when the favicon for the current page has changed. */
    public void onFaviconChanged(Bitmap bitmap) {
        if (mMediaSessionHelper != null) {
            mMediaSessionHelper.updateFavicon(bitmap);
        }
    }

    @NativeMethods
    interface Natives {
        TabImpl fromWebContents(WebContents webContents);
        long createTab(long tab, TabImpl caller);
        void removeTabFromBrowserBeforeDestroying(long nativeTabImpl);
        void deleteTab(long tab);
        void setJavaImpl(long nativeTabImpl, TabImpl impl);
        void initializeAutofillIfNecessary(long nativeTabImpl);
        WebContents getWebContents(long nativeTabImpl);
        void executeScript(long nativeTabImpl, String script, boolean useSeparateIsolate,
                Callback<String> callback);
        String getGuid(long nativeTabImpl);
        void captureScreenShot(long nativeTabImpl, float scale,
                ValueCallback<Pair<Bitmap, Integer>> valueCallback);
        boolean setData(long nativeTabImpl, String[] data);
        String[] getData(long nativeTabImpl);
        boolean canTranslate(long nativeTabImpl);
        void showTranslateUi(long nativeTabImpl);
        void setTranslateTargetLanguage(long nativeTabImpl, String targetLanguage);
        void setDesktopUserAgentEnabled(long nativeTabImpl, boolean enable);
        boolean isDesktopUserAgentEnabled(long nativeTabImpl);
        void download(long nativeTabImpl, long nativeContextMenuParams);
        void destroyContextMenuParams(long contextMenuParams);
    }
}
