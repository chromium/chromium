package org.jni_zero.test_sample;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.jni_zero.samples.SampleActivity;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public final class JniZeroSampleTest {
    @Test
    public void testLaunchSampleActivity() {
        ActivityScenario.launch(SampleActivity.class);
    }
}
