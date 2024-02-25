/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.task.core;

import com.google.auto.value.AutoValue;

/** Options to configure Task APIs in general. */
@AutoValue
public abstract class BaseOptions {
  private static final int DEFAULT_NUM_THREADS = -1;

  /** Builder for {@link BaseOptions}. */
  @AutoValue.Builder
  public abstract static class Builder {

    /**
     * Sets the advanced accelerator options.
     *
     * <p>Note: this method will override those highlevel API to choose an delegate, such as {@link
     * #useGpu} and {@link #useNnapi}.
     */
    public abstract Builder setComputeSettings(ComputeSettings computeSettings);

    /**
     * Sets the number of threads to be used for TFLite ops that support multi-threading when
     * running inference with CPU. Defaults to -1.
     *
     * <p>{@code numThreads} should be greater than 0 or equal to -1. Setting numThreads to -1 has
     * the effect to let TFLite runtime set the value.
     */
    public abstract Builder setNumThreads(int numThreads);

    /**
     * Uses GPU for inference. The advanced GPU configuration settings will be set to default
     * values.
     *
     * <p>Note: this method will override the settings from {@link #setComputeSettings}.
     *
     * <p>To manipulate the advanced GPU configuration settings, use {@link #setComputeSettings}.
     */
    public Builder useGpu() {
      return setComputeSettings(
          ComputeSettings.builder().setDelegate(ComputeSettings.Delegate.GPU).build());
    }

    /**
     * Uses NNAPI for inference. The advanced NNAPI configuration settings will be set to default
     * values.
     *
     * <p>Note: this method will override the settings from {@link #setComputeSettings}.
     *
     * <p>To manipulate the advanced NNAPI configuration settings, use {@link #setComputeSettings}.
     */
    public Builder useNnapi() {
      return setComputeSettings(
          ComputeSettings.builder().setDelegate(ComputeSettings.Delegate.NNAPI).build());
    }

    public abstract BaseOptions build();
  }

  public static Builder builder() {
    return new AutoValue_BaseOptions.Builder()
        .setComputeSettings(ComputeSettings.builder().build())
        .setNumThreads(DEFAULT_NUM_THREADS);
  }

  abstract ComputeSettings getComputeSettings();

  abstract int getNumThreads();
}
