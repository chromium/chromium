// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test;

import android.os.IBinder;
import android.os.RemoteException;

import androidx.fragment.app.FragmentManager;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.components.autofill.AutofillProviderTestHelper;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs;
import org.chromium.components.infobars.InfoBarAnimationListener;
import org.chromium.components.infobars.InfoBarUiItem;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.media_router.BrowserMediaRouter;
import org.chromium.components.media_router.MockMediaRouteProvider;
import org.chromium.components.media_router.RouterTestUtils;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.webauthn.AuthenticatorImpl;
import org.chromium.components.webauthn.MockFido2CredentialRequest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.weblayer_private.BrowserImpl;
import org.chromium.weblayer_private.DownloadImpl;
import org.chromium.weblayer_private.InfoBarContainer;
import org.chromium.weblayer_private.ProfileImpl;
import org.chromium.weblayer_private.TabImpl;
import org.chromium.weblayer_private.WebLayerAccessibilityUtil;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.media.MediaRouteDialogFragmentImpl;
import org.chromium.weblayer_private.test_interfaces.ITestWebLayer;

import java.util.ArrayList;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Root implementation class for TestWebLayer.
 */
@JNINamespace("weblayer")
@UsedByReflection("WebLayer")
public final class TestWebLayerImpl extends ITestWebLayer.Stub {
    private MockLocationProvider mMockLocationProvider;

    @UsedByReflection("WebLayer")
    public static IBinder create() {
        return new TestWebLayerImpl();
    }

    private TestWebLayerImpl() {}

    @Override
    public boolean isNetworkChangeAutoDetectOn() {
        return NetworkChangeNotifier.getAutoDetectorForTest() != null;
    }

    @Override
    public void setMockLocationProvider(boolean enable) {
        if (enable) {
            mMockLocationProvider = new MockLocationProvider();
            LocationProviderOverrider.setLocationProviderImpl(mMockLocationProvider);
        } else if (mMockLocationProvider != null) {
            mMockLocationProvider.stop();
            mMockLocationProvider.stopUpdates();
        }
    }

    @Override
    public boolean isMockLocationProviderRunning() {
        return mMockLocationProvider.isRunning();
    }

