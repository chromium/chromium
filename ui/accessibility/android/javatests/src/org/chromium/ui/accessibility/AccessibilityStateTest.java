// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.CAPABILITIES_MASK;
import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.CAPABILITIES_MASK_HEURISTIC;
import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.EVENT_TYPE_MASK;
import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.EVENT_TYPE_MASK_HEURISTIC;
import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.FEEDBACK_TYPE_MASK;
import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.FEEDBACK_TYPE_MASK_HEURISTIC;
import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.FLAGS_MASK;
import static org.chromium.ui.accessibility.AccessibilityState.StateIdentifierForTesting.FLAGS_MASK_HEURISTIC;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.view.accessibility.AccessibilityEvent;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

@RunWith(BaseRobolectricTestRunner.class)
public class AccessibilityStateTest {
    private static final String EVENT_TYPE_MASK_ERROR =
            "Conversion of event masks to event types not correct.";

    private static final int MOCK_EVENT_TYPE_MASK =
            AccessibilityEvent.TYPE_VIEW_CLICKED
                    | AccessibilityEvent.TYPE_VIEW_FOCUSED
                    | AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED
                    | AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED;

    private static final int MOCK_FLAG_TYPE_MASK =
            AccessibilityServiceInfo.DEFAULT
                    | AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE
                    | AccessibilityServiceInfo.FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY
                    | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
                    | AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;

    private static final int MOCK_CAPABILITY_TYPE_MASK =
            AccessibilityServiceInfo.CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.getApplication();

