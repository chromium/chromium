/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
package org.tensorflow.lite.support.model;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.InterpreterApi.Options.TfLiteRuntime;

/**
 * Tests of {@link org.tensorflow.lite.support.model.GpuDelegateProxy}. These tests are built with
 * the GpuDelegate class NOT linked in.
 */
@RunWith(RobolectricTestRunner.class)
public final class GpuDelegateProxyTest {

  @Test
  public void createGpuDelegateProxyWithoutDependencyShouldReturnNull() {
    try (GpuDelegateProxy proxy =
        GpuDelegateProxy.maybeNewInstance(TfLiteRuntime.PREFER_SYSTEM_OVER_APPLICATION)) {
      assertThat(proxy).isNull();
    }
  }

  @Test
  public void createApplicationGpuDelegateProxyWithoutDependencyShouldReturnNull() {
    try (GpuDelegateProxy proxy =
        GpuDelegateProxy.maybeNewInstance(TfLiteRuntime.FROM_APPLICATION_ONLY)) {
      assertThat(proxy).isNull();
    }
  }

  @Test
  public void createSystemGpuDelegateProxyWithoutDependencyShouldReturnNull() {
    try (GpuDelegateProxy proxy =
        GpuDelegateProxy.maybeNewInstance(TfLiteRuntime.FROM_SYSTEM_ONLY)) {
      assertThat(proxy).isNull();
    }
  }
}
