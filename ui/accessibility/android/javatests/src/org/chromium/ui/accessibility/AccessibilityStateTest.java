// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.view.accessibility.AccessibilityEvent;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

@RunWith(BaseRobolectricTestRunner.class)
public class AccessibilityStateTest {

    private static final String EVENT_TYPE_MASK_ERROR =
            "Conversion of event masks to event types not correct.";

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
        AccessibilityState.setEventTypeMaskForTesting(serviceEventMask_empty);
        Set<Integer> outcome_empty = AccessibilityState.relevantEventTypesForCurrentServices();

        AccessibilityState.setEventTypeMaskForTesting(serviceEventMask_full);
        Set<Integer> outcome_full = AccessibilityState.relevantEventTypesForCurrentServices();

        AccessibilityState.setEventTypeMaskForTesting(serviceEventMask_test);
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
}