        // Reset all flags to empty/default state.
        AccessibilityState.setStateMaskForTesting(EVENT_TYPE_MASK, 0);
        AccessibilityState.setStateMaskForTesting(FEEDBACK_TYPE_MASK, 0);
        AccessibilityState.setStateMaskForTesting(FLAGS_MASK, 0);
        AccessibilityState.setStateMaskForTesting(CAPABILITIES_MASK, 0);
        AccessibilityState.setStateMaskForTesting(EVENT_TYPE_MASK_HEURISTIC, 0);
        AccessibilityState.setStateMaskForTesting(FEEDBACK_TYPE_MASK_HEURISTIC, 0);
        AccessibilityState.setStateMaskForTesting(FLAGS_MASK_HEURISTIC, 0);
        AccessibilityState.setStateMaskForTesting(CAPABILITIES_MASK_HEURISTIC, 0);
    }

    @After
    public void tearDown() {
        AccessibilityState.uninitializeForTesting();
    }

    @Test
    @SmallTest
    public void testSimpleString() {
        String inputString = "placeholder";
        List<String> response = AccessibilityState.getCanonicalizedEnabledServiceNames(inputString);

        Assert.assertNotNull(response);
        Assert.assertFalse(response.isEmpty());
        Assert.assertEquals(1, response.size());
        Assert.assertEquals("placeholder", response.get(0));
    }

    @Test
    @SmallTest
    public void testBadInput() {
        String inputString = "placeholder:::";
        List<String> response = AccessibilityState.getCanonicalizedEnabledServiceNames(inputString);

        Assert.assertNotNull(response);
        Assert.assertFalse(response.isEmpty());
        Assert.assertEquals(1, response.size());
        Assert.assertEquals("placeholder", response.get(0));
    }

    @Test
    @SmallTest
    public void testComplexString() {
        String inputString = "com.google.placeholder.test/com.test.google";
        List<String> response = AccessibilityState.getCanonicalizedEnabledServiceNames(inputString);

        Assert.assertNotNull(response);
        Assert.assertFalse(response.isEmpty());
        Assert.assertEquals(1, response.size());
        Assert.assertEquals("com.google.placeholder.test/com.test.google", response.get(0));
    }

    @Test
    @SmallTest
    public void testMultipleSimpleStrings() {
        String inputString = "placeholder:foo:bar";
        List<String> response = AccessibilityState.getCanonicalizedEnabledServiceNames(inputString);

        Assert.assertNotNull(response);
        Assert.assertFalse(response.isEmpty());
        Assert.assertEquals(3, response.size());
        Assert.assertEquals("placeholder", response.get(0));
        Assert.assertEquals("foo", response.get(1));
        Assert.assertEquals("bar", response.get(2));
    }

    @Test
    @SmallTest
    public void testMulitpleComplexStrings() {
        String inputString =
                "com.google.placeholder.test/com.test.google:"
                        + "placeholder:com.google.test/.classname:com.google.test/test.google";
        List<String> response = AccessibilityState.getCanonicalizedEnabledServiceNames(inputString);

        Assert.assertNotNull(response);
        Assert.assertFalse(response.isEmpty());
        Assert.assertEquals(4, response.size());
        Assert.assertEquals("com.google.placeholder.test/com.test.google", response.get(0));
        Assert.assertEquals("placeholder", response.get(1));
        Assert.assertEquals("com.google.test/.classname", response.get(2));
        Assert.assertEquals("com.google.test/test.google", response.get(3));
    }

    @Test
    @SmallTest
    public void testMulitpleComplexStringsIncludingBadInput() {
        String inputString =
                "com.google.placeholder.test/com.test.google:"
                        + "placeholder::::com.google.test/.classname:::com.google.test/test.google";
        List<String> response = AccessibilityState.getCanonicalizedEnabledServiceNames(inputString);

        Assert.assertNotNull(response);
        Assert.assertFalse(response.isEmpty());
        Assert.assertEquals(4, response.size());
        Assert.assertEquals("com.google.placeholder.test/com.test.google", response.get(0));
        Assert.assertEquals("placeholder", response.get(1));
        Assert.assertEquals("com.google.test/.classname", response.get(2));
        Assert.assertEquals("com.google.test/test.google", response.get(3));
    }

    @Test
    @SmallTest
    public void testEnabledServicesForTesting() {
        String enabledServicesForTesting = "placeholder:services";
        AccessibilityState.setEnabledServiceStringForTesting(enabledServicesForTesting);

        Assert.assertEquals(
                enabledServicesForTesting, AccessibilityState.getEnabledServiceString(null));
    }

    @Test
    @SmallTest
    public void testRunningServicesForTesting() {
        AccessibilityServiceInfo service1 = new AccessibilityServiceInfo();
        AccessibilityServiceInfo service2 = new AccessibilityServiceInfo();
        List<AccessibilityServiceInfo> serviceInfoList = new ArrayList<AccessibilityServiceInfo>();
        serviceInfoList.add(service1);
        serviceInfoList.add(service2);
        AccessibilityState.setEnabledServiceInfoListForTesting(serviceInfoList);

        List<AccessibilityServiceInfo> runningServices =
                AccessibilityState.getRunningServiceInfoList();
        Assert.assertNotNull(runningServices);
        Assert.assertFalse(runningServices.isEmpty());
        Assert.assertEquals(2, runningServices.size());
        Assert.assertEquals(service1, runningServices.get(0));
        Assert.assertEquals(service2, runningServices.get(1));
    }

    /** Test logic for converting event type masks to a list of relevant event types. */
    @Test
    @SmallTest
    public void testMaskToEventTypeConversion() {
        // Create some event masks with known outcomes.
        int serviceEventMask_empty = 0;
        int serviceEventMask_full = Integer.MAX_VALUE;
        int serviceEventMask_test =
                AccessibilityEvent.TYPE_VIEW_CLICKED
                        | AccessibilityEvent.TYPE_VIEW_LONG_CLICKED
                        | AccessibilityEvent.TYPE_VIEW_FOCUSED
                        | AccessibilityEvent.TYPE_VIEW_SCROLLED
                        | AccessibilityEvent.TYPE_VIEW_SELECTED
                        | AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END;

        // Convert each mask to a set of eventTypes.
        AccessibilityState.setStateMaskForTesting(EVENT_TYPE_MASK, serviceEventMask_empty);
        Set<Integer> outcome_empty = AccessibilityState.relevantEventTypesForCurrentServices();

        AccessibilityState.setStateMaskForTesting(EVENT_TYPE_MASK, serviceEventMask_full);
        Set<Integer> outcome_full = AccessibilityState.relevantEventTypesForCurrentServices();

        AccessibilityState.setStateMaskForTesting(EVENT_TYPE_MASK, serviceEventMask_test);
        Set<Integer> outcome_test = AccessibilityState.relevantEventTypesForCurrentServices();

        // Verify results.
        Assert.assertNotNull(EVENT_TYPE_MASK_ERROR, outcome_empty);
        Assert.assertTrue(EVENT_TYPE_MASK_ERROR, outcome_empty.isEmpty());

        Assert.assertNotNull(EVENT_TYPE_MASK_ERROR, outcome_full);
        Assert.assertEquals(EVENT_TYPE_MASK_ERROR, 31, outcome_full.size());

        Set<Integer> expected_test =
                new HashSet<Integer>(
                        Arrays.asList(
                                AccessibilityEvent.TYPE_VIEW_CLICKED,
                                AccessibilityEvent.TYPE_VIEW_LONG_CLICKED,
                                AccessibilityEvent.TYPE_VIEW_FOCUSED,
                                AccessibilityEvent.TYPE_VIEW_SCROLLED,
                                AccessibilityEvent.TYPE_VIEW_SELECTED,
                                AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END));

        Assert.assertNotNull(EVENT_TYPE_MASK_ERROR, outcome_test);
        Assert.assertEquals(EVENT_TYPE_MASK_ERROR, expected_test, outcome_test);
    }

    @Test
    @SmallTest
    public void testAreOnlyPasswordManagerFlagsRequested_empty() {
        Assert.assertFalse(AccessibilityState.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testAreOnlyPasswordManagerFlagsRequested_true() {
        AccessibilityState.setStateMaskForTesting(
                EVENT_TYPE_MASK_HEURISTIC, AccessibilityState.PASSWORD_MANAGER_EVENT_TYPE_MASK);
        AccessibilityState.setStateMaskForTesting(
                FLAGS_MASK_HEURISTIC, AccessibilityState.PASSWORD_MANAGER_FLAG_TYPE_MASK);
        AccessibilityState.setStateMaskForTesting(
                CAPABILITIES_MASK_HEURISTIC,
                AccessibilityState.PASSWORD_MANAGER_CAPABILITY_TYPE_MASK);

        Assert.assertTrue(AccessibilityState.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testAreOnlyPasswordManagerFlagsRequested_missingFlags() {
        AccessibilityState.setStateMaskForTesting(
                EVENT_TYPE_MASK_HEURISTIC, AccessibilityState.PASSWORD_MANAGER_EVENT_TYPE_MASK);

        int flags_mask =
                AccessibilityServiceInfo.DEFAULT
                        | AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
                        | AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE;
        // Do not add the following to make sure we don't get false negatives:
        // | AccessibilityServiceInfo.FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY
        // | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
        // | AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;

        AccessibilityState.setStateMaskForTesting(FLAGS_MASK_HEURISTIC, flags_mask);
        AccessibilityState.setStateMaskForTesting(
                CAPABILITIES_MASK_HEURISTIC,
                AccessibilityState.PASSWORD_MANAGER_CAPABILITY_TYPE_MASK);

        Assert.assertTrue(AccessibilityState.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testAreOnlyPasswordManagerFlagsRequested_extraFlags() {
        AccessibilityState.setStateMaskForTesting(
                EVENT_TYPE_MASK_HEURISTIC, AccessibilityState.PASSWORD_MANAGER_EVENT_TYPE_MASK);

        int flags_mask =
                AccessibilityServiceInfo.DEFAULT
                        | AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
                        | AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE
                        | AccessibilityServiceInfo.FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY
                        | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
                        | AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS
                        // Add extra flag to make sure we don't get false positives:
                        | AccessibilityServiceInfo.FLAG_ENABLE_ACCESSIBILITY_VOLUME;

        AccessibilityState.setStateMaskForTesting(FLAGS_MASK_HEURISTIC, flags_mask);
        AccessibilityState.setStateMaskForTesting(
                CAPABILITIES_MASK_HEURISTIC,
                AccessibilityState.PASSWORD_MANAGER_CAPABILITY_TYPE_MASK);

        Assert.assertFalse(AccessibilityState.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testCalculateHeuristicState_Autofill_passwordManager() {
        AccessibilityServiceInfo myService =
                new BuilderForTests(mContext)
                        .setPackageName("android")
                        .setClassName(
                                "com.android.server.autofill.AutofillCompatAccessibilityService")
                        .setEventTypes(AccessibilityState.PASSWORD_MANAGER_EVENT_TYPE_MASK)
                        .setFlags(AccessibilityState.PASSWORD_MANAGER_FLAG_TYPE_MASK)
                        .setCapabilities(AccessibilityState.PASSWORD_MANAGER_CAPABILITY_TYPE_MASK)
                        .build();
        startTestWithService(
                myService,
                "android/com.android.server.autofill.AutofillCompatAccessibilityService");

        AccessibilityState.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        Assert.assertFalse(AccessibilityState.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testCalculateHeuristicState_notAutofill_notPasswordManager() {
        AccessibilityServiceInfo myService =
                new BuilderForTests(mContext)
                        .setEventTypes(~0)
                        .setFlags(~0)
                        .setCapabilities(~0)
                        .build();
        startTestWithService(myService);

        AccessibilityState.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        Assert.assertFalse(AccessibilityState.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testCalculateHeuristicState_notAutofill_passwordManager() {
        AccessibilityServiceInfo myService =
                new BuilderForTests(mContext)
                        .setEventTypes(AccessibilityState.PASSWORD_MANAGER_EVENT_TYPE_MASK)
                        .setFlags(AccessibilityState.PASSWORD_MANAGER_FLAG_TYPE_MASK)
                        .setCapabilities(AccessibilityState.PASSWORD_MANAGER_CAPABILITY_TYPE_MASK)
                        .build();
        startTestWithService(myService);

        AccessibilityState.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        Assert.assertTrue(AccessibilityState.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testTogglingMisconfiguredAccessibilityServices() {
        // This service has the same config as Microsoft Authenticator during recent P0.
        AccessibilityServiceInfo errorProneService =
                new BuilderForTests(mContext)
                        .setEventTypes(MOCK_EVENT_TYPE_MASK)
                        .setFlags(MOCK_FLAG_TYPE_MASK)
                        .setCapabilities(MOCK_CAPABILITY_TYPE_MASK)
                        .build();

        // This service has the correct config for a password manager.
        AccessibilityServiceInfo properConfigService =
                new BuilderForTests(mContext)
                        .setEventTypes(MOCK_EVENT_TYPE_MASK)
                        .setFlags(MOCK_FLAG_TYPE_MASK)
                        .setCapabilities(
                                MOCK_CAPABILITY_TYPE_MASK
                                        | AccessibilityServiceInfo
                                                .CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION)
                        .build();

        startTestWithService(errorProneService);

        AccessibilityState.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        // Before P0 fix, this call would have (incorrectly) returned true.
        Assert.assertFalse(AccessibilityState.isTouchExplorationEnabled());

        // Now enable the proper config, and ensure we do not enter an infinite loop and that
        // we now show touch exploration as being enabled.
        AccessibilityState.setEnabledServiceInfoListForTesting(List.of(properConfigService));

        AccessibilityState.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        Assert.assertTrue(AccessibilityState.isTouchExplorationEnabled());
    }

    private void startTestWithService(AccessibilityServiceInfo newService) {
        startTestWithService(
                newService, "com.example.google/app.accessibility.AccessibilityService");
    }

    private void startTestWithService(AccessibilityServiceInfo newService, String serviceName) {
        Assert.assertNotNull(newService);
        Assert.assertFalse(AccessibilityState.isAnyAccessibilityServiceEnabled());
        AccessibilityState.setEnabledServiceInfoListForTesting(List.of(newService));
        AccessibilityState.setEnabledServiceStringForTesting(serviceName);
    }

    public static class BuilderForTests {

        private Context mContext;
        private String mPackageName = "com.example.google";
        private String mClassName = "app.accessibility.AccessibilityService";
        private int mEventTypes;
        private int mFeedbackType = AccessibilityServiceInfo.FEEDBACK_GENERIC;
        private int mFlags;
        private int mCapabilities;

        public BuilderForTests(Context context) {
            this.mContext = context;
        }

        public BuilderForTests setPackageName(String packageName) {
            this.mPackageName = packageName;
            return this;
        }

        public BuilderForTests setClassName(String className) {
            this.mClassName = className;
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
            ServiceInfo serviceInfo = new ServiceInfo();
            serviceInfo.packageName = mPackageName;
            serviceInfo.name = mClassName;
            serviceInfo.flags = ServiceInfo.FLAG_SINGLE_USER;

            ResolveInfo resolveInfo = new ResolveInfo();
            resolveInfo.serviceInfo = serviceInfo;

            AccessibilityServiceInfo service =
                    constructAccessibilityServiceInfo(resolveInfo, mContext);
            setCapabilities(service, mCapabilities);
            assert service != null;
            service.eventTypes = mEventTypes;
            service.feedbackType = mFeedbackType;
            service.flags = mFlags;

            return service;
        }

        private void setCapabilities(AccessibilityServiceInfo info, int capabilities) {
            try {
                Method setResolveInfoMethod =
                        AccessibilityServiceInfo.class.getMethod("setCapabilities", int.class);
                setResolveInfoMethod.invoke(info, capabilities);
            } catch (Exception ex) {
                throw new AssertionError(
                        "Unable to call AccessibilityServiceInfo hidden method.", ex);
            }
        }

        private AccessibilityServiceInfo constructAccessibilityServiceInfo(
                ResolveInfo resolveInfo, Context context) {
            try {
                Constructor<AccessibilityServiceInfo> ctr =
                        AccessibilityServiceInfo.class.getConstructor(
                                ResolveInfo.class, Context.class);
                return ctr.newInstance(resolveInfo, context);
            } catch (Exception ex) {
                throw new AssertionError(
                        "Unable to call AccessibilityServiceInfo hidden method.", ex);
            }
        }
    }
}
