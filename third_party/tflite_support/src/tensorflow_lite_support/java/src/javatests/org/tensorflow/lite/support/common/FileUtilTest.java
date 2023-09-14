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

package org.tensorflow.lite.support.common;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.MappedByteBuffer;
import java.nio.charset.Charset;
import java.util.List;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of {@link org.tensorflow.lite.support.common.FileUtil}. */
@RunWith(RobolectricTestRunner.class)
public final class FileUtilTest {
  private final Context context = ApplicationProvider.getApplicationContext();
  private static final String LABEL_PATH = "flower_labels.txt";

  @Test
  public void testLoadLabels() throws IOException {
    List<String> labels = FileUtil.loadLabels(context, LABEL_PATH);
    assertThat(labels)
        .containsExactly("daisy", "dandelion", "roses", "sunflowers", "tulips")
        .inOrder();
  }

  @Test
  public void testLoadLabelsFromInputStream() throws IOException {
    InputStream inputStream = context.getAssets().open(LABEL_PATH);
    assertThat(FileUtil.loadLabels(inputStream))
        .containsExactly("daisy", "dandelion", "roses", "sunflowers", "tulips")
        .inOrder();
  }

  @Test
  public void whitespaceLabelsShouldNotCount() throws IOException {
    String s = "a\nb\n \n\n\nc";
    InputStream stream = new ByteArrayInputStream(s.getBytes(Charset.defaultCharset()));
    assertThat(FileUtil.loadLabels(stream)).hasSize(3);
  }

  @Test
  public void testLoadLabelsNullContext() throws IOException {
    Context nullContext = null;
    Assert.assertThrows(
        NullPointerException.class, () -> FileUtil.loadLabels(nullContext, LABEL_PATH));
  }

  @Test
  public void testLoadLabelsNullFilePath() throws IOException {
    String nullFilePath = null;
    Assert.assertThrows(
        NullPointerException.class, () -> FileUtil.loadLabels(context, nullFilePath));
  }

  @Test
  public void testLoadMappedFile() throws IOException {
    MappedByteBuffer byteModel = FileUtil.loadMappedFile(context, LABEL_PATH);
    assertThat(byteModel).isNotNull();
  }

  @Test
  public void testLoadMappedFileWithNullContext() throws IOException {
    Context nullContext = null;
    Assert.assertThrows(
        NullPointerException.class, () -> FileUtil.loadMappedFile(nullContext, LABEL_PATH));
  }

  @Test
  public void loadMappedFileWithNullFilePath() throws IOException {
    String nullFilePath = null;
    Assert.assertThrows(
        NullPointerException.class, () -> FileUtil.loadMappedFile(context, nullFilePath));
  }
}
