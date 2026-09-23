// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero.test_sample;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.jni_zero.JniRawPtr;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.sample.Sample;
import org.jni_zero.sample.Sample.NativeObjectToken;
import org.jni_zero.sample.SampleActivity;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public final class JniZeroSampleTest {
    @Before
    public void setUp() {
        ActivityScenario.launch(SampleActivity.class);
    }

    @Test
    public void testDoParameterCalls() {
        Sample.doParameterCalls();
    }

    @Test
    public void testDoTwoWayCalls() {
        Sample.doTwoWayCalls();
    }

    @Test
    public void testCalledByNativeSafePointers() {
        Sample.sLastReceivedValue = 0;
        Sample.sLastReceivedPtr = null;
        Sample.sLastReceivedRawPtr = null;
        Sample.sLastReceivedUniquePtr = null;
        Sample.triggerCallbackWithSafePtr(42);

        Assert.assertEquals(42, Sample.sLastReceivedValue);
        Assert.assertNotNull(Sample.sLastReceivedPtr);
        Assert.assertThrows(
                IllegalStateException.class, () -> Sample.getValue(Sample.sLastReceivedPtr));

        Assert.assertNotNull(Sample.sLastReceivedRawPtr);
        Assert.assertEquals(42, Sample.getValue(Sample.sLastReceivedRawPtr));
        Sample.sLastReceivedRawPtr.release();
        Sample.sLastReceivedRawPtr.release();
        Assert.assertThrows(
                IllegalStateException.class, () -> Sample.getValue(Sample.sLastReceivedRawPtr));

        Assert.assertNotNull(Sample.sLastReceivedUniquePtr);
        Assert.assertEquals(42, Sample.readHeldPtrFromCpp());
        Sample.sLastReceivedUniquePtr.destroy();
        Sample.sLastReceivedUniquePtr = null;
        Assert.assertEquals(-1, Sample.readHeldPtrFromCpp());
    }

    @Test
    public void testNativeMethodsSafePointers() {
        Assert.assertNull(Sample.createNullNativeObject());
        Assert.assertTrue(Sample.isNullPtr(null));

        JniRawPtr<NativeObjectToken> borrowed = Sample.borrowNativeObject(99);
        Assert.assertEquals(99, Sample.getValue(borrowed));
        borrowed.release();
        Assert.assertThrows(IllegalStateException.class, () -> Sample.getValue(borrowed));

        int initialDeleted = Sample.getDeletedCount();
        JniUniquePtr<NativeObjectToken> obj = Sample.createNativeObject(42);
        Assert.assertFalse(Sample.isNullPtr(obj));
        Assert.assertEquals(42, Sample.getValue(obj));
        Assert.assertEquals(initialDeleted, Sample.getDeletedCount());
        obj.destroy();
        Assert.assertEquals(initialDeleted + 1, Sample.getDeletedCount());
        obj.destroy();
        Assert.assertEquals(initialDeleted + 1, Sample.getDeletedCount());
        Assert.assertThrows(IllegalStateException.class, () -> Sample.getValue(obj));
    }
}
