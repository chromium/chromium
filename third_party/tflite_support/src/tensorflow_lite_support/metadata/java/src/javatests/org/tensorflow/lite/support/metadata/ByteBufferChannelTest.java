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
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertThrows;

import java.nio.ByteBuffer;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of {@link ByteBufferChannel}. */
@RunWith(RobolectricTestRunner.class)
public final class ByteBufferChannelTest {
  private static final String VALID_STRING = "1234567890";
  private final ByteBuffer validByteBuffer = ByteBuffer.wrap(VALID_STRING.getBytes(UTF_8));
  private final int validByteBufferLength = validByteBuffer.limit();

  @Test
  public void byteBufferChannel_validByteBuffer() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    assertThat(byteBufferChannel).isNotNull();
  }

  @Test
  public void byteBufferChannel_nullByteBuffer_throwsException() {
    NullPointerException exception =
        assertThrows(NullPointerException.class, () -> new ByteBufferChannel(/*buffer=*/ null));
    assertThat(exception).hasMessageThat().isEqualTo("The ByteBuffer cannot be null.");
  }

  @Test
  public void isOpen_openedByteBufferChannel() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    assertThat(byteBufferChannel.isOpen()).isTrue();
  }

  @Test
  public void position_newByteBufferChannelWithInitialPosition0() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long position = byteBufferChannel.position();

    long expectedPosition = 0;
    assertThat(position).isEqualTo(expectedPosition);
  }

  @Test
  public void position_validNewPosition() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long validNewPosition = 6;

    byteBufferChannel.position(validNewPosition);
    long position = byteBufferChannel.position();

    assertThat(position).isEqualTo(validNewPosition);
  }

  @Test
  public void position_negtiveNewPosition_throwsException() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long invalidNewPosition = -1;

    IllegalArgumentException exception =
        assertThrows(
            IllegalArgumentException.class, () -> byteBufferChannel.position(invalidNewPosition));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo("The new position should be non-negative and be less than Integer.MAX_VALUE.");
  }

  @Test
  public void position_newPositionGreaterThanMaxIntegerValue_throwsException() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long invalidNewPosition = Integer.MAX_VALUE + 1;

    IllegalArgumentException exception =
        assertThrows(
            IllegalArgumentException.class, () -> byteBufferChannel.position(invalidNewPosition));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo("The new position should be non-negative and be less than Integer.MAX_VALUE.");
  }

  @Test
  public void position_newPositionGreaterThanByfferLength_throwsException() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long invalidNewPosition = (long) validByteBufferLength + 1;

    IllegalArgumentException exception =
        assertThrows(
            IllegalArgumentException.class, () -> byteBufferChannel.position(invalidNewPosition));
    assertThat(exception).hasMessageThat().isEqualTo("newPosition > limit: (11 > 10)");
  }

  @Test
  public void read_fromPosition0() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long validNewPosition = 0;

    byteBufferChannel.position(validNewPosition);
    ByteBuffer dstBuffer = ByteBuffer.allocate(validByteBufferLength);
    int numBytes = byteBufferChannel.read(dstBuffer);

    assertThat(numBytes).isEqualTo(validByteBufferLength);
    assertThat(dstBuffer).isEqualTo(validByteBuffer);
  }

  @Test
  public void read_fromPosition5() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long validNewPosition = 5;

    byteBufferChannel.position(validNewPosition);
    ByteBuffer dstBuffer = ByteBuffer.allocate(validByteBufferLength);
    int numBytes = byteBufferChannel.read(dstBuffer);

    assertThat(numBytes).isEqualTo(validByteBufferLength - (int) validNewPosition);
    String dstString = convertByteByfferToString(dstBuffer, numBytes);
    String expectedString = "67890";
    assertThat(dstString).isEqualTo(expectedString);
  }

  @Test
  public void read_fromPositionValidByteBufferLength() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long validNewPosition = validByteBufferLength;

    byteBufferChannel.position(validNewPosition);
    ByteBuffer dstBuffer = ByteBuffer.allocate(validByteBufferLength);
    int numBytes = byteBufferChannel.read(dstBuffer);

    assertThat(numBytes).isEqualTo(-1);
  }

  @Test
  public void read_dstBufferRemaining0() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long validNewPosition = 0;

    byteBufferChannel.position(validNewPosition);
    ByteBuffer dstBuffer = ByteBuffer.allocate(validByteBufferLength);
    dstBuffer.position(validByteBufferLength);
    int numBytes = byteBufferChannel.read(dstBuffer);

    assertThat(numBytes).isEqualTo(0);
    String dstString = convertByteByfferToString(dstBuffer, numBytes);
    String expectedString = "";
    assertThat(dstString).isEqualTo(expectedString);
  }

  @Test
  public void read_dstBufferIsSmallerThanTheBufferChannel() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    int dstBufferLength = 3;

    ByteBuffer dstBuffer = ByteBuffer.allocate(dstBufferLength);
    int numBytes = byteBufferChannel.read(dstBuffer);

    assertThat(numBytes).isEqualTo(dstBufferLength);
    assertThat(validByteBuffer.position()).isEqualTo(dstBufferLength);

    String dstString = convertByteByfferToString(dstBuffer, dstBufferLength);
    String expectedString = "123";
    assertThat(dstString).isEqualTo(expectedString);
  }

  @Test
  public void size_validBuffer() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    assertThat(byteBufferChannel.size()).isEqualTo(validByteBufferLength);
  }

  @Test
  public void truncate_validSizeAndPosition0() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long truncateSize = 3;

    byteBufferChannel.truncate(truncateSize);

    assertThat(byteBufferChannel.size()).isEqualTo(truncateSize);
    assertThat(byteBufferChannel.position()).isEqualTo(0);
  }

  @Test
  public void truncate_validSizeAndPosition5() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long validNewPosition = 5;

    byteBufferChannel.position(validNewPosition);
    long truncateSize = 3;
    byteBufferChannel.truncate(truncateSize);

    assertThat(byteBufferChannel.position()).isEqualTo(truncateSize);
  }

  @Test
  public void truncate_sizeNotSmallerThanBufferSize() {
    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    long truncateSize = (long) validByteBufferLength;

    byteBufferChannel.truncate(truncateSize);

    assertThat(byteBufferChannel.position()).isEqualTo(0);
  }

  @Test
  public void write_srcBufferSmallerThanBufferChannel() {
    String srcString = "5555";
    long newPosition = 3;
    String expectedString = "1235555890";
    ByteBuffer srcBuffer = ByteBuffer.wrap(srcString.getBytes(UTF_8));

    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    byteBufferChannel.position(newPosition);
    byteBufferChannel.write(srcBuffer);

    assertThat(byteBufferChannel.position()).isEqualTo(newPosition + srcString.length());
    ByteBuffer dstBuffer = ByteBuffer.allocate(validByteBufferLength);
    byteBufferChannel.position(0);
    byteBufferChannel.read(dstBuffer);
    ByteBuffer expectedBuffer = ByteBuffer.wrap(expectedString.getBytes(UTF_8));
    dstBuffer.rewind();
    expectedBuffer.rewind();
    assertThat(dstBuffer).isEqualTo(expectedBuffer);
  }

  @Test
  public void write_srcBufferGreaterThanBufferChannel() {
    String srcString = "5555";
    long newPosition = 8;
    String expectedString = "1234567855";
    ByteBuffer srcBuffer = ByteBuffer.wrap(srcString.getBytes(UTF_8));

    ByteBufferChannel byteBufferChannel = new ByteBufferChannel(validByteBuffer);
    byteBufferChannel.position(newPosition);
    byteBufferChannel.write(srcBuffer);
    assertThat(byteBufferChannel.position()).isEqualTo(validByteBufferLength);

    ByteBuffer dstBuffer = ByteBuffer.allocate(validByteBufferLength);
    byteBufferChannel.position(0);
    byteBufferChannel.read(dstBuffer);
    ByteBuffer expectedBuffer = ByteBuffer.wrap(expectedString.getBytes(UTF_8));
    dstBuffer.rewind();
    expectedBuffer.rewind();
    assertThat(dstBuffer).isEqualTo(expectedBuffer);
  }

  private static String convertByteByfferToString(ByteBuffer buffer, int arrLength) {
    byte[] bytes = new byte[arrLength];
    buffer.rewind();
    buffer.get(bytes);
    return new String(bytes, UTF_8);
  }
}
