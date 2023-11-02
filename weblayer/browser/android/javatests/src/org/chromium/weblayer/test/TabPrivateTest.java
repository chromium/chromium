// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.RemoteException;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tab tests that need to use WebLayerPrivate.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class TabPrivateTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(mActivityTestRule.getContextForWebLayer());
    }

    @Test
    @SmallTest
    public void testCreateTabWithAccessibilityEnabledCrashTest() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                getTestWebLayer().setAccessibilityEnabled(true);
            } catch (RemoteException e) {
                Assert.fail("Unable to enable accessibility");
            }
            activity.getBrowser().createTab();
        });
    }

    @Test
    @SmallTest
    public void testAutoReloadOnBackgroundCrash() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        activity.setIgnoreRendererCrashes();

        CallbackHelper renderProcessGoneHelper = new CallbackHelper();
        final Tab crashedTab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Browser browser = activity.getBrowser();
            Tab originalTab = browser.getActiveTab();
            originalTab.registerTabCallback(new TabCallback() {
                @Override
                public void onRenderProcessGone() {
                    renderProcessGoneHelper.notifyCalled();
                }
            });

            // Place a different tab in the foreground.
            browser.setActiveTab(browser.createTab());

            return originalTab;
        });

        // Crash the background tab.
        getTestWebLayer().crashTab(crashedTab);
        renderProcessGoneHelper.waitForFirst();

        // Expect a navigation to occur when the crashed background tab is brought to the front.
        // See logic in content's navigation_controller_impl.cc for why this is not classified as a
        // reload.
        CallbackHelper navigationCompletedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            crashedTab.getNavigationController().registerNavigationCallback(
                    new NavigationCallback() {
                        @Override
                        public void onNavigationCompleted(Navigation navigation) {
                            navigationCompletedHelper.notifyCalled();
                        }
                    });

            activity.getBrowser().setActiveTab(crashedTab);
        });
        navigationCompletedHelper.waitForFirst();
    }

    @Test
    @SmallTest
    public void testOnRenderProcessGone() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        CallbackHelper callbackHelper = new CallbackHelper();
        Tab tabToCrash = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            activity.setIgnoreRendererCrashes();
            TabCallback callback = new TabCallback() {
                @Override
                public void onRenderProcessGone() {
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
            return tab;
        });
        getTestWebLayer().crashTab(tabToCrash);
        callbackHelper.waitForFirst();
    }
}
