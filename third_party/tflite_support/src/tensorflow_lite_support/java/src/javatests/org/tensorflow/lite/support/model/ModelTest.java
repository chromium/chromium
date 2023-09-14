/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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
import static org.junit.Assert.fail;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import java.io.IOException;
import java.nio.MappedByteBuffer;
import java.util.HashMap;
import java.util.Map;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.support.model.Model.Device;
import org.tensorflow.lite.support.model.Model.Options;

import org.junit.Ignore;

/** Tests of {@link org.tensorflow.lite.support.model.Model}. */
@RunWith(RobolectricTestRunner.class)
public final class ModelTest {

  private final Context context = ApplicationProvider.getApplicationContext();
  private static final String MODEL_PATH = "add.tflite";

  @Ignore
  @Test
  public void testLoadLocalModel() throws IOException {
    MappedByteBuffer byteModel = new Model.Builder(context, MODEL_PATH).build().getData();
    assertThat(byteModel).isNotNull();
  }

  @Ignore
  @Test
  public void testBuildMultiThreadModel() throws IOException {
    MappedByteBuffer byteModel =
        new Model.Builder(context, MODEL_PATH).setNumThreads(4).build().getData();
    assertThat(byteModel).isNotNull();
  }

  @Ignore
  @Test
  public void buildModelWithOptionsShouldSuccess() throws IOException {
    Options options = new Options.Builder().setNumThreads(2).setDevice(Device.NNAPI).build();
    Model model = Model.createModel(context, MODEL_PATH, options);
    assertThat(model.getData()).isNotNull();
  }

  @Ignore
  @Test
  public void testGetModelPath() throws IOException {
    String modelPath = new Model.Builder(context, MODEL_PATH).build().getPath();
    assertThat(modelPath).isEqualTo(MODEL_PATH);
  }

  @Test
  public void testNonExistingLocalModel() {
    try {
      new Model.Builder(context, "non_exist_model_file").build();
      fail();
    } catch (IOException e) {
      assertThat(e).hasMessageThat().contains("non_exist_model_file");
    }
  }

  @Test
  public void testNullLocalModelPath() throws IOException {
    try {
      new Model.Builder(context, null).build();
      fail();
    } catch (NullPointerException e) {
      assertThat(e).hasMessageThat().contains("File path cannot be null.");
    }
  }

  @Test
  public void testNullContext() throws IOException {
    try {
      new Model.Builder(null, MODEL_PATH).build();
      fail();
    } catch (NullPointerException e) {
      assertThat(e).hasMessageThat().contains("Context should not be null.");
    }
  }

  @Ignore
  @Test
  public void testGetInputTensor() throws IOException {
    Options options = new Options.Builder().build();
    Model model = Model.createModel(context, MODEL_PATH, options);
    assertThat(model.getInputTensor(0)).isNotNull();
  }

  @Ignore
  @Test
  public void testGetOutputTensor() throws IOException {
    Options options = new Options.Builder().build();
    Model model = Model.createModel(context, MODEL_PATH, options);
    assertThat(model.getOutputTensor(0)).isNotNull();
  }

  @Ignore
  @Test
  public void testRun() throws IOException {
    Context context = ApplicationProvider.getApplicationContext();
    Model model = new Model.Builder(context, MODEL_PATH).build();
    runModel(model);
  }

  @Ignore
  @Test
  public void testMultiThreadingRun() throws IOException {
    Context context = ApplicationProvider.getApplicationContext();
    Model model = new Model.Builder(context, MODEL_PATH).setNumThreads(4).build();
    runModel(model);
  }

  @Ignore
  @Test
  public void testNnApiRun() throws IOException {
    Context context = ApplicationProvider.getApplicationContext();
    Model model = new Model.Builder(context, MODEL_PATH).setDevice(Device.NNAPI).build();
    runModel(model);
  }

  private static void runModel(Model model) throws IOException {
    // Creates the inputs.
    float[] x = {1.5f};
    float[] y = {0.5f};
    float[] expectedSum = {2.0f};
    Object[] inputs = {x, y};

    // Creates the outputs buffer.
    float[] sum = new float[1];
    Map<Integer, Object> outputs = new HashMap<>();
    outputs.put(0, sum);

    // Runs inference.
    model.run(inputs, outputs);
    assertThat(sum).isEqualTo(expectedSum);
  }
}
