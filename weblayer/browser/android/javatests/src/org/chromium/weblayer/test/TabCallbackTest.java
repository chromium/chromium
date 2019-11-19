// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests that TabCallback methods are invoked as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
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
                        new Criteria() {
                            @Override
                            public boolean isSatisfied() {
                                return mObservedValues.contains(expectation);
                            }
                        },
                        CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                        CriteriaHelper.DEFAULT_POLLING_INTERVAL);
            }
        }

        public TabCallbackValueRecorder visibleUrlChangedCallback = new TabCallbackValueRecorder();

        @Override
        public void onVisibleUrlChanged(Uri url) {
            visibleUrlChangedCallback.recordValue(url.toString());
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
        callback.visibleUrlChangedCallback.waitUntilValueObserved(url);
    }

    @Test
    @SmallTest
    public void testOnRenderProcessGone() throws TimeoutException {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onRenderProcessGone() {
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
            tab.getNavigationController().navigate(Uri.parse("chrome://crash"));
        });
        callbackHelper.waitForFirst();
    }
}
