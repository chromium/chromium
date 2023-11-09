// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import android.accessibilityservice.AccessibilityServiceInfo;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
public class AccessibilityStateTest {

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
}
