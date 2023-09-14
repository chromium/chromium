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

/** Options to configure how to accelerate the model inference using dedicated delegates. */
@AutoValue
public abstract class ComputeSettings {

  /** TFLite accelerator delegate options. */
  public enum Delegate {
    NONE(0),
    NNAPI(1),
    GPU(2);

    private final int value;

    Delegate(int value) {
      this.value = value;
    }

    public int getValue() {
      return value;
    }
  }

  /** Builder for {@link ComputeSettings}. */
  @AutoValue.Builder
  public abstract static class Builder {

    public abstract Builder setDelegate(Delegate delegate);

    public abstract ComputeSettings build();
  }

  public static Builder builder() {
    return new AutoValue_ComputeSettings.Builder().setDelegate(DEFAULT_DELEGATE);
  }

  public abstract Delegate getDelegate();

  private static final Delegate DEFAULT_DELEGATE = Delegate.NONE;
}
