// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test;

import android.os.IBinder;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.components.infobars.InfoBarAnimationListener;
import org.chromium.components.infobars.InfoBarUiItem;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.weblayer_private.InfoBarContainer;
import org.chromium.weblayer_private.TabImpl;
import org.chromium.weblayer_private.WebLayerAccessibilityUtil;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.test_interfaces.ITestWebLayer;

import java.util.concurrent.ExecutionException;

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
    public boolean canBrowserControlsScroll(ITab tab) {
        return ((TabImpl) tab).canBrowserControlsScrollForTesting();
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

    @NativeMethods
    interface Natives {
        void waitForBrowserControlsMetadataState(
                long tabImpl, int top, int bottom, Runnable runnable);
        void setIgnoreMissingKeyForTranslateManager(boolean ignore);
    }

    @Override
    public boolean canInfoBarContainerScroll(ITab tab) {
        return ((TabImpl) tab).canInfoBarContainerScrollForTesting();
    }

    @Override
    public String getDisplayedUrl(IObjectWrapper /* View */ view) {
        View urlBarView = ObjectWrapper.unwrap(view, View.class);
        assert (urlBarView instanceof LinearLayout);
        LinearLayout urlBarLayout = (LinearLayout) urlBarView;
        assert (urlBarLayout.getChildCount() == 2);

        View textView = urlBarLayout.getChildAt(1);
        assert (textView instanceof TextView);
        TextView urlBarTextView = (TextView) textView;
        return urlBarTextView.getText().toString();
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
}
