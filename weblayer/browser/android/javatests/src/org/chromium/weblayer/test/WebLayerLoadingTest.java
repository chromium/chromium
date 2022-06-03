// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Callback;
import org.chromium.weblayer.UnsupportedVersionException;
import org.chromium.weblayer.WebLayer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link Weblayer#createAsync} and {@link Weblayer#createSync}.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class WebLayerLoadingTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @Test
    @SmallTest
    public void loadsSync() {
        assertNotNull(loadSync());
    }

    @Test
    @SmallTest
    public void loadsAsync() {
        loadAsyncAndWait(webLayer -> { assertNotNull(webLayer); });
    }

    @Test
    @SmallTest
    public void twoSequentialAsyncLoadsYieldSameInstance() {
        loadAsyncAndWait(webLayer1 -> {
            loadAsyncAndWait(webLayer2 -> { assertEquals(webLayer1, webLayer2); });
        });
    }

    @Test
    @SmallTest
    public void twoParallelAsyncLoadsYieldSameInstance() {
        List<WebLayer> asyncResults = new ArrayList<>();
        for (int i = 0; i < 2; i++) {
            loadAsyncAndWait(webLayer -> { asyncResults.add(webLayer); });
        }
        assertEquals(asyncResults.get(0), asyncResults.get(1));
    }

    @Test
    @SmallTest
    public void syncLoadAfterAsyncLoadYieldsTheSameInstance() {
        loadAsyncAndWait(webLayer1 -> {
            WebLayer webLayer2 = loadSync();
            assertEquals(webLayer1, webLayer2);
        });
    }

    @Test
    @SmallTest
    public void asyncLoadAfterSyncLoadYieldsTheSameInstance() {
        WebLayer webLayer1 = loadSync();
        loadAsyncAndWait(webLayer2 -> { assertEquals(webLayer1, webLayer2); });
    }

    @Test
    @SmallTest
    public void syncLoadDuringAsyncLoadYieldsTheSameInstance() throws Exception {
        List<WebLayer> asyncResults = new ArrayList<>();
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                WebLayer.loadAsync(mContext, webLayer -> {
                    asyncResults.add(webLayer);
                    callbackHelper.notifyCalled();
                });
            } catch (UnsupportedVersionException e) {
                throw new RuntimeException(e);
            }
        });
        WebLayer webLayer2 = loadSync();
        callbackHelper.waitForFirst();
        assertEquals(asyncResults.get(0), webLayer2);
    }

    @Test
    @SmallTest
    public void twoSyncLoadsYieldSameInstance() {
        assertEquals(loadSync(), loadSync());
    }

    private WebLayer loadSync() {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(() -> WebLayer.loadSync(mContext));
        } catch (ExecutionException e) {
            throw new RuntimeException(e.getCause());
        }
    }

    private void loadAsyncAndWait(Callback<WebLayer> callback) {
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                WebLayer.loadAsync(mContext, webLayer -> {
                    callback.onResult(webLayer);
                    callbackHelper.notifyCalled();
                });
            } catch (UnsupportedVersionException e) {
                throw new RuntimeException(e);
            }
        });
        try {
            callbackHelper.waitForFirst();
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        }
    }
}