    @Override
    public boolean isPermissionDialogShown() {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> {
                return PermissionDialogController.getInstance().isDialogShownForTest();
            });
        } catch (ExecutionException e) {
            return false;
        }
    }

    @Override
    public void clickPermissionDialogButton(boolean allow) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController.getInstance().clickButtonForTest(allow
                            ? ModalDialogProperties.ButtonType.POSITIVE
                            : ModalDialogProperties.ButtonType.NEGATIVE);
        });
    }

    @Override
    public void setSystemLocationSettingEnabled(boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LocationUtils.setFactory(() -> {
                return new LocationUtils() {
                    @Override
                    public boolean isSystemLocationSettingEnabled() {
                        return enabled;
                    }
                };
            });
        });
    }

    @Override
    public void waitForBrowserControlsMetadataState(
            ITab tab, int topHeight, int bottomHeight, IObjectWrapper runnable) {
        TestWebLayerImplJni.get().waitForBrowserControlsMetadataState(
                ((TabImpl) tab).getNativeTab(), topHeight, bottomHeight,
                ObjectWrapper.unwrap(runnable, Runnable.class));
    }

    @Override
    public void setAccessibilityEnabled(boolean value) {
        WebLayerAccessibilityUtil.get().setAccessibilityEnabledForTesting(value);
    }

    @Override
    public void addInfoBar(ITab tab, IObjectWrapper runnable) {
        Runnable unwrappedRunnable = ObjectWrapper.unwrap(runnable, Runnable.class);
        TabImpl tabImpl = (TabImpl) tab;

        InfoBarContainer infoBarContainer = tabImpl.getInfoBarContainerForTesting();
        infoBarContainer.addAnimationListener(new InfoBarAnimationListener() {
            @Override
            public void notifyAnimationFinished(int animationType) {}
            @Override
            public void notifyAllAnimationsFinished(InfoBarUiItem frontInfoBar) {
                unwrappedRunnable.run();
                infoBarContainer.removeAnimationListener(this);
            }
        });

        TestInfoBar.show((TabImpl) tab);
    }

    @Override
    public IObjectWrapper getInfoBarContainerView(ITab tab) {
        return ObjectWrapper.wrap(
                ((TabImpl) tab).getInfoBarContainerForTesting().getViewForTesting());
    }

    @Override
    public void setIgnoreMissingKeyForTranslateManager(boolean ignore) {
        TestWebLayerImplJni.get().setIgnoreMissingKeyForTranslateManager(ignore);
    }

    @Override
    public void forceNetworkConnectivityState(boolean networkAvailable) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NetworkChangeNotifier.forceConnectivityState(true); });
    }

    @Override
    public boolean canInfoBarContainerScroll(ITab tab) {
        return ((TabImpl) tab).canInfoBarContainerScrollForTesting();
    }

    @Override
    public String getTranslateInfoBarTargetLanguage(ITab tab) {
        TabImpl tabImpl = (TabImpl) tab;
        return tabImpl.getTranslateInfoBarTargetLanguageForTesting();
    }

    @Override
    public boolean didShowFullscreenToast(ITab tab) {
        TabImpl tabImpl = (TabImpl) tab;
        return tabImpl.didShowFullscreenToast();
    }

    @Override
    public void initializeMockMediaRouteProvider(boolean closeRouteWithErrorOnSend,
            boolean disableIsSupportsSource, String createRouteErrorMessage,
            String joinRouteErrorMessage) {
        BrowserMediaRouter.setRouteProviderFactoryForTest(new MockMediaRouteProvider.Factory());

        if (closeRouteWithErrorOnSend) {
            MockMediaRouteProvider.Factory.sProvider.setCloseRouteWithErrorOnSend(true);
        }
        if (disableIsSupportsSource) {
            MockMediaRouteProvider.Factory.sProvider.setIsSupportsSource(false);
        }
        if (createRouteErrorMessage != null) {
            MockMediaRouteProvider.Factory.sProvider.setCreateRouteErrorMessage(
                    createRouteErrorMessage);
        }
        if (joinRouteErrorMessage != null) {
            MockMediaRouteProvider.Factory.sProvider.setJoinRouteErrorMessage(
                    joinRouteErrorMessage);
        }
    }

    @Override
    public IObjectWrapper getMediaRouteButton(String name) {
        FragmentManager fm =
                MediaRouteDialogFragmentImpl.getInstanceForTest().getSupportFragmentManager();
        return ObjectWrapper.wrap(RouterTestUtils.waitForRouteButton(fm, name));
    }

    @Override
    public void crashTab(ITab tab) {
        try {
            TabImpl tabImpl = (TabImpl) tab;
            WebContentsUtils.crashTabAndWait(tabImpl.getWebContents());
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public boolean isWindowOnSmallDevice(IBrowser browser) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> { return ((BrowserImpl) browser).isWindowOnSmallDevice(); });
        } catch (ExecutionException e) {
            return true;
        }
    }

    @Override
    public void fetchAccessToken(IProfile profile, IObjectWrapper /* Set<String> */ scopes,
            IObjectWrapper /* ValueCallback<String> */ onTokenFetched) throws RemoteException {
        ProfileImpl profileImpl = (ProfileImpl) profile;
        profileImpl.fetchAccessTokenForTesting(scopes, onTokenFetched);
    }

    @Override
    public void addContentCaptureConsumer(IBrowser browser,
            IObjectWrapper /* Runnable */ onNewEvents,
            IObjectWrapper /* ArrayList<Integer>*/ eventsObserved) {
        Runnable unwrappedOnNewEvents = ObjectWrapper.unwrap(onNewEvents, Runnable.class);
        ArrayList<Integer> unwrappedEventsObserved =
                ObjectWrapper.unwrap(eventsObserved, ArrayList.class);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowserImpl browserImpl = (BrowserImpl) browser;
            browserImpl.getViewController().addContentCaptureConsumerForTesting(
                    new TestContentCaptureConsumer(unwrappedOnNewEvents, unwrappedEventsObserved));
        });
    }

    @Override
    public void notifyOfAutofillEvents(IBrowser browser, IObjectWrapper /* Runnable */ onNewEvents,
            IObjectWrapper /* ArrayList<Integer>*/ eventsObserved) {
        Runnable unwrappedOnNewEvents = ObjectWrapper.unwrap(onNewEvents, Runnable.class);
        ArrayList<Integer> unwrappedEventsObserved =
                ObjectWrapper.unwrap(eventsObserved, ArrayList.class);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AutofillProviderTestHelper.disableDownloadServerForTesting();
            BrowserImpl browserImpl = (BrowserImpl) browser;
            TabImpl tab = browserImpl.getActiveTab();
            tab.getAutofillProviderForTesting().replaceAutofillManagerWrapperForTesting(
                    new TestAutofillManagerWrapper(browserImpl.getContext(), unwrappedOnNewEvents,
                            unwrappedEventsObserved));
        });
    }

    @Override
    public void activateBackgroundFetchNotification(int id) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> DownloadImpl.activateNotificationForTesting(id));
    }

    @Override
    public void expediteDownloadService() {
        TestWebLayerImplJni.get().expediteDownloadService();
    }

    @Override
    public void setMockWebAuthnEnabled(boolean enabled) {
        if (enabled) {
            AuthenticatorImpl.overrideFido2CredentialRequestForTesting(
                    new MockFido2CredentialRequest());
        } else {
            AuthenticatorImpl.overrideFido2CredentialRequestForTesting(null);
        }
    }

    @Override
    public void fireOnAccessTokenIdentifiedAsInvalid(IProfile profile,
            IObjectWrapper /* Set<String> */ scopes, IObjectWrapper /* String */ token)
            throws RemoteException {
        ProfileImpl profileImpl = (ProfileImpl) profile;
        profileImpl.fireOnAccessTokenIdentifiedAsInvalidForTesting(scopes, token);
    }

    @Override
    public void grantLocationPermission(String url) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { TestWebLayerImplJni.get().grantLocationPermission(url); });
    }

    @Override
    public void setTextScaling(IProfile profile, float value) {
        ProfileImpl profileImpl = (ProfileImpl) profile;
        FontSizePrefs.getInstance(profileImpl).setUserFontScaleFactor(value);
    }

    @Override
    public boolean getForceEnableZoom(IProfile profile) {
        ProfileImpl profileImpl = (ProfileImpl) profile;
        return FontSizePrefs.getInstance(profileImpl).getForceEnableZoom();
    }

    @NativeMethods
    interface Natives {
        void waitForBrowserControlsMetadataState(
                long tabImpl, int top, int bottom, Runnable runnable);
        void setIgnoreMissingKeyForTranslateManager(boolean ignore);
        void expediteDownloadService();
        void grantLocationPermission(String url);
    }
}
