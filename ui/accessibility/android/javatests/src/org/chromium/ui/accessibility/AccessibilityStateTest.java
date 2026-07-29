// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import static com.google.common.truth.Truth.assertThat;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Activity;
import android.app.Application;
import android.content.ContentResolver;
import android.content.Context;
import android.provider.Settings;
import android.view.accessibility.AccessibilityEvent;

import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableSet;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSettings;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.ui.accessibility.AccessibilityStateTestHelper.BuilderForTests;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {AccessibilityStateTestHelper.ShadowAccessibilityServiceInfo.class})
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
    private AccessibilityStateDelegateImpl mDelegate;
    @Mock private AccessibilityState.Natives mAccessibilityStateNatives;
    private AutoCloseable mCloseableMocks;

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);
        AccessibilityStateJni.setInstanceForTesting(mAccessibilityStateNatives);
        mContext = RuntimeEnvironment.getApplication();
        mDelegate = AccessibilityState.getDelegate();

        // Reset all flags to empty/default state.
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(mContext, new ArrayList<>());
        mDelegate.updateAccessibilityServices();
    }

    @After
    public void tearDown() throws Exception {
        mCloseableMocks.close();
        AccessibilityState.uninitializeForTesting();
    }

    private AccessibilityServiceInfo createPasswordManagerServiceInfoWithFlags(int flags) {
        return createPasswordManagerServiceInfoBuilderWithFlags(flags).build();
    }

    private BuilderForTests createPasswordManagerServiceInfoBuilderWithFlags(int flags) {
        return new BuilderForTests()
                .setCapabilities(
                        AccessibilityStateDelegateImpl.PASSWORD_MANAGER_CAPABILITY_TYPE_MASK)
                .setEventTypes(AccessibilityStateDelegateImpl.PASSWORD_MANAGER_EVENT_TYPE_MASK)
                .setFlags(flags);
    }

    private void simulateActivityStateChange(
            Activity activity,
            @ActivityState int activityState,
            @ApplicationState int expectedApplicationState) {
        ApplicationStatus.onStateChangeForTesting(activity, activityState);
        assertThat(ApplicationStatus.getStateForApplication()).isEqualTo(expectedApplicationState);
    }

    @Test
    @SmallTest
    public void testSimpleString() {
        String inputString = "placeholder";
        List<String> response =
                AccessibilityStateDelegateImpl.getCanonicalizedEnabledServiceNames(inputString);

        Assert.assertNotNull(response);
        Assert.assertFalse(response.isEmpty());
        Assert.assertEquals(1, response.size());
        Assert.assertEquals("placeholder", response.get(0));
    }

    @Test
    @SmallTest
    public void testBadInput() {
        String inputString = "placeholder:::";
        List<String> response =
                AccessibilityStateDelegateImpl.getCanonicalizedEnabledServiceNames(inputString);

        Assert.assertNotNull(response);
        Assert.assertFalse(response.isEmpty());
        Assert.assertEquals(1, response.size());
        Assert.assertEquals("placeholder", response.get(0));
    }

    @Test
    @SmallTest
    public void testComplexString() {
        String inputString = "com.google.placeholder.test/com.test.google";
        List<String> response =
                AccessibilityStateDelegateImpl.getCanonicalizedEnabledServiceNames(inputString);

        Assert.assertNotNull(response);
        Assert.assertFalse(response.isEmpty());
        Assert.assertEquals(1, response.size());
        Assert.assertEquals("com.google.placeholder.test/com.test.google", response.get(0));
    }

    @Test
    @SmallTest
    public void testMultipleSimpleStrings() {
        String inputString = "placeholder:foo:bar";
        List<String> response =
                AccessibilityStateDelegateImpl.getCanonicalizedEnabledServiceNames(inputString);

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
        List<String> response =
                AccessibilityStateDelegateImpl.getCanonicalizedEnabledServiceNames(inputString);

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
        List<String> response =
                AccessibilityStateDelegateImpl.getCanonicalizedEnabledServiceNames(inputString);

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
    public void testEnabledServices() {
        String enabledServices = "placeholder:services";
        AccessibilityServiceInfo service1 = new BuilderForTests().setId("placeholder").build();
        AccessibilityServiceInfo service2 = new BuilderForTests().setId("services").build();
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(
                mContext, List.of(service1, service2));

        Assert.assertEquals(enabledServices, mDelegate.getEnabledServiceString(mContext));
    }

    @Test
    @SmallTest
    public void testRunningServices() {
        AccessibilityServiceInfo service1 = new AccessibilityServiceInfo();
        AccessibilityServiceInfo service2 = new AccessibilityServiceInfo();
        List<AccessibilityServiceInfo> serviceInfoList = new ArrayList<>();
        serviceInfoList.add(service1);
        serviceInfoList.add(service2);
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(mContext, serviceInfoList);

        List<AccessibilityServiceInfo> runningServices = mDelegate.getRunningServiceInfoList();
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
        AccessibilityServiceInfo serviceEmpty =
                new BuilderForTests().setEventTypes(serviceEventMask_empty).build();
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(mContext, List.of(serviceEmpty));
        mDelegate.updateAccessibilityServices();
        Set<Integer> outcome_empty = AccessibilityState.relevantEventTypesForCurrentServices();

        AccessibilityServiceInfo serviceFull =
                new BuilderForTests().setEventTypes(serviceEventMask_full).build();
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(mContext, List.of(serviceFull));
        mDelegate.updateAccessibilityServices();
        Set<Integer> outcome_full = AccessibilityState.relevantEventTypesForCurrentServices();

        AccessibilityServiceInfo serviceTest =
                new BuilderForTests().setEventTypes(serviceEventMask_test).build();
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(mContext, List.of(serviceTest));
        mDelegate.updateAccessibilityServices();
        Set<Integer> outcome_test = AccessibilityState.relevantEventTypesForCurrentServices();

        // Verify results.
        Assert.assertNotNull(EVENT_TYPE_MASK_ERROR, outcome_empty);
        Assert.assertTrue(EVENT_TYPE_MASK_ERROR, outcome_empty.isEmpty());

        Assert.assertNotNull(EVENT_TYPE_MASK_ERROR, outcome_full);
        Assert.assertEquals(EVENT_TYPE_MASK_ERROR, 31, outcome_full.size());

        Set<Integer> expected_test =
                new HashSet<>(
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
        Assert.assertFalse(mDelegate.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testAreOnlyPasswordManagerFlagsRequested_true() {
        AccessibilityServiceInfo passwordManagerService =
                createPasswordManagerServiceInfoWithFlags(
                        AccessibilityStateDelegateImpl.PASSWORD_MANAGER_FLAG_TYPE_MASK);
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(
                mContext, List.of(passwordManagerService));
        mDelegate.updateAccessibilityServices();

        Assert.assertTrue(mDelegate.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testAreOnlyPasswordManagerFlagsRequested_missingFlags() {
        int flags_mask =
                AccessibilityServiceInfo.DEFAULT
                        | AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
                        | AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE;
        // Do not add the following to make sure we don't get false negatives:
        // | AccessibilityServiceInfo.FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY
        // | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
        // | AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;

        AccessibilityServiceInfo passwordManagerService =
                createPasswordManagerServiceInfoWithFlags(flags_mask);
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(
                mContext, List.of(passwordManagerService));
        mDelegate.updateAccessibilityServices();

        Assert.assertTrue(mDelegate.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testAreOnlyPasswordManagerFlagsRequested_extraFlags() {
        int flags_mask =
                AccessibilityServiceInfo.DEFAULT
                        | AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS
                        | AccessibilityServiceInfo.FLAG_REQUEST_TOUCH_EXPLORATION_MODE
                        | AccessibilityServiceInfo.FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY
                        | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS
                        | AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS
                        // Add extra flag to make sure we don't get false positives:
                        | AccessibilityServiceInfo.FLAG_ENABLE_ACCESSIBILITY_VOLUME;

        AccessibilityServiceInfo passwordManagerService =
                createPasswordManagerServiceInfoWithFlags(flags_mask);
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(
                mContext, List.of(passwordManagerService));
        mDelegate.updateAccessibilityServices();

        Assert.assertFalse(mDelegate.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testCalculateHeuristicState_Autofill_passwordManager() {
        AccessibilityServiceInfo myService =
                createPasswordManagerServiceInfoBuilderWithFlags(
                                AccessibilityStateDelegateImpl.PASSWORD_MANAGER_FLAG_TYPE_MASK)
                        .setId(AccessibilityState.AUTOFILL_COMPAT_ACCESSIBILITY_SERVICE_ID)
                        .build();
        startTestWithService(myService);

        mDelegate.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        Assert.assertFalse(mDelegate.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testCalculateHeuristicState_notAutofill_notPasswordManager() {
        AccessibilityServiceInfo myService =
                new BuilderForTests().setEventTypes(~0).setFlags(~0).setCapabilities(~0).build();
        startTestWithService(myService);

        mDelegate.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        Assert.assertFalse(mDelegate.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testCalculateHeuristicState_notAutofill_passwordManager() {
        AccessibilityServiceInfo myService =
                new BuilderForTests()
                        .setEventTypes(
                                AccessibilityStateDelegateImpl.PASSWORD_MANAGER_EVENT_TYPE_MASK)
                        .setFlags(AccessibilityStateDelegateImpl.PASSWORD_MANAGER_FLAG_TYPE_MASK)
                        .setCapabilities(
                                AccessibilityStateDelegateImpl
                                        .PASSWORD_MANAGER_CAPABILITY_TYPE_MASK)
                        .build();
        startTestWithService(myService);

        mDelegate.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        Assert.assertTrue(mDelegate.areOnlyPasswordManagerMasksRequested());
    }

    @Test
    @SmallTest
    public void testTogglingMisconfiguredAccessibilityServices() {
        // This service has the same config as Microsoft Authenticator during recent P0.
        AccessibilityServiceInfo errorProneService =
                new BuilderForTests()
                        .setEventTypes(MOCK_EVENT_TYPE_MASK)
                        .setFlags(MOCK_FLAG_TYPE_MASK)
                        .setCapabilities(MOCK_CAPABILITY_TYPE_MASK)
                        .build();

        // This service has the correct config for a password manager.
        AccessibilityServiceInfo properConfigService =
                new BuilderForTests()
                        .setEventTypes(MOCK_EVENT_TYPE_MASK)
                        .setFlags(MOCK_FLAG_TYPE_MASK)
                        .setCapabilities(
                                MOCK_CAPABILITY_TYPE_MASK
                                        | AccessibilityServiceInfo
                                                .CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION)
                        .build();

        startTestWithService(errorProneService);

        mDelegate.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        // Before P0 fix, this call would have (incorrectly) returned true.
        Assert.assertFalse(AccessibilityState.isTouchExplorationEnabled());

        // Now enable the proper config, and ensure we do not enter an infinite loop and that
        // we now show touch exploration as being enabled.
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(
                mContext, List.of(properConfigService));

        mDelegate.updateAccessibilityServices();

        Assert.assertTrue(AccessibilityState.isAnyAccessibilityServiceEnabled());
        Assert.assertTrue(AccessibilityState.isTouchExplorationEnabled());
    }

    @Test
    @SmallTest
    public void testRelevantEventTypesUpdatedWhenServiceRegistered() {
        AccessibilityState.registerObservers();

        // Check initial state.
        Assert.assertTrue(AccessibilityState.relevantEventTypesForCurrentServices().isEmpty());

        // Register new accessibility service.
        int newServiceEventMask =
                AccessibilityEvent.TYPE_VIEW_CLICKED | AccessibilityEvent.TYPE_VIEW_FOCUSED;
        AccessibilityServiceInfo newService =
                new BuilderForTests().setEventTypes(newServiceEventMask).build();
        startTestWithService(newService);
        RobolectricUtil.runAllBackgroundAndUi();

        Set<Integer> expectedEventTypes =
                ImmutableSet.of(
                        AccessibilityEvent.TYPE_VIEW_CLICKED, AccessibilityEvent.TYPE_VIEW_FOCUSED);
        Assert.assertEquals(
                expectedEventTypes, AccessibilityState.relevantEventTypesForCurrentServices());
    }

    /**
     * Test that AccessibilityState#getAnimatorDurationScale() uses the cached value if one is
     * available.
     */
    @Test
    @SmallTest
    @Config(shadows = {CountAnimatorDurationScaleShadowSettingsSecure.class})
    public void testPrefersReducedMotionUsesCachedValue() throws Exception {
        CountAnimatorDurationScaleShadowSettingsSecure.sNumAnimatorDurationGets = 0;

        Settings.Global.putFloat(
                mContext.getContentResolver(), Settings.Global.ANIMATOR_DURATION_SCALE, 14.0f);

        assertThat(AccessibilityState.getAnimatorDurationScale()).isWithin(0.1f).of(14.0f);
        // Should use cached value for second call
        assertThat(AccessibilityState.getAnimatorDurationScale()).isWithin(0.1f).of(14.0f);
        assertThat(CountAnimatorDurationScaleShadowSettingsSecure.sNumAnimatorDurationGets)
                .isEqualTo(1);

        CountAnimatorDurationScaleShadowSettingsSecure.sNumAnimatorDurationGets = 0;
    }

    /** Test that Chromium ignores accessibility state changes when its in the background. */
    @Test
    @SmallTest
    public void testApplicationStateChange() {
        Activity mockActivity = Robolectric.buildActivity(Activity.class).setup().get();

        Application application = (Application) mContext.getApplicationContext();

        // App starts out in foreground.
        simulateActivityStateChange(
                mockActivity, ActivityState.STARTED, ApplicationState.HAS_RUNNING_ACTIVITIES);
        AccessibilityState.initializeOnStartup();
        AccessibilityState.registerObservers();

        // Verify initial call from initializeOnStartup().
        Mockito.verify(mAccessibilityStateNatives, Mockito.times(1))
                .onAnimatorDurationScaleChanged();

        ContentResolver contentResolver = mContext.getContentResolver();
        Settings.Global.putFloat(contentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 14.0f);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify call after setting scale in foreground.
        Mockito.verify(mAccessibilityStateNatives, Mockito.times(2))
                .onAnimatorDurationScaleChanged();

        // Move app to background, state changes should be ignored.
        simulateActivityStateChange(
                mockActivity, ActivityState.STOPPED, ApplicationState.HAS_STOPPED_ACTIVITIES);
        Settings.Global.putFloat(contentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 10.0f);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify no extra calls while in background.
        Mockito.verify(mAccessibilityStateNatives, Mockito.times(2))
                .onAnimatorDurationScaleChanged();

        // Move app to foreground, state changes should be picked up by observers.
        simulateActivityStateChange(
                mockActivity, ActivityState.RESUMED, ApplicationState.HAS_RUNNING_ACTIVITIES);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Mockito.verify(mAccessibilityStateNatives, Mockito.times(3))
                .onAnimatorDurationScaleChanged();

        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        ApplicationStatus.destroyForJUnitTests();
    }

    @Test
    @SmallTest
    public void testUninitialize() {
        Activity mockActivity = Robolectric.buildActivity(Activity.class).setup().get();

        // Initialize and register observers.
        AccessibilityState.initializeOnStartup();
        AccessibilityState.registerObservers();

        // Verify initial call from initializeOnStartup().
        Mockito.verify(mAccessibilityStateNatives, Mockito.times(1))
                .onAnimatorDurationScaleChanged();

        // Verify observer is notified when activity is resumed.
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.RESUMED);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Mockito.verify(mAccessibilityStateNatives, Mockito.times(2))
                .onAnimatorDurationScaleChanged();

        // Verify that observer is not notified when activity is resumed after uninitialization.
        AccessibilityState.uninitializeForTesting();
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.PAUSED);
        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.RESUMED);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Mockito.verify(mAccessibilityStateNatives, Mockito.times(2))
                .onAnimatorDurationScaleChanged();

        ApplicationStatus.onStateChangeForTesting(mockActivity, ActivityState.STOPPED);
        ApplicationStatus.destroyForJUnitTests();
    }

    private void startTestWithService(AccessibilityServiceInfo newService) {
        Assert.assertNotNull(newService);
        Assert.assertFalse(AccessibilityState.isAnyAccessibilityServiceEnabled());
        AccessibilityStateTestHelper.setEnabledAccessibilityServiceList(mContext, List.of(newService));
    }

    @Implements(Settings.Global.class)
    public static class CountAnimatorDurationScaleShadowSettingsSecure
            extends ShadowSettings.ShadowGlobal {
        public static int sNumAnimatorDurationGets;

        @Implementation
        protected static String getString(ContentResolver cr, String name) {
            if (Settings.Global.ANIMATOR_DURATION_SCALE.equals(name)) {
                ++sNumAnimatorDurationGets;
            }
            return ShadowSettings.ShadowGlobal.getString(cr, name);
        }
    }
}
