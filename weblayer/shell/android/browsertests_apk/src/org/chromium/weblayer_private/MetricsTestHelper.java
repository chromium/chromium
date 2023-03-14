// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer.TestProfile;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.WebLayer;

/**
 * Helper for metrics_browsertest.cc
 */
@JNINamespace("weblayer")
class MetricsTestHelper {
    private static class TestGmsBridge extends GmsBridge {
        private final @ConsentType int mConsentType;
        private Callback<Boolean> mConsentCallback;
        public static TestGmsBridge sInstance;

        public TestGmsBridge(@ConsentType int consentType) {
            sInstance = this;
            mConsentType = consentType;
        }

        @Override
        public boolean canUseGms() {
            return true;
        }

        @Override
        public void setSafeBrowsingHandler() {
            // We don't have this specialized service here.
        }

        @Override
        public void queryMetricsSetting(Callback<Boolean> callback) {
            ThreadUtils.assertOnUiThread();
            if (mConsentType == ConsentType.DELAY_CONSENT) {
                mConsentCallback = callback;
            } else {
                callback.onResult(mConsentType == ConsentType.CONSENT);
            }
        }

        @Override
        public void logMetrics(byte[] data) {
            MetricsTestHelperJni.get().onLogMetrics(data);
        }
    }

    @CalledByNative
    private static void installTestGmsBridge(@ConsentType int consentType) {
        GmsBridge.injectInstance(new TestGmsBridge(consentType));
    }

    @CalledByNative
    private static void runConsentCallback(boolean hasConsent) {
        assert TestGmsBridge.sInstance != null;
        assert TestGmsBridge.sInstance.mConsentCallback != null;
        TestGmsBridge.sInstance.mConsentCallback.onResult(hasConsent);
    }

    @CalledByNative
    private static void createProfile(String name, boolean incognito) {
        Context appContext = ContextUtils.getApplicationContext();
        WebLayer weblayer = TestWebLayer.loadSync(appContext);

        if (incognito) {
            String nameOrNull = null;
            if (!TextUtils.isEmpty(name)) nameOrNull = name;
            weblayer.getIncognitoProfile(nameOrNull);
        } else {
            weblayer.getProfile(name);
        }
    }

    @CalledByNative
    private static void destroyProfile(String name, boolean incognito) {
        Context appContext = ContextUtils.getApplicationContext();
        WebLayer weblayer = TestWebLayer.loadSync(appContext);

        if (incognito) {
            String nameOrNull = null;
            if (!TextUtils.isEmpty(name)) nameOrNull = name;
            TestProfile.destroy(weblayer.getIncognitoProfile(nameOrNull));
        } else {
            TestProfile.destroy(weblayer.getProfile(name));
        }
    }

    @CalledByNative
    private static void removeTestGmsBridge() {
        GmsBridge.injectInstance(null);
    }

    @NativeMethods
    interface Natives {
        void onLogMetrics(byte[] data);
    }
}
