// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.fragment.app.Fragment;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.ActionModeCallback;
import org.chromium.weblayer.ActionModeItemType;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for Tab.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class TabTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    @Test
    @SmallTest
    public void testBeforeUnload() {
        String url = mActivityTestRule.getTestDataURL("before_unload.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        Assert.assertTrue(mActivity.hasWindowFocus());

        // Touch the view so that beforeunload will be respected (if the user doesn't interact with
        // the tab, it's ignored).
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());
        // Round trip through the renderer to make sure te above touch is handled before we call
        // dispatchBeforeUnloadAndClose().
        mActivityTestRule.executeScriptSync("var x = 1", true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getBrowser().getActiveTab().dispatchBeforeUnloadAndClose(); });

        // Wait till the main window loses focus due to the app modal beforeunload dialog.
        BoundedCountDownLatch noFocusLatch = new BoundedCountDownLatch(1);
        BoundedCountDownLatch hasFocusLatch = new BoundedCountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getWindow()
                    .getDecorView()
                    .getViewTreeObserver()
                    .addOnWindowFocusChangeListener((boolean hasFocus) -> {
                        (hasFocus ? hasFocusLatch : noFocusLatch).countDown();
                    });
        });
        noFocusLatch.timedAwait();

        // Verify closing the tab works still while beforeunload is showing (no crash).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getBrowser().destroyTab(mActivity.getBrowser().getActiveTab());
        });

        // Focus returns to the main window because the dialog is dismissed when the tab is
        // destroyed.
        hasFocusLatch.timedAwait();
    }

    @Test
    @SmallTest
    public void testBeforeUnloadNoHandler() {
        String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        OnTabRemovedTabListCallbackImpl callback = new OnTabRemovedTabListCallbackImpl();
        // Verify that calling dispatchBeforeUnloadAndClose will close the tab asynchronously when
        // there is no beforeunload handler.
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            mActivity.getBrowser().registerTabListCallback(callback);
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.dispatchBeforeUnloadAndClose();
            return callback.hasClosed();
        }));

        callback.waitForCloseTab();
    }

    @Test
    @SmallTest
    public void testBeforeUnloadNoInteraction() {
        String url = mActivityTestRule.getTestDataURL("before_unload.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(mActivity);
        OnTabRemovedTabListCallbackImpl callback = new OnTabRemovedTabListCallbackImpl();
        // Verify that beforeunload is not run when there's no user action.
        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            mActivity.getBrowser().registerTabListCallback(callback);
            Tab tab = mActivity.getBrowser().getActiveTab();
            tab.dispatchBeforeUnloadAndClose();
            return callback.hasClosed();
        }));

        callback.waitForCloseTab();
    }

    private Bitmap captureScreenShot(float scale) throws TimeoutException {
        Bitmap[] bitmapHolder = new Bitmap[1];
        int[] errorCodeHolder = new int[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivity.getTab();
            tab.captureScreenShot(scale, (Bitmap bitmap, int errorCode) -> {
                errorCodeHolder[0] = errorCode;
                bitmapHolder[0] = bitmap;
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForFirst();
        Assert.assertNotNull(bitmapHolder[0]);
        Assert.assertEquals(0, errorCodeHolder[0]);
        return bitmapHolder[0];
    }

    private void checkQuadrantColors(Bitmap bitmap) {
        int quarterWidth = bitmap.getWidth() / 4;
        int quarterHeight = bitmap.getHeight() / 4;
        Assert.assertEquals(Color.rgb(255, 0, 0), bitmap.getPixel(quarterWidth, quarterHeight));
        Assert.assertEquals(Color.rgb(0, 255, 0), bitmap.getPixel(quarterWidth * 3, quarterHeight));
        Assert.assertEquals(Color.rgb(0, 0, 255), bitmap.getPixel(quarterWidth, quarterHeight * 3));
        Assert.assertEquals(
                Color.rgb(128, 128, 128), bitmap.getPixel(quarterWidth * 3, quarterHeight * 3));
    }

    @Test
    @SmallTest
    public void testCaptureScreenShot() throws TimeoutException {
        String url = mActivityTestRule.getTestDataURL("quadrant_colors.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);

        Bitmap bitmap = captureScreenShot(1.f);
        checkQuadrantColors(bitmap);
        Bitmap halfBitmap = captureScreenShot(0.5f);
        checkQuadrantColors(bitmap);

        final int allowedError = 10;
        Assert.assertTrue(Math.abs(bitmap.getWidth() / 2 - halfBitmap.getWidth()) < allowedError);
        Assert.assertTrue(Math.abs(bitmap.getHeight() / 2 - halfBitmap.getHeight()) < allowedError);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(101)
    @DisabledTest(message = "https://crbug.com/1319845")
    public void testCaptureScreenShotAfterResize() throws TimeoutException, ExecutionException {
        String url = mActivityTestRule.getTestDataURL("quadrant_colors.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);

        int newHeight = TestThreadUtils.runOnUiThreadBlocking(() -> {
            View view = mActivity.getFragment().getView();
            int height = view.getHeight() + 10;
            LinearLayout.LayoutParams params =
                    new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, height);
            view.setLayoutParams(params);
            view.requestLayout();
            return height;
        });

        CriteriaHelper.pollUiThread(() -> {
            View view = mActivity.getFragment().getView();
            int height = view.getHeight();
            Criteria.checkThat(height, Matchers.is(newHeight));
        });

        Bitmap bitmap = captureScreenShot(1.f);
        checkQuadrantColors(bitmap);
    }

    @Test
    @SmallTest
    public void testCaptureScreenShotDoesNotHang() throws TimeoutException {
        String startupUrl = "about:blank";
        mActivity = mActivityTestRule.launchShellWithUrl(startupUrl);

        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivity.getTab();
            tab.captureScreenShot(1.f, (Bitmap bitmap, int errorCode) -> {
                // Failure is ok here, so not checking |bitmap| or |errorCode|.
                callbackHelper.notifyCalled();
            });
            mActivity.destroyFragment();
        });
        callbackHelper.waitForFirst();
    }

    @Test
    @SmallTest
    public void testSetData() {
        String startupUrl = "about:blank";
        mActivity = mActivityTestRule.launchShellWithUrl(startupUrl);

        Map<String, String> data = new HashMap<>();
        data.put("foo", "bar");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivity.getTab();
            tab.setData(data);
            Assert.assertEquals(data.get("foo"), tab.getData().get("foo"));

            tab.setData(new HashMap<>());
            Assert.assertTrue(tab.getData().isEmpty());
        });
    }

    @Test
    @SmallTest
    public void testSetDataMaxSize() {
        String startupUrl = "about:blank";
        mActivity = mActivityTestRule.launchShellWithUrl(startupUrl);

        Map<String, String> data = new HashMap<>();
        data.put("big", new String(new char[10000]));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                mActivity.getTab().setData(data);
            } catch (IllegalArgumentException e) {
                // Expected exception.
                return;
            }
            Assert.fail("Expected IllegalArgumentException.");
        });
    }

    @Test
    @SmallTest
    public void testCreateTab() throws Exception {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        CallbackHelper helper = new CallbackHelper();
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Browser browser = mActivity.getBrowser();
            browser.registerTabListCallback(new TabListCallback() {
                @Override
                public void onTabAdded(Tab tab) {
                    helper.notifyCalled();
                }
            });
            Tab newTab = mActivity.getBrowser().createTab();
            Assert.assertEquals(mActivity.getBrowser().getTabs().size(), 2);
            Assert.assertNotEquals(newTab, mActivity.getTab());
            return newTab;
        });
        helper.waitForFirst();

        // Make sure the new tab can navigate correctly.
        mActivityTestRule.navigateAndWait(
                tab, mActivityTestRule.getTestDataURL("simple_page.html"), false);
    }

    @Test
    @SmallTest
    public void testViewDetachedTabIsInvisible() throws Exception {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");

        boolean hidden = mActivityTestRule.executeScriptAndExtractBoolean("document.hidden;");
        Assert.assertFalse(hidden);

        Fragment fragment = mActivityTestRule.getFragment();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View fragmentView = fragment.getView();
            ViewGroup parent = (ViewGroup) fragmentView.getParent();
            parent.removeView(fragmentView);
        });

        hidden = mActivityTestRule.executeScriptAndExtractBoolean("document.hidden;");
        Assert.assertTrue(hidden);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1248183")
    // This is a regression test for https://crbug.com/1075744 .
    public void testRotationDoesntChangeVisibility() throws Exception {
        String url = mActivityTestRule.getTestDataURL("rotation.html");
        mActivity = mActivityTestRule.launchShellWithUrl(url);
        mActivity.setRetainInstance(true);
        Assert.assertNotNull(mActivity);

        // Touch to trigger fullscreen and rotation.
        EventUtils.simulateTouchCenterOfView(mActivity.getWindow().getDecorView());

        // Wait for the page to be told the orientation changed.
        CriteriaHelper.pollInstrumentationThread(() -> {
            return mActivityTestRule.executeScriptAndExtractBoolean("gotOrientationChange", false);
        });

        // The WebContents should not have been hidden as a result of the rotation.
        Assert.assertFalse(mActivityTestRule.executeScriptAndExtractBoolean("gotHide", false));
    }

    @Test
    @SmallTest
    public void setFloatingActionModeOverride() throws Exception {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getBrowser().getActiveTab().setFloatingActionModeOverride(
                    ActionModeItemType.SHARE, new ActionModeCallback() {
                        @Override
                        public void onActionItemClicked(
                                @ActionModeItemType int item, String selectedText) {}
                    });
        });

        // Smoke test. It's not possible to trigger an action mode click in a test.
    }

    @Test
    @SmallTest
    public void testWillAutomaticallyReloadAfterCrash() throws Exception {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        Browser browser2 = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Browser browser = mActivity.getBrowser();
            // Initial tab is foreground, so it won't automatically reload.
            Tab initialTab = browser.getActiveTab();
            Assert.assertFalse(initialTab.willAutomaticallyReloadAfterCrash());

            // New tab is background, so it will automatically reload.
            Tab newTab = browser.createTab();
            Assert.assertEquals(browser.getTabs().size(), 2);
            Assert.assertNotEquals(newTab, mActivity.getTab());
            Assert.assertNotEquals(newTab, browser.getActiveTab());
            Assert.assertTrue(newTab.willAutomaticallyReloadAfterCrash());

            // New tab is foreground after being made active.
            browser.setActiveTab(newTab);
            Assert.assertEquals(newTab, browser.getActiveTab());
            Assert.assertFalse(newTab.willAutomaticallyReloadAfterCrash());
            Assert.assertTrue(initialTab.willAutomaticallyReloadAfterCrash());

            // Add a second browser; both browsers can have tabs that think they're foreground.
            Browser newBrowser =
                    Browser.fromFragment(mActivity.createBrowserFragment(android.R.id.content));
            Assert.assertTrue(initialTab.willAutomaticallyReloadAfterCrash());
            Assert.assertFalse(newTab.willAutomaticallyReloadAfterCrash());
            Assert.assertFalse(newBrowser.getActiveTab().willAutomaticallyReloadAfterCrash());

            // Moving the activity to the background causes all tabs to be not foreground.
            mActivity.moveTaskToBack(true);
            return newBrowser;
        });

        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(mActivity.getBrowser()
                                                      .getActiveTab()
                                                      .willAutomaticallyReloadAfterCrash(),
                                Matchers.is(true)));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue(
                                browser2.getActiveTab().willAutomaticallyReloadAfterCrash()));
    }
}
