// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.weblayer.ContextMenuParams;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.ScrollNotificationType;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests that TabCallback methods are invoked as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class TabCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static class Callback extends TabCallback {
        public static class TabCallbackValueRecorder {
            private List<String> mObservedValues =
                    Collections.synchronizedList(new ArrayList<String>());

            public void recordValue(String parameter) {
                mObservedValues.add(parameter);
            }

            public List<String> getObservedValues() {
                return mObservedValues;
            }

            public void waitUntilValueObserved(String expectation) {
                CriteriaHelper.pollInstrumentationThread(
                        () -> Criteria.checkThat(expectation, Matchers.isIn(mObservedValues)));
            }
        }

        public TabCallbackValueRecorder visibleUriChangedCallback = new TabCallbackValueRecorder();

        @Override
        public void onVisibleUriChanged(Uri uri) {
            visibleUriChangedCallback.recordValue(uri.toString());
        }
    }

    @Test
    @SmallTest
    public void testLoadEvents() {
        String startupUrl = "about:blank";
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(startupUrl);

        Callback callback = new Callback();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getTab().registerTabCallback(callback); });

        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(url);

        /* Verify that the visible URL changes to the target. */
        callback.visibleUriChangedCallback.waitUntilValueObserved(url);
    }

    private ContextMenuParams runContextMenuTest(String file) throws TimeoutException {
        String pageUrl = mActivityTestRule.getTestDataURL(file);
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(pageUrl);

        ContextMenuParams params[] = new ContextMenuParams[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void showContextMenu(ContextMenuParams param) {
                    params[0] = param;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
        });

        TestTouchUtils.longClickView(
                InstrumentationRegistry.getInstrumentation(), activity.getWindow().getDecorView());
        callbackHelper.waitForFirst();
        Assert.assertEquals(Uri.parse(pageUrl), params[0].pageUri);
        return params[0];
    }

    @Test
    @SmallTest
    public void testShowContextMenu() throws TimeoutException {
        ContextMenuParams params = runContextMenuTest("download.html");
        Assert.assertEquals(
                Uri.parse(mActivityTestRule.getTestDataURL("lorem_ipsum.txt")), params.linkUri);
        Assert.assertEquals("anchor text", params.linkText);
    }

    @Test
    @SmallTest
    public void testShowContextMenuImg() throws TimeoutException {
        ContextMenuParams params = runContextMenuTest("img.html");
        Assert.assertEquals(
                Uri.parse(mActivityTestRule.getTestDataURL("favicon.png")), params.srcUri);
        Assert.assertEquals("alt_text", params.titleOrAltText);
    }

    private File setTempDownloadDir() {
        // Don't fill up the default download directory on the device.
        File tempDownloadDirectory = new File(
                InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir()
                + "/weblayer/Downloads");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = mActivityTestRule.getActivity().getBrowser().getProfile();
            profile.setDownloadDirectory(tempDownloadDirectory);
        });
        return tempDownloadDirectory;
    }

    private void waitForFileExist(File filePath, String fileName) {
        File file = new File(filePath, fileName);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat("Invalid file existence state for: " + fileName, file.exists(),
                    Matchers.is(true));
        });
        file.delete();
    }

    @Test
    @SmallTest
    @DisableIf.Build(supported_abis_includes = "x86", sdk_is_greater_than = Build.VERSION_CODES.P,
            message = "https://crbug.com/1201813")
    @DisableIf.Build(supported_abis_includes = "x86_64",
            sdk_is_greater_than = Build.VERSION_CODES.R, message = "https://crbug.com/1201813")
    public void
    testDownloadFromContextMenu() throws TimeoutException {
        ContextMenuParams params = runContextMenuTest("download.html");
        ;
        Assert.assertFalse(params.isImage);
        Assert.assertFalse(params.isVideo);
        Assert.assertTrue(params.canDownload);

        File tempDownloadDirectory = setTempDownloadDir();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getTab().download(params); });
        waitForFileExist(tempDownloadDirectory, "lorem_ipsum.txt");
    }

    @Test
    @SmallTest
    @DisableIf.Build(supported_abis_includes = "x86", sdk_is_greater_than = Build.VERSION_CODES.P,
            message = "https://crbug.com/1201813")
    @DisableIf.Build(supported_abis_includes = "x86_64",
            sdk_is_greater_than = Build.VERSION_CODES.R, message = "https://crbug.com/1201813")
    public void
    testDownloadFromContextMenuImg() throws TimeoutException {
        ContextMenuParams params = runContextMenuTest("img.html");
        ;
        Assert.assertTrue(params.isImage);
        Assert.assertFalse(params.isVideo);
        Assert.assertTrue(params.canDownload);

        File tempDownloadDirectory = setTempDownloadDir();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getTab().download(params); });
        waitForFileExist(tempDownloadDirectory, "favicon.png");
    }

    @Test
    @SmallTest
    public void testTabModalOverlay() throws TimeoutException {
        String pageUrl = mActivityTestRule.getTestDataURL("alert.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(pageUrl);
        Assert.assertNotNull(activity);

        Boolean isTabModalShowingResult[] = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onTabModalStateChanged(boolean isTabModalShowing) {
                    isTabModalShowingResult[0] = isTabModalShowing;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
        });

        int callCount = callbackHelper.getCallCount();
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(true, isTabModalShowingResult[0]);

        callCount = callbackHelper.getCallCount();
        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getTab().dismissTransientUi()));
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(false, isTabModalShowingResult[0]);
    }

    @Test
    @SmallTest
    public void testDismissTransientUi() throws TimeoutException {
        String pageUrl = mActivityTestRule.getTestDataURL("alert.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(pageUrl);
        Assert.assertNotNull(activity);

        Boolean isTabModalShowingResult[] = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onTabModalStateChanged(boolean isTabModalShowing) {
                    isTabModalShowingResult[0] = isTabModalShowing;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
        });

        int callCount = callbackHelper.getCallCount();
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(true, isTabModalShowingResult[0]);

        callCount = callbackHelper.getCallCount();
        Assert.assertTrue(TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getTab().dismissTransientUi()));
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(false, isTabModalShowingResult[0]);

        Assert.assertFalse(TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getTab().dismissTransientUi()));
    }

    @Test
    @SmallTest
    public void testTabModalOverlayOnBackgroundTab() throws TimeoutException {
        // Create a tab.
        String url = mActivityTestRule.getTestDataURL("new_browser.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(activity);
        NewTabCallbackImpl callback = new NewTabCallbackImpl();
        Tab firstTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = activity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
            return tab;
        });

        // Tapping it creates a second tab, which is active.
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());
        callback.waitForNewTab();

        Tab secondTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Assert.assertEquals(2, activity.getBrowser().getTabs().size());
            return activity.getBrowser().getActiveTab();
        });
        Assert.assertNotSame(firstTab, secondTab);

        // Track tab modal updates.
        Boolean isTabModalShowingResult[] = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        int callCount = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            firstTab.registerTabCallback(new TabCallback() {
                @Override
                public void onTabModalStateChanged(boolean isTabModalShowing) {
                    isTabModalShowingResult[0] = isTabModalShowing;
                    callbackHelper.notifyCalled();
                }
            });
        });

        // Create an alert from the background tab. It shouldn't display. There's no way to
        // consistently verify that nothing happens, but the script execution should finish, which
        // is not the case for dialogs that show on an active tab until they're dismissed.
        mActivityTestRule.executeScriptSync("window.alert('foo');", true, firstTab);
        Assert.assertEquals(0, callbackHelper.getCallCount());

        // When that tab becomes active again, the alert should show.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowser().setActiveTab(firstTab); });
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(true, isTabModalShowingResult[0]);

        // Switch away from the tab again; the alert should be hidden.
        callCount = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowser().setActiveTab(secondTab); });
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(false, isTabModalShowingResult[0]);
    }

    @Test
    @SmallTest
    public void testOnTitleUpdated() throws TimeoutException {
        String startupUrl = "about:blank";
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(startupUrl);

        String titles[] = new String[1];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onTitleUpdated(String title) {
                    titles[0] = title;
                }
            };
            tab.registerTabCallback(callback);
        });

        String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.navigateAndWait(url);
        // Use polling because title is allowed to go through multiple transitions.
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(titles[0], Matchers.is("OK")));

        url = mActivityTestRule.getTestDataURL("shakespeare.html");
        mActivityTestRule.navigateAndWait(url);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(titles[0], Matchers.endsWith("shakespeare.html")));

        mActivityTestRule.executeScriptSync("document.title = \"foobar\";", false);
        Assert.assertEquals("foobar", titles[0]);
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(titles[0], Matchers.is("foobar")));
    }

    @Test
    @SmallTest
    public void testOnBackgroundColorChanged() throws TimeoutException {
        String startupUrl = "about:blank";
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(startupUrl);

        Integer backgroundColors[] = new Integer[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onBackgroundColorChanged(int color) {
                    backgroundColors[0] = color;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
        });

        mActivityTestRule.executeScriptSync(
                "document.body.style.backgroundColor = \"rgb(255, 0, 0)\"",
                /*useSeparateIsolate=*/false);

        callbackHelper.waitForFirst();
        Assert.assertEquals(0xffff0000, (int) backgroundColors[0]);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1339982")
    public void testScrollNotificationDirectionChange() throws TimeoutException {
        final String url = mActivityTestRule.getTestDataURL("tall_page.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        Integer notificationTypes[] = new Integer[1];
        Float scrollRatio[] = new Float[1];
        CallbackHelper callbackHelper = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onScrollNotification(
                        @ScrollNotificationType int notificationType, float currentScrollRatio) {
                    notificationTypes[0] = notificationType;
                    scrollRatio[0] = currentScrollRatio;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
        });

        // Scroll to bottom of page.
        int callCount = callbackHelper.getCallCount();
        mActivityTestRule.executeScriptSync("window.scroll(0, 5000)",
                /*useSeparateIsolate=*/false);
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(
                ScrollNotificationType.DIRECTION_CHANGED_DOWN, (int) notificationTypes[0]);
        Assert.assertTrue(scrollRatio[0] > 0.5);

        // Scroll to top of page.
        callCount = callbackHelper.getCallCount();
        mActivityTestRule.executeScriptSync("window.scroll(0, 0)",
                /*useSeparateIsolate=*/false);
        callbackHelper.waitForCallback(callCount);
        Assert.assertEquals(
                ScrollNotificationType.DIRECTION_CHANGED_UP, (int) notificationTypes[0]);
        Assert.assertTrue(scrollRatio[0] < 0.5);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(101)
    public void testOnVerticalOverscroll() throws TimeoutException {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");

        float overscrollY[] = new float[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onVerticalOverscroll(float accumulatedOverscrollY) {
                    overscrollY[0] = accumulatedOverscrollY;
                    callbackHelper.notifyCalled();
                    tab.unregisterTabCallback(this);
                }
            };
            tab.registerTabCallback(callback);
        });

        View decorView[] = new View[1];
        int dimension[] = new int[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            decorView[0] = activity.getWindow().getDecorView();
            dimension[0] = decorView[0].getWidth();
            dimension[1] = decorView[0].getHeight();
        });

        int x = dimension[0] / 2;
        int fromY = dimension[1] / 3;
        int toY = dimension[1] / 3 * 2;

        TestTouchUtils.dragCompleteView(InstrumentationRegistry.getInstrumentation(), decorView[0],
                /*fromX=*/x, /*toX=*/x, fromY, toY, /*stepCount=*/10);
        callbackHelper.waitForFirst();
        Assert.assertThat(overscrollY[0], Matchers.lessThan(0f));
    }
}
