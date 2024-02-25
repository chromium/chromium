package org.jni_zero.test_sample;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.jni_zero.sample.Sample;
import org.jni_zero.sample.SampleActivity;
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
}
