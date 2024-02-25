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
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertThrows;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import androidx.test.core.app.ApplicationProvider;
import com.google.flatbuffers.FlatBufferBuilder;
import java.io.FileInputStream;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Random;
import java.util.Set;
import org.apache.commons.io.IOUtils;
import org.checkerframework.checker.nullness.qual.Nullable;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Suite;
import org.junit.runners.Suite.SuiteClasses;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.schema.Buffer;
import org.tensorflow.lite.schema.Metadata;
import org.tensorflow.lite.schema.Model;
import org.tensorflow.lite.schema.QuantizationParameters;
import org.tensorflow.lite.schema.SubGraph;
import org.tensorflow.lite.schema.Tensor;
import org.tensorflow.lite.schema.TensorType;
import org.tensorflow.lite.support.metadata.MetadataExtractor.QuantizationParams;
import org.tensorflow.lite.support.metadata.schema.Content;
import org.tensorflow.lite.support.metadata.schema.ContentProperties;
import org.tensorflow.lite.support.metadata.schema.ModelMetadata;
import org.tensorflow.lite.support.metadata.schema.SubGraphMetadata;
import org.tensorflow.lite.support.metadata.schema.TensorMetadata;

import org.junit.Ignore;

/** Tests of {@link MetadataExtractor}. */
@RunWith(Suite.class)
@SuiteClasses({MetadataExtractorTest.General.class, MetadataExtractorTest.InputTensorType.class})
public class MetadataExtractorTest {
  private static final int[] validShape = new int[] {4, 10, 10, 3};
  private static final byte DATA_TYPE = TensorType.UINT8;
  private static final byte CONTENT_PROPERTIES_TYPE = ContentProperties.ImageProperties;
  private static final float VALID_SCALE = 3.3f;
  private static final long VALID_ZERO_POINT = 2;
  private static final float DEFAULT_SCALE = 0.0f;
  private static final long DEFAULT_ZERO_POINT = 0;
  private static final String MODEL_NAME = "model.tflite";
  // Scale and zero point should both be a single value, not an array.
  private static final float[] invalidScale = new float[] {0.0f, 1.2f};
  private static final long[] invalidZeroPoint = new long[] {1, 2};
  private static final String MODEL_PATH = "mobilenet_v1_1.0_224_quant.tflite";
  // labels.txt is packed in mobilenet_v1_1.0_224_quant.tflite as an associated file.
  private static final String VALID_LABEL_FILE_NAME = "labels.txt";
  // invalid.txt is not packed in mobilenet_v1_1.0_224_quant.tflite.
  private static final String INVALID_LABEL_FILE_NAME = "invalid.txt";
  private static final int EMPTY_FLATBUFFER_VECTOR = -1;
  private static final String TFLITE_MODEL_IDENTIFIER = "TFL3";
  private static final String TFLITE_METADATA_IDENTIFIER = "M001";

  /** General tests of MetadataExtractor. */
  @RunWith(RobolectricTestRunner.class)
  public static final class General extends MetadataExtractorTest {

    @Test
    public void hasMetadata_modelWithMetadata() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      assertThat(metadataExtractor.hasMetadata()).isTrue();
    }

