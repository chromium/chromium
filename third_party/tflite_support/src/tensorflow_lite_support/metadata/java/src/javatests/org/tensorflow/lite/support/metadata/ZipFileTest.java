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

package org.tensorflow.lite.support.metadata;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import androidx.test.core.app.ApplicationProvider;
import java.io.FileInputStream;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.HashSet;
import java.util.Set;
import java.util.zip.ZipException;
import org.apache.commons.io.IOUtils;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import org.junit.Ignore;

/** Tests of {@link ZipFile}. */
@RunWith(RobolectricTestRunner.class)
public final class ZipFileTest {

  // The TFLite model file is a zip file.
  private static final String MODEL_PATH = "mobilenet_v1_1.0_224_quant.tflite";
  // labels.txt is packed in mobilenet_v1_1.0_224_quant.tflite as an associated file.
  private static final String VALID_LABEL_FILE_NAME = "labels.txt";
  // invalid.txt is not packed in mobilenet_v1_1.0_224_quant.tflite.
  private static final String INVALID_LABEL_FILE_NAME = "invalid.txt";
  private final Context context = ApplicationProvider.getApplicationContext();

  @Test
  public void zipFile_nullChannel_throwsException() throws Exception {
    NullPointerException exception =
        assertThrows(NullPointerException.class, () -> ZipFile.createFrom(null));
    assertThat(exception).hasMessageThat().isEqualTo("The object reference is null.");
  }

  @Test
  public void zipFile_invalidFileWithExtremeSmallSize_throwsException() throws Exception {
    // The size limit for a zip file is the End head size, ZipConstant.ENDHDR, which is 22.
    ByteBuffer modelBuffer = ByteBuffer.allocate(21);
    ByteBufferChannel modelChannel = new ByteBufferChannel(modelBuffer);

    ZipException exception =
        assertThrows(ZipException.class, () -> ZipFile.createFrom(modelChannel));
    assertThat(exception).hasMessageThat().isEqualTo("The archive is not a ZIP archive.");
  }

  @Test
  public void zipFile_invalidFileWithNoSignature_throwsException() throws Exception {
    // An invalid zip file that meets the size requirement but does not contain the zip signature.
    ByteBuffer modelBuffer = ByteBuffer.allocate(22);
    ByteBufferChannel modelChannel = new ByteBufferChannel(modelBuffer);

    ZipException exception =
        assertThrows(ZipException.class, () -> ZipFile.createFrom(modelChannel));
    assertThat(exception).hasMessageThat().isEqualTo("The archive is not a ZIP archive.");
  }

  @Ignore
  @Test
  public void getFileNames_correctFileName() throws Exception {
    ByteBufferChannel modelChannel = loadModel(MODEL_PATH);
    ZipFile zipFile = ZipFile.createFrom(modelChannel);
    Set<String> expectedSet = new HashSet<>();
    expectedSet.add(VALID_LABEL_FILE_NAME);
    assertThat(zipFile.getFileNames()).isEqualTo(expectedSet);
  }

  @Ignore
  @Test
  public void getRawInputStream_existentFile() throws Exception {
    ByteBufferChannel modelChannel = loadModel(MODEL_PATH);
    ZipFile zipFile = ZipFile.createFrom(modelChannel);
    InputStream fileStream = zipFile.getRawInputStream(VALID_LABEL_FILE_NAME);

    // Reads the golden file from context.
    InputStream goldenFileStream = context.getAssets().open(VALID_LABEL_FILE_NAME);
    assertThat(IOUtils.contentEquals(goldenFileStream, fileStream)).isTrue();
  }

  @Ignore
  @Test
  public void getRawInputStream_nonExistentFile() throws Exception {
    ByteBufferChannel modelChannel = loadModel(MODEL_PATH);
    ZipFile zipFile = ZipFile.createFrom(modelChannel);

    IllegalArgumentException exception =
        assertThrows(
            IllegalArgumentException.class,
            () -> zipFile.getRawInputStream(INVALID_LABEL_FILE_NAME));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo(
            String.format(
                "The file, %s, does not exist in the zip file.", INVALID_LABEL_FILE_NAME));
  }

  @Ignore
  @Test
  public void close_validStatus() throws Exception {
    ByteBufferChannel modelChannel = loadModel(MODEL_PATH);
    ZipFile zipFile = ZipFile.createFrom(modelChannel);
    // Should do nothing (including not throwing an exception).
    zipFile.close();
  }

  private static ByteBufferChannel loadModel(String modelPath) throws Exception {
    // Creates a ZipFile with a TFLite model flatbuffer with metadata. The MobileNet
    // model is a zip file that contains a label file as the associated file.
    Context context = ApplicationProvider.getApplicationContext();
    AssetFileDescriptor fileDescriptor = context.getAssets().openFd(modelPath);
    FileInputStream inputStream = new FileInputStream(fileDescriptor.getFileDescriptor());
    FileChannel fileChannel = inputStream.getChannel();
    long startOffset = fileDescriptor.getStartOffset();
    long declaredLength = fileDescriptor.getDeclaredLength();
    ByteBuffer modelBuffer =
        fileChannel.map(FileChannel.MapMode.READ_ONLY, startOffset, declaredLength);
    return new ByteBufferChannel(modelBuffer);
  }
}
