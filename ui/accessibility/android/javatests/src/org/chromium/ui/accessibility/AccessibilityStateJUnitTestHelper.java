// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityManager;

import org.mockito.Mockito;
import org.robolectric.Shadows;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowAccessibilityManager;

import java.util.List;

/** Robolectric-specific helper for testing AccessibilityState. */
public class AccessibilityStateJUnitTestHelper {
    public static class BuilderForTests {
        private String mId = "com.example.google/app.accessibility.AccessibilityService";
        private int mEventTypes;
        private int mFeedbackType = AccessibilityServiceInfo.FEEDBACK_GENERIC;
        private int mFlags;
        private int mCapabilities;

        public BuilderForTests setId(String id) {
            this.mId = id;
            return this;
        }

        public BuilderForTests setEventTypes(int eventTypes) {
            this.mEventTypes = eventTypes;
            return this;
        }

        public BuilderForTests setFeedbackType(int feedbackType) {
            this.mFeedbackType = feedbackType;
            return this;
        }

        public BuilderForTests setFlags(int flags) {
            this.mFlags = flags;
            return this;
        }

        public BuilderForTests setCapabilities(int capabilities) {
            this.mCapabilities = capabilities;
            return this;
        }

        public AccessibilityServiceInfo build() {
            AccessibilityServiceInfo service = new AccessibilityServiceInfo();
            service.eventTypes = mEventTypes;
            service.feedbackType = mFeedbackType;
            service.flags = mFlags;

            ShadowAccessibilityServiceInfo shadow = Shadow.extract(service);
            shadow.mCapabilities = mCapabilities;
            shadow.mId = mId;

            return service;
        }
    }

    @Implements(AccessibilityServiceInfo.class)
    public static class ShadowAccessibilityServiceInfo {
        public int mCapabilities;
        public String mId;

        @Implementation
        protected int getCapabilities() {
            return mCapabilities;
        }

        @Implementation
        protected String getId() {
            return mId;
        }

        @Implementation
        protected boolean isAccessibilityTool() {
            return false;
        }
    }

    /**
     * Sets the enabled accessibility services list in ShadowAccessibilityManager and also updates
     * the Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES setting.
     */
    public static void setEnabledAccessibilityServiceList(
            Context context, List<AccessibilityServiceInfo> services) {
        ShadowAccessibilityManager shadowManager =
                Shadows.shadowOf(
                        (AccessibilityManager)
                                context.getSystemService(Context.ACCESSIBILITY_SERVICE));
        shadowManager.setEnabledAccessibilityServiceList(services);

        StringBuilder sb = new StringBuilder();
        for (AccessibilityServiceInfo service : services) {
            String id = service.getId();
            if (!TextUtils.isEmpty(id)) {
                if (sb.length() > 0) {
                    sb.append(":");
                }
                sb.append(id);
            }
        }
        Settings.Secure.putString(
                context.getContentResolver(),
                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES,
                sb.length() > 0 ? sb.toString() : null);
    }

    /** Updates the cached accessibility services in {@link AccessibilityState}. */
    public static void updateAccessibilityServices() {
        AccessibilityState.getDelegate().updateAccessibilityServices();
    }

    /** Sets the JNI instance for AccessibilityState. */
    public static void mockAccessibilityStateJni() {
        AccessibilityStateJni.setInstanceForTesting(Mockito.mock(AccessibilityState.Natives.class));
    }
}