    @Test
    public void hasMetadata_modelWithoutMetadata() throws Exception {
      // Creates a model flatbuffer without metadata.
      ByteBuffer modelWithoutMetadata = createModelByteBuffer(/*metadataBuffer=*/ null, DATA_TYPE);

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithoutMetadata);
      assertThat(metadataExtractor.hasMetadata()).isFalse();
    }

    @Ignore
    @Test
    public void getAssociatedFile_validAssociateFile() throws Exception {
      ByteBuffer mobileNetBuffer = loadMobileNetBuffer();
      MetadataExtractor mobileNetMetadataExtractor = new MetadataExtractor(mobileNetBuffer);
      InputStream associateFileStream =
          mobileNetMetadataExtractor.getAssociatedFile(VALID_LABEL_FILE_NAME);

      // Reads the golden file from context.
      Context context = ApplicationProvider.getApplicationContext();
      InputStream goldenAssociateFileStream = context.getAssets().open(VALID_LABEL_FILE_NAME);
      assertThat(IOUtils.contentEquals(goldenAssociateFileStream, associateFileStream)).isTrue();
    }

    @Ignore
    @Test
    public void getAssociatedFile_invalidAssociateFile() throws Exception {
      ByteBuffer mobileNetBuffer = loadMobileNetBuffer();
      MetadataExtractor mobileNetMetadataExtractor = new MetadataExtractor(mobileNetBuffer);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () -> mobileNetMetadataExtractor.getAssociatedFile(INVALID_LABEL_FILE_NAME));
      assertThat(exception)
          .hasMessageThat()
          .isEqualTo(
              String.format(
                  "The file, %s, does not exist in the zip file.", INVALID_LABEL_FILE_NAME));
    }

    @Ignore
    @Test
    public void getAssociatedFile_nullFileName() throws Exception {
      ByteBuffer mobileNetBuffer = loadMobileNetBuffer();
      MetadataExtractor mobileNetMetadataExtractor = new MetadataExtractor(mobileNetBuffer);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () -> mobileNetMetadataExtractor.getAssociatedFile(/*fileName=*/ null));
      assertThat(exception)
          .hasMessageThat()
          .contains("The file, null, does not exist in the zip file.");
    }

    @Test
    public void getAssociatedFile_nonZipModel_throwsException() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalStateException exception =
          assertThrows(
              IllegalStateException.class,
              () -> metadataExtractor.getAssociatedFile(VALID_LABEL_FILE_NAME));
      assertThat(exception)
          .hasMessageThat()
          .contains("This model does not contain associated files, and is not a Zip file.");
    }

    @Test
    public void getAssociatedFileNames_nonZipModel_throwsException() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalStateException exception =
          assertThrows(IllegalStateException.class, metadataExtractor::getAssociatedFileNames);
      assertThat(exception)
          .hasMessageThat()
          .contains("This model does not contain associated files, and is not a Zip file.");
    }

    @Ignore
    @Test
    public void getAssociatedFileNames_validFileNames() throws Exception {
      ByteBuffer mobileNetBuffer = loadMobileNetBuffer();
      MetadataExtractor mobileNetMetadataExtractor = new MetadataExtractor(mobileNetBuffer);
      Set<String> expectedSet = new HashSet<>();
      expectedSet.add(VALID_LABEL_FILE_NAME);
      assertThat(mobileNetMetadataExtractor.getAssociatedFileNames()).isEqualTo(expectedSet);
    }

    @Test
    public void metadataExtractor_loadNullBuffer_throwsException() {
      ByteBuffer nullBuffer = null;
      NullPointerException exception =
          assertThrows(NullPointerException.class, () -> new MetadataExtractor(nullBuffer));
      assertThat(exception).hasMessageThat().contains("Model flatbuffer cannot be null.");
    }

    @Test
    public void metadataExtractor_loadRandomBuffer_throwsException() {
      ByteBuffer randomBuffer = createRandomByteBuffer();
      IllegalArgumentException exception =
          assertThrows(IllegalArgumentException.class, () -> new MetadataExtractor(randomBuffer));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "The identifier of the model is invalid. The buffer may not be a valid TFLite model"
                  + " flatbuffer.");
    }

    @Test
    public void metadataExtractor_loadModelWithInvalidIdentifier_throwsException() {
      // Creates a model with an invalid identifier.
      String invalidIdentifier = "INVI";
      FlatBufferBuilder builder = new FlatBufferBuilder();
      Model.startModel(builder);
      int model = Model.endModel(builder);
      builder.finish(model, invalidIdentifier);
      ByteBuffer modelBuffer = builder.dataBuffer();

      IllegalArgumentException exception =
          assertThrows(IllegalArgumentException.class, () -> new MetadataExtractor(modelBuffer));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "The identifier of the model is invalid. The buffer may not be a valid TFLite model"
                  + " flatbuffer.");
    }

    @Test
    public void metadataExtractor_loadMetadataWithInvalidIdentifier_throwsException() {
      // Creates a model with metadata which contains an invalid identifier.
      String invalidIdentifier = "INVI";
      ByteBuffer metadata = createMetadataByteBuffer(invalidIdentifier, null);
      ByteBuffer modelBuffer = createModelByteBuffer(metadata, DATA_TYPE);

      IllegalArgumentException exception =
          assertThrows(IllegalArgumentException.class, () -> new MetadataExtractor(modelBuffer));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "The identifier of the metadata is invalid. The buffer may not be a valid TFLite"
                  + " metadata flatbuffer.");
    }

    @Test
    public void getInputTensorCount_validModelFile() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      int count = metadataExtractor.getInputTensorCount();
      assertThat(count).isEqualTo(3);
    }

    @Test
    public void getOutputTensorCount_validModelFile() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      int count = metadataExtractor.getOutputTensorCount();
      assertThat(count).isEqualTo(3);
    }

    @Test
    public void getInputTensorShape_validTensorShape() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      int[] shape = metadataExtractor.getInputTensorShape(0);
      assertArrayEquals(validShape, shape);
    }

    @Test
    public void getInputTensorShape_emptyTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      int[] shape = metadataExtractor.getInputTensorShape(1);
      assertThat(shape).isEmpty();
    }

    @Test
    public void getInputTensorType_emptyTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      byte type = metadataExtractor.getInputTensorType(1);
      assertThat(type).isEqualTo(TensorType.FLOAT32);
    }

    @Test
    public void getOutputTensorShape_validTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      int[] shape = metadataExtractor.getOutputTensorShape(0);
      assertArrayEquals(validShape, shape);
    }

    @Test
    public void getOutputTensorShape_emptyTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      int[] shape = metadataExtractor.getOutputTensorShape(1);
      assertThat(shape).isEmpty();
    }

    @Test
    public void getOutputTensorType_emptyTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      byte type = metadataExtractor.getOutputTensorType(1);
      assertThat(type).isEqualTo(TensorType.FLOAT32);
    }

    @Test
    public void getInputTensorShape_indexGreaterThanTensorNumber_throwsException()
        throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> metadataExtractor.getInputTensorShape(3));
      assertThat(exception).hasMessageThat().contains("The inputIndex specified is invalid.");
    }

    @Test
    public void getInputTensorShape_negtiveIndex_throwsException() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> metadataExtractor.getInputTensorShape(-1));
      assertThat(exception).hasMessageThat().contains("The inputIndex specified is invalid.");
    }

    @Test
    public void getOutputTensorShape_indexGreaterThanTensorNumber_throwsException()
        throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> metadataExtractor.getOutputTensorShape(3));
      assertThat(exception).hasMessageThat().contains("The outputIndex specified is invalid.");
    }

    @Test
    public void getOutputTensorShape_negtiveIndex_throwsException() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> metadataExtractor.getOutputTensorShape(-1));
      assertThat(exception).hasMessageThat().contains("The outputIndex specified is invalid.");
    }

    @Test
    public void getModelMetadata_modelWithMetadata() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      ModelMetadata modelMetadata = metadataExtractor.getModelMetadata();
      assertThat(modelMetadata.name()).isEqualTo(MODEL_NAME);
    }

    @Test
    public void getModelMetadata_modelWithoutMetadata_throwsException() throws Exception {
      // Creates a model flatbuffer without metadata.
      ByteBuffer modelWithoutMetadata = createModelByteBuffer(/*metadataBuffer=*/ null, DATA_TYPE);

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithoutMetadata);

      IllegalStateException exception =
          assertThrows(IllegalStateException.class, () -> metadataExtractor.getModelMetadata());
      assertThat(exception)
          .hasMessageThat()
          .contains("This model does not contain model metadata.");
    }

    @Test
    public void metadataExtractor_modelWithEmptySubgraphMetadata_throwsException() {
      // Creates a metadata FlatBuffer without empty subgraph metadata.
      FlatBufferBuilder builder = new FlatBufferBuilder();
      SubGraphMetadata.startSubGraphMetadata(builder);
      int subgraph1Metadata = SubGraphMetadata.endSubGraphMetadata(builder);
      int subgraphsMetadata =
          ModelMetadata.createSubgraphMetadataVector(builder, new int[] {subgraph1Metadata});

      ModelMetadata.startModelMetadata(builder);
      ModelMetadata.addSubgraphMetadata(builder, subgraphsMetadata);
      int modelMetadata = ModelMetadata.endModelMetadata(builder);
      builder.finish(modelMetadata, TFLITE_METADATA_IDENTIFIER);
      ByteBuffer emptyMetadata = builder.dataBuffer();
      ByteBuffer modelWithEmptyMetadata = createModelByteBuffer(emptyMetadata, DATA_TYPE);

      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> new MetadataExtractor(modelWithEmptyMetadata));
      assertThat(exception)
          .hasMessageThat()
          .isEqualTo(
              "The number of input tensors in the model is 3. The number of input tensors that"
                  + " recorded in the metadata is 0. These two values does not match.");
    }

    @Test
    public void metadataExtractor_modelWithEmptyMetadata_throwsException() {
      // Creates a empty metadata FlatBuffer.
      FlatBufferBuilder builder = new FlatBufferBuilder();
      ModelMetadata.startModelMetadata(builder);
      int modelMetadata = ModelMetadata.endModelMetadata(builder);
      builder.finish(modelMetadata, TFLITE_METADATA_IDENTIFIER);

      ByteBuffer emptyMetadata = builder.dataBuffer();
      ByteBuffer modelWithEmptyMetadata = createModelByteBuffer(emptyMetadata, DATA_TYPE);

      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> new MetadataExtractor(modelWithEmptyMetadata));
      assertThat(exception)
          .hasMessageThat()
          .contains("The metadata flatbuffer does not contain any subgraph metadata.");
    }

    @Test
    public void metadataExtractor_modelWithNoMetadata_throwsException() throws Exception {
      // Creates a model flatbuffer without metadata.
      ByteBuffer modelWithoutMetadata = createModelByteBuffer(/*metadataBuffer=*/ null, DATA_TYPE);

      // It is allowed to create a model without metadata, but invoking methods that reads metadata
      // is not allowed.
      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithoutMetadata);

      IllegalStateException exception =
          assertThrows(
              IllegalStateException.class, () -> metadataExtractor.getInputTensorMetadata(0));
      assertThat(exception)
          .hasMessageThat()
          .contains("This model does not contain model metadata.");
    }

    @Test
    public void metadataExtractor_modelWithIrrelevantMetadata_throwsException() throws Exception {
      // Creates a model with irrelevant metadata.
      FlatBufferBuilder builder = new FlatBufferBuilder();
      SubGraph.startSubGraph(builder);
      int subgraph = SubGraph.endSubGraph(builder);

      int metadataName = builder.createString("Irrelevant metadata");
      Metadata.startMetadata(builder);
      Metadata.addName(builder, metadataName);
      int metadata = Metadata.endMetadata(builder);
      int metadataArray = Model.createMetadataVector(builder, new int[] {metadata});

      // Creates Model.
      int[] subgraphs = new int[1];
      subgraphs[0] = subgraph;
      int modelSubgraphs = Model.createSubgraphsVector(builder, subgraphs);
      Model.startModel(builder);
      Model.addSubgraphs(builder, modelSubgraphs);
      Model.addMetadata(builder, metadataArray);
      int model = Model.endModel(builder);
      builder.finish(model, TFLITE_MODEL_IDENTIFIER);
      ByteBuffer modelBuffer = builder.dataBuffer();

      // It is allowed to create a model without metadata, but invoking methods that reads metadata
      // is not allowed.
      MetadataExtractor metadataExtractor = new MetadataExtractor(modelBuffer);

      IllegalStateException exception =
          assertThrows(
              IllegalStateException.class, () -> metadataExtractor.getInputTensorMetadata(0));
      assertThat(exception)
          .hasMessageThat()
          .contains("This model does not contain model metadata.");
    }

    @Test
    public void getInputTensorMetadata_validTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      TensorMetadata inputMetadata = metadataExtractor.getInputTensorMetadata(0);
      assertThat(inputMetadata.content().contentPropertiesType())
          .isEqualTo(CONTENT_PROPERTIES_TYPE);
    }

    @Test
    public void getInputTensorMetadata_emptyTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      TensorMetadata inputMetadata = metadataExtractor.getInputTensorMetadata(1);
      assertThat(inputMetadata.content()).isNull();
    }

    @Test
    public void getInputTensorMetadata_invalidTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      TensorMetadata inputMetadata = metadataExtractor.getInputTensorMetadata(2);
      assertThat(inputMetadata.content().contentPropertiesType())
          .isEqualTo(CONTENT_PROPERTIES_TYPE);
    }

    @Test
    public void getOutputTensorMetadata_validTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      TensorMetadata outputMetadata = metadataExtractor.getOutputTensorMetadata(0);
      assertThat(outputMetadata.content().contentPropertiesType())
          .isEqualTo(CONTENT_PROPERTIES_TYPE);
    }

    @Test
    public void getOutputTensorMetadata_emptyTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      TensorMetadata outputMetadata = metadataExtractor.getOutputTensorMetadata(1);
      assertThat(outputMetadata.content()).isNull();
    }

    @Test
    public void getOutputTensorMetadata_invalidTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      TensorMetadata outputMetadata = metadataExtractor.getOutputTensorMetadata(2);
      assertThat(outputMetadata.content().contentPropertiesType())
          .isEqualTo(CONTENT_PROPERTIES_TYPE);
    }

    @Test
    public void getInputTensorMetadata_indexGreaterThanTensorNumber_throwsException()
        throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> metadataExtractor.getInputTensorMetadata(3));
      assertThat(exception).hasMessageThat().contains("The inputIndex specified is invalid.");
    }

    @Test
    public void getInputTensorMetadata_negtiveIndex_throwsException() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> metadataExtractor.getInputTensorMetadata(-1));
      assertThat(exception).hasMessageThat().contains("The inputIndex specified is invalid.");
    }

    @Test
    public void getOutputTensorMetadata_indexGreaterThanTensorNumber_throwsException()
        throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> metadataExtractor.getOutputTensorMetadata(3));
      assertThat(exception).hasMessageThat().contains("The outputIndex specified is invalid.");
    }

    @Test
    public void getOutputTensorMetadata_negtiveIndex_throwsException() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> metadataExtractor.getOutputTensorMetadata(-1));
      assertThat(exception).hasMessageThat().contains("The outputIndex specified is invalid.");
    }

    @Test
    public void getInputTensorQuantizationParams_validScaleAndZeroPoint() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      QuantizationParams quantizationParams = metadataExtractor.getInputTensorQuantizationParams(0);
      assertThat(quantizationParams.getScale()).isEqualTo(VALID_SCALE);
      assertThat(quantizationParams.getZeroPoint()).isEqualTo(VALID_ZERO_POINT);
    }

    @Test
    public void getInputTensorQuantizationParams_emptyTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      QuantizationParams quantizationParams = metadataExtractor.getInputTensorQuantizationParams(1);
      // Scale and zero point are expected to be 1.0f and 0, respectively as default.
      assertThat(quantizationParams.getScale()).isEqualTo(DEFAULT_SCALE);
      assertThat(quantizationParams.getZeroPoint()).isEqualTo(DEFAULT_ZERO_POINT);
    }

    @Test
    public void getInputTensorQuantizationParams_invalidScale() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () -> metadataExtractor.getInputTensorQuantizationParams(2));
      assertThat(exception)
          .hasMessageThat()
          .contains("Input and output tensors do not support per-channel quantization.");
    }

    @Test
    public void getOutputTensorQuantizationParams_validScaleAndZeroPoint() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      QuantizationParams quantizationParams =
          metadataExtractor.getOutputTensorQuantizationParams(0);
      assertThat(quantizationParams.getScale()).isEqualTo(VALID_SCALE);
      assertThat(quantizationParams.getZeroPoint()).isEqualTo(VALID_ZERO_POINT);
    }

    @Test
    public void getOutputTensorQuantizationParams_emptyTensor() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      QuantizationParams quantizationParams =
          metadataExtractor.getOutputTensorQuantizationParams(1);
      // Scale and zero point are expected to be 1.0f and 0, respectively as default.
      assertThat(quantizationParams.getScale()).isEqualTo(DEFAULT_SCALE);
      assertThat(quantizationParams.getZeroPoint()).isEqualTo(DEFAULT_ZERO_POINT);
    }

    @Test
    public void getOutputTensorQuantizationParams_invalidScale() throws Exception {
      // Creates a model flatbuffer with metadata.
      ByteBuffer modelWithMetadata = createModelByteBuffer();

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () -> metadataExtractor.getOutputTensorQuantizationParams(2));
      assertThat(exception)
          .hasMessageThat()
          .contains("Input and output tensors do not support per-channel quantization.");
    }

    @Test
    public void isMinimumParserVersionSatisfied_olderVersion() throws Exception {
      // A version older than the current one. The version starts from 1.0.0, thus 0.10.0 will
      // precede any furture versions.
      String minVersion = "0.10";
      // Creates a metadata using the above version.
      ByteBuffer metadata = createMetadataByteBuffer(TFLITE_METADATA_IDENTIFIER, minVersion);
      ByteBuffer modelWithMetadata = createModelByteBuffer(metadata, DATA_TYPE);

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);

      assertThat(metadataExtractor.isMinimumParserVersionSatisfied()).isTrue();
    }

    @Test
    public void isMinimumParserVersionSatisfied_sameVersionSamelength() throws Exception {
      // A version the same as the current one.
      String minVersion = MetadataParser.VERSION;
      // Creates a metadata using the above version.
      ByteBuffer metadata = createMetadataByteBuffer(TFLITE_METADATA_IDENTIFIER, minVersion);
      ByteBuffer modelWithMetadata = createModelByteBuffer(metadata, DATA_TYPE);

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);

      assertThat(metadataExtractor.isMinimumParserVersionSatisfied()).isTrue();
    }

    @Test
    public void isMinimumParserVersionSatisfied_sameVersionLongerlength() throws Exception {
      // A version the same as the current one, but with longer length.
      String minVersion = MetadataParser.VERSION + ".0";
      // Creates a metadata using the above version.
      ByteBuffer metadata = createMetadataByteBuffer(TFLITE_METADATA_IDENTIFIER, minVersion);
      ByteBuffer modelWithMetadata = createModelByteBuffer(metadata, DATA_TYPE);

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);

      assertThat(metadataExtractor.isMinimumParserVersionSatisfied()).isTrue();
    }

    @Test
    public void isMinimumParserVersionSatisfied_emptyVersion() throws Exception {
      // An empty version, which can be generated before the first versioned release.
      String minVersion = null;
      // Creates a metadata using the above version.
      ByteBuffer metadata = createMetadataByteBuffer(TFLITE_METADATA_IDENTIFIER, minVersion);
      ByteBuffer modelWithMetadata = createModelByteBuffer(metadata, DATA_TYPE);

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);

      assertThat(metadataExtractor.isMinimumParserVersionSatisfied()).isTrue();
    }

    @Test
    public void isMinimumParserVersionSatisfied_newerVersion() throws Exception {
      // Creates a version newer than the current one by appending "1" to the end of the current
      // version for testing purposes. For example, 1.0.0 becomes 1.0.01.
      String minVersion = MetadataParser.VERSION + "1";
      // Creates a metadata using the above version.
      ByteBuffer metadata = createMetadataByteBuffer(TFLITE_METADATA_IDENTIFIER, minVersion);
      ByteBuffer modelWithMetadata = createModelByteBuffer(metadata, DATA_TYPE);

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);

      assertThat(metadataExtractor.isMinimumParserVersionSatisfied()).isFalse();
    }

    @Test
    public void isMinimumParserVersionSatisfied_newerVersionLongerLength() throws Exception {
      // Creates a version newer than the current one by appending ".1" to the end of the current
      // version for testing purposes. For example, 1.0.0 becomes 1.0.0.1.
      String minVersion = MetadataParser.VERSION + ".1";
      // Creates a metadata using the above version.
      ByteBuffer metadata = createMetadataByteBuffer(TFLITE_METADATA_IDENTIFIER, minVersion);
      ByteBuffer modelWithMetadata = createModelByteBuffer(metadata, DATA_TYPE);

      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);

      assertThat(metadataExtractor.isMinimumParserVersionSatisfied()).isFalse();
    }
  }

  /** Parameterized tests for the input tensor data type. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class InputTensorType extends MetadataExtractorTest {
    /** The tensor type that used to create the model buffer. */
    @Parameter(0)
    public byte tensorType;

    /** A list of TensorType that is used in the test. */
    @Parameters
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {TensorType.FLOAT32}, {TensorType.INT32},
            {TensorType.UINT8}, {TensorType.INT64},
            {TensorType.STRING}
          });
    }

    @Test
    public void getInputTensorType_validTensor() throws Exception {
      ByteBuffer metadata = createMetadataByteBuffer(TFLITE_METADATA_IDENTIFIER, null);
      ByteBuffer modelWithMetadata = createModelByteBuffer(metadata, tensorType);
      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      byte type = metadataExtractor.getInputTensorType(0);
      assertThat(type).isEqualTo(tensorType);
    }

    @Test
    public void getOutputTensorType_validTensor() throws Exception {
      ByteBuffer metadata = createMetadataByteBuffer(TFLITE_METADATA_IDENTIFIER, null);
      ByteBuffer modelWithMetadata = createModelByteBuffer(metadata, tensorType);
      MetadataExtractor metadataExtractor = new MetadataExtractor(modelWithMetadata);
      byte type = metadataExtractor.getOutputTensorType(0);
      assertThat(type).isEqualTo(tensorType);
    }
  }

  /**
   * Creates an example metadata flatbuffer, which contains one subgraph with three inputs and three
   * outputs.
   */
  private static ByteBuffer createMetadataByteBuffer(
      String identifier, @Nullable String minVersionStr) {
    FlatBufferBuilder builder = new FlatBufferBuilder();

    Content.startContent(builder);
    Content.addContentPropertiesType(builder, CONTENT_PROPERTIES_TYPE);
    int content = Content.endContent(builder);

    TensorMetadata.startTensorMetadata(builder);
    TensorMetadata.addContent(builder, content);
    int metadataForValidTensor = TensorMetadata.endTensorMetadata(builder);

    TensorMetadata.startTensorMetadata(builder);
    int metadataForEmptyTensor = TensorMetadata.endTensorMetadata(builder);

    TensorMetadata.startTensorMetadata(builder);
    TensorMetadata.addContent(builder, content);
    int metadataForInvalidTensor = TensorMetadata.endTensorMetadata(builder);

    int[] tensorMetadataArray =
        new int[] {metadataForValidTensor, metadataForEmptyTensor, metadataForInvalidTensor};
    int inputTensorMetadata =
        SubGraphMetadata.createInputTensorMetadataVector(builder, tensorMetadataArray);
    int outputTensorMetadata =
        SubGraphMetadata.createOutputTensorMetadataVector(builder, tensorMetadataArray);

    SubGraphMetadata.startSubGraphMetadata(builder);
    SubGraphMetadata.addInputTensorMetadata(builder, inputTensorMetadata);
    SubGraphMetadata.addOutputTensorMetadata(builder, outputTensorMetadata);
    int subgraph1Metadata = SubGraphMetadata.endSubGraphMetadata(builder);

    int[] subgraphMetadataArray = new int[] {subgraph1Metadata};
    int subgraphsMetadata =
        ModelMetadata.createSubgraphMetadataVector(builder, subgraphMetadataArray);

    int modelName = builder.createString(MODEL_NAME);
    if (minVersionStr != null) {
      int minVersion = builder.createString(minVersionStr);
      ModelMetadata.startModelMetadata(builder);
      ModelMetadata.addMinParserVersion(builder, minVersion);
    } else {
      // If minVersionStr is null, skip generating the field in the metadata.
      ModelMetadata.startModelMetadata(builder);
    }
    ModelMetadata.addName(builder, modelName);
    ModelMetadata.addSubgraphMetadata(builder, subgraphsMetadata);
    int modelMetadata = ModelMetadata.endModelMetadata(builder);

    builder.finish(modelMetadata, identifier);
    return builder.dataBuffer();
  }

  private static int createQuantizationParameters(
      FlatBufferBuilder builder, float[] scale, long[] zeroPoint) {
    int inputScale = QuantizationParameters.createScaleVector(builder, scale);
    int inputZeroPoint = QuantizationParameters.createZeroPointVector(builder, zeroPoint);
    QuantizationParameters.startQuantizationParameters(builder);
    QuantizationParameters.addScale(builder, inputScale);
    QuantizationParameters.addZeroPoint(builder, inputZeroPoint);
    return QuantizationParameters.endQuantizationParameters(builder);
  }

  private static int createTensor(
      FlatBufferBuilder builder, int[] inputShape, byte inputType, int inputQuantization) {
    int inputShapeVector1 = Tensor.createShapeVector(builder, inputShape);
    Tensor.startTensor(builder);
    Tensor.addShape(builder, inputShapeVector1);
    Tensor.addType(builder, inputType);
    Tensor.addQuantization(builder, inputQuantization);
    return Tensor.endTensor(builder);
  }

  /**
   * Creates an example model flatbuffer, which contains one subgraph with three inputs and three
   * output.
   */
  private static ByteBuffer createModelByteBuffer(ByteBuffer metadataBuffer, byte dataType) {
    FlatBufferBuilder builder = new FlatBufferBuilder();

    // Creates a valid set of quantization parameters.
    int validQuantization =
        createQuantizationParameters(
            builder, new float[] {VALID_SCALE}, new long[] {VALID_ZERO_POINT});

    // Creates an invalid set of quantization parameters.
    int inValidQuantization = createQuantizationParameters(builder, invalidScale, invalidZeroPoint);

    // Creates an input Tensor with valid quantization parameters.
    int validTensor = createTensor(builder, validShape, dataType, validQuantization);

    // Creates an empty input Tensor.
    Tensor.startTensor(builder);
    int emptyTensor = Tensor.endTensor(builder);

    // Creates an input Tensor with invalid quantization parameters.
    int invalidTensor = createTensor(builder, validShape, dataType, inValidQuantization);

    // Creates the SubGraph.
    int[] tensors = new int[6];
    tensors[0] = validTensor;
    tensors[1] = emptyTensor;
    tensors[2] = invalidTensor;
    tensors[3] = validTensor;
    tensors[4] = emptyTensor;
    tensors[5] = invalidTensor;
    int subgraphTensors = SubGraph.createTensorsVector(builder, tensors);

    int subgraphInputs = SubGraph.createInputsVector(builder, new int[] {0, 1, 2});
    int subgraphOutputs = SubGraph.createOutputsVector(builder, new int[] {3, 4, 5});

    SubGraph.startSubGraph(builder);
    SubGraph.addTensors(builder, subgraphTensors);
    SubGraph.addInputs(builder, subgraphInputs);
    SubGraph.addOutputs(builder, subgraphOutputs);
    int subgraph = SubGraph.endSubGraph(builder);

    // Creates the Model.
    int[] subgraphs = new int[1];
    subgraphs[0] = subgraph;
    int modelSubgraphs = Model.createSubgraphsVector(builder, subgraphs);

    // Inserts metadataBuffer into the model if it's not null.
    int modelBuffers = EMPTY_FLATBUFFER_VECTOR;
    int metadataArray = EMPTY_FLATBUFFER_VECTOR;
    if (metadataBuffer != null) {
      int data = Buffer.createDataVector(builder, metadataBuffer);
      Buffer.startBuffer(builder);
      Buffer.addData(builder, data);
      int buffer = Buffer.endBuffer(builder);
      modelBuffers = Model.createBuffersVector(builder, new int[] {buffer});

      int metadataName = builder.createString(ModelInfo.METADATA_FIELD_NAME);
      Metadata.startMetadata(builder);
      Metadata.addName(builder, metadataName);
      Metadata.addBuffer(builder, 0);
      int metadata = Metadata.endMetadata(builder);
      metadataArray = Model.createMetadataVector(builder, new int[] {metadata});
    }

    Model.startModel(builder);
    Model.addSubgraphs(builder, modelSubgraphs);
    if (modelBuffers != EMPTY_FLATBUFFER_VECTOR && metadataArray != EMPTY_FLATBUFFER_VECTOR) {
      Model.addBuffers(builder, modelBuffers);
      Model.addMetadata(builder, metadataArray);
    }
    int model = Model.endModel(builder);
    builder.finish(model, TFLITE_MODEL_IDENTIFIER);

    return builder.dataBuffer();
  }

  /** Creates an example model flatbuffer with the default metadata and data type. */
  private static ByteBuffer createModelByteBuffer() {
    ByteBuffer metadata =
        createMetadataByteBuffer(TFLITE_METADATA_IDENTIFIER, /*minVersionStr=*/ null);
    return createModelByteBuffer(metadata, DATA_TYPE);
  }

  private static ByteBuffer loadMobileNetBuffer() throws Exception {
    Context context = ApplicationProvider.getApplicationContext();
    // Loads a MobileNet model flatbuffer with metadata. The MobileNet model is a zip file that
    // contains a label file as the associated file.
    AssetFileDescriptor fileDescriptor = context.getAssets().openFd(MODEL_PATH);
    FileInputStream inputStream = new FileInputStream(fileDescriptor.getFileDescriptor());
    FileChannel fileChannel = inputStream.getChannel();
    long startOffset = fileDescriptor.getStartOffset();
    long declaredLength = fileDescriptor.getDeclaredLength();
    return fileChannel.map(FileChannel.MapMode.READ_ONLY, startOffset, declaredLength);
  }

  private static ByteBuffer createRandomByteBuffer() {
    byte[] buffer = new byte[20];
    new Random().nextBytes(buffer);
    return ByteBuffer.wrap(buffer);
  }
}
