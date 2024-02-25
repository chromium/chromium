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

import java.nio.ByteBuffer;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of {@link BoundedInputStream}. */
@RunWith(RobolectricTestRunner.class)
public class BoundedInputStreamTest {

  private static final byte[] testBytes = new byte[] {10, 20, 30, 40, 50};
  private static final int[] testInts = new int[] {10, 20, 30, 40, 50};
  private static final int TEST_BYTES_LENGTH = testBytes.length;

  @Test
  public void boundedInputStream_negtiveStart_throwsException() throws Exception {
    long start = -1;
    long remaining = 2;
    IllegalArgumentException exception =
        assertThrows(
            IllegalArgumentException.class,
            () -> createBoundedInputStream(testBytes, start, remaining));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo(
            String.format("Invalid length of stream at offset=%d, length=%d", start, remaining));
  }

  @Test
  public void boundedInputStream_negtiveRemaining_throwsException() throws Exception {
    long start = 1;
    long remaining = -2;
    IllegalArgumentException exception =
        assertThrows(
            IllegalArgumentException.class,
            () -> createBoundedInputStream(testBytes, start, remaining));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo(
            String.format("Invalid length of stream at offset=%d, length=%d", start, remaining));
  }

  @Test
  public void available_atStart() throws Exception {
    int start = 3;
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, start, TEST_BYTES_LENGTH);

    int available = boundedInputStream.available();
    assertThat(available).isEqualTo(TEST_BYTES_LENGTH - start);
  }

  @Test
  public void available_afterRead() throws Exception {
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);
    // Read a byte out of boundedInputStream. The number of remaining bytes is TEST_BYTES_LENGTH -1.
    boundedInputStream.read();

    int available = boundedInputStream.available();
    assertThat(available).isEqualTo(TEST_BYTES_LENGTH - 1);
  }

  @Test
  public void read_repeatedRead() throws Exception {
    int[] values = new int[TEST_BYTES_LENGTH];
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);

    for (int i = 0; i < TEST_BYTES_LENGTH; i++) {
      values[i] = boundedInputStream.read();
    }

    assertArrayEquals(testInts, values);
  }

  @Test
  public void read_reachTheEnd() throws Exception {
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);
    boundedInputStream.skip(TEST_BYTES_LENGTH);
    int value = boundedInputStream.read();

    assertThat(value).isEqualTo(-1);
  }

  @Test
  public void read_channelSizeisSmallerThanTheStreamSpecified() throws Exception {
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH + 1);
    boundedInputStream.skip(TEST_BYTES_LENGTH);

    int value = boundedInputStream.read();

    assertThat(value).isEqualTo(-1);
  }

  @Test
  public void readArray_nullArray_throwsException() throws Exception {
    byte[] array = null;
    int offset = 0;
    int length = 1;
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);

    NullPointerException exception =
        assertThrows(
            NullPointerException.class, () -> boundedInputStream.read(array, offset, length));
    assertThat(exception).hasMessageThat().isEqualTo("The object reference is null.");
  }

  @Test
  public void readArray_negativeOffset_throwsException() throws Exception {
    byte[] array = new byte[5];
    int offset = -1;
    int length = array.length;
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);

    IndexOutOfBoundsException exception =
        assertThrows(
            IndexOutOfBoundsException.class, () -> boundedInputStream.read(array, offset, length));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo(String.format("The start offset (%s) must not be negative", offset));
  }

  @Test
  public void readArray_OffsetEqualsArrayLength_throwsException() throws Exception {
    byte[] array = new byte[5];
    int offset = array.length;
    int length = 0;
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);

    IndexOutOfBoundsException exception =
        assertThrows(
            IndexOutOfBoundsException.class, () -> boundedInputStream.read(array, offset, length));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo(
            String.format(
                "The start offset (%s) must be less than size (%s)", offset, array.length));
  }

  @Test
  public void readArray_negativeLength_throwsException() throws Exception {
    byte[] array = new byte[5];
    int offset = 0;
    int length = -1;
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);

    IndexOutOfBoundsException exception =
        assertThrows(
            IndexOutOfBoundsException.class, () -> boundedInputStream.read(array, offset, length));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo(
            String.format(
                "The maximumn number of bytes to read (%s) must not be negative", length));
  }

  @Test
  public void readArray_exceedEndOfArray_throwsException() throws Exception {
    byte[] array = new byte[5];
    int offset = 0;
    int length = array.length + 1;
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);

    IndexOutOfBoundsException exception =
        assertThrows(
            IndexOutOfBoundsException.class, () -> boundedInputStream.read(array, offset, length));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo(
            String.format(
                "The maximumn number of bytes to read (%s) must be less than size (%s)",
                length, array.length - offset + 1));
  }

  @Test
  public void readArray_zeroLength() throws Exception {
    byte[] array = new byte[5];
    int offset = 0;
    int length = 0;
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);

    int value = boundedInputStream.read(array, offset, length);
    assertThat(value).isEqualTo(0);
  }

  @Test
  public void readArray_exceedEndOfStream() throws Exception {
    byte[] array = new byte[5];
    int offset = 0;
    int length = 1;
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);

    // Move the position of the stream to the end.
    boundedInputStream.skip(TEST_BYTES_LENGTH);

    int value = boundedInputStream.read(array, offset, length);

    assertThat(value).isEqualTo(-1);
  }

  @Test
  public void readArray_lengthGreaterThanStreamRemaining() throws Exception {
    byte[] array = new byte[5];
    int offset = 1;
    int length = array.length - 1; // 4
    BoundedInputStream boundedInputStream =
        createBoundedInputStream(testBytes, 0, TEST_BYTES_LENGTH);

    // Moves the position of the stream to end-2.
    boundedInputStream.skip(TEST_BYTES_LENGTH - 2);

    // Reads the last two bytes of the stream to the array, and put the data at offset 1.
    int value = boundedInputStream.read(array, offset, length);

    byte[] expectedArray = new byte[] {0, 40, 50, 0, 0};
    assertArrayEquals(expectedArray, array);
    assertThat(value).isEqualTo(2);

    // Reachs the end of the stream, thus cannot read anymore.
    assertThat(boundedInputStream.read()).isEqualTo(-1);
  }

  private static BoundedInputStream createBoundedInputStream(
      final byte[] testBytes, long start, long remaining) {
    ByteBuffer buffer = ByteBuffer.wrap(testBytes);
    SeekableByteChannelCompat channel = new ByteBufferChannel(buffer);
    return new BoundedInputStream(channel, start, remaining);
  }
}
