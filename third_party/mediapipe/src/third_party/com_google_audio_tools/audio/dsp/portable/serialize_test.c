/* Copyright 2019, 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/portable/serialize.h"

#include <stdlib.h>

#include "audio/dsp/portable/logging.h"

/* Test serialization / deserialization of uint16_t. */
static void TestU16(void) {
  puts("TestU16");
  uint8_t buffer[sizeof(uint16_t)];
  LittleEndianWriteS16(0x0102, buffer);
  ABSL_CHECK(buffer[0] == 0x02);
  ABSL_CHECK(buffer[1] == 0x01);

  BigEndianWriteS16(0x0102, buffer);
  ABSL_CHECK(buffer[0] == 0x01);
  ABSL_CHECK(buffer[1] == 0x02);

  /* Check round trip of serializing and deserializing some test values. */
  static const uint16_t kTestValues[] = {0, 1, 2, 300, 50000, UINT16_MAX};
  size_t i;
  for (i = 0; i < sizeof(kTestValues) / sizeof(*kTestValues); ++i) {
    const uint16_t value = kTestValues[i];
    LittleEndianWriteU16(value, buffer);
    ABSL_CHECK(LittleEndianReadU16(buffer) == value);

    BigEndianWriteU16(value, buffer);
    ABSL_CHECK(BigEndianReadU16(buffer) == value);
  }
}

/* Test serialization / deserialization of uint32_t. */
static void TestU32(void) {
  puts("TestU32");
  uint8_t buffer[sizeof(uint32_t)];
  LittleEndianWriteU32(UINT32_C(0x01020304), buffer);
  ABSL_CHECK(buffer[0] == 0x04);
  ABSL_CHECK(buffer[1] == 0x03);
  ABSL_CHECK(buffer[2] == 0x02);
  ABSL_CHECK(buffer[3] == 0x01);

  BigEndianWriteU32(UINT32_C(0x01020304), buffer);
  ABSL_CHECK(buffer[0] == 0x01);
  ABSL_CHECK(buffer[1] == 0x02);
  ABSL_CHECK(buffer[2] == 0x03);
  ABSL_CHECK(buffer[3] == 0x04);

  /* Check round trip of serializing and deserializing some test values. */
  static const uint32_t kTestValues[] = {0, 1, 2, 250000UL, UINT32_MAX};
  size_t i;
  for (i = 0; i < sizeof(kTestValues) / sizeof(*kTestValues); ++i) {
    const uint32_t value = kTestValues[i];
    LittleEndianWriteU32(value, buffer);
    ABSL_CHECK(LittleEndianReadU32(buffer) == value);

    BigEndianWriteU32(value, buffer);
    ABSL_CHECK(BigEndianReadU32(buffer) == value);
  }
}

/* Test serialization / deserialization of uint64_t. */
static void TestU64(void) {
  puts("TestU64");
  uint8_t buffer[sizeof(uint64_t)];
  LittleEndianWriteU64(UINT64_C(0x0102030405060708), buffer);
  ABSL_CHECK(buffer[0] == 0x08);
  ABSL_CHECK(buffer[1] == 0x07);
  ABSL_CHECK(buffer[2] == 0x06);
  ABSL_CHECK(buffer[3] == 0x05);
  ABSL_CHECK(buffer[4] == 0x04);
  ABSL_CHECK(buffer[5] == 0x03);
  ABSL_CHECK(buffer[6] == 0x02);
  ABSL_CHECK(buffer[7] == 0x01);

  BigEndianWriteU64(UINT64_C(0x0102030405060708), buffer);
  ABSL_CHECK(buffer[0] == 0x01);
  ABSL_CHECK(buffer[1] == 0x02);
  ABSL_CHECK(buffer[2] == 0x03);
  ABSL_CHECK(buffer[3] == 0x04);
  ABSL_CHECK(buffer[4] == 0x05);
  ABSL_CHECK(buffer[5] == 0x06);
  ABSL_CHECK(buffer[6] == 0x07);
  ABSL_CHECK(buffer[7] == 0x08);

  /* Check round trip of serializing and deserializing some test values. */
  static const uint64_t kTestValues[] = {0, 1, UINT64_C(92) << 55, UINT64_MAX};
  size_t i;
  for (i = 0; i < sizeof(kTestValues) / sizeof(*kTestValues); ++i) {
    const uint64_t value = kTestValues[i];
    LittleEndianWriteU64(value, buffer);
    ABSL_CHECK(LittleEndianReadU64(buffer) == value);

    BigEndianWriteU64(value, buffer);
    ABSL_CHECK(BigEndianReadU64(buffer) == value);
  }
}

/* Test serialization / deserialization of int16_t. */
static void TestS16(void) {
  puts("TestS16");
  uint8_t buffer[sizeof(int16_t)];
  LittleEndianWriteS16(-2, buffer);  /* -2 in two's complement is 0xfffe. */
  ABSL_CHECK(buffer[0] == 0xfe);
  ABSL_CHECK(buffer[1] == 0xff);

  BigEndianWriteS16(-2, buffer);
  ABSL_CHECK(buffer[0] == 0xff);
  ABSL_CHECK(buffer[1] == 0xfe);

  /* Check round trip of serializing and deserializing some test values. */
  static const int16_t kTestValues[] = {0, 1, INT16_MAX, -1, -25000, INT16_MIN};
  size_t i;
  for (i = 0; i < sizeof(kTestValues) / sizeof(*kTestValues); ++i) {
    const int16_t value = kTestValues[i];
    LittleEndianWriteS16(value, buffer);
    ABSL_CHECK(LittleEndianReadS16(buffer) == value);

    BigEndianWriteS16(value, buffer);
    ABSL_CHECK(BigEndianReadS16(buffer) == value);
  }
}

/* Test serialization / deserialization of int32_t. */
static void TestS32(void) {
  puts("TestS32");
  uint8_t buffer[sizeof(int32_t)];
  LittleEndianWriteS32(-2, buffer);
  ABSL_CHECK(buffer[0] == 0xfe);
  ABSL_CHECK(buffer[1] == 0xff);
  ABSL_CHECK(buffer[2] == 0xff);
  ABSL_CHECK(buffer[3] == 0xff);

  BigEndianWriteS32(-2, buffer);
  ABSL_CHECK(buffer[0] == 0xff);
  ABSL_CHECK(buffer[1] == 0xff);
  ABSL_CHECK(buffer[2] == 0xff);
  ABSL_CHECK(buffer[3] == 0xfe);

  /* Check round trip of serializing and deserializing some test values. */
  static const int32_t kTestValues[] = {0, 1, INT32_MAX, -1, -25000, INT32_MIN};
  size_t i;
  for (i = 0; i < sizeof(kTestValues) / sizeof(*kTestValues); ++i) {
    const int32_t value = kTestValues[i];
    LittleEndianWriteS32(value, buffer);
    ABSL_CHECK(LittleEndianReadS32(buffer) == value);

    BigEndianWriteS32(value, buffer);
    ABSL_CHECK(BigEndianReadS32(buffer) == value);
  }
}

/* Test serialization / deserialization of int64_t. */
static void TestS64(void) {
  puts("TestS64");
  uint8_t buffer[sizeof(int64_t)];
  LittleEndianWriteS64(-2, buffer);
  ABSL_CHECK(buffer[0] == 0xfe);
  ABSL_CHECK(buffer[1] == 0xff);
  ABSL_CHECK(buffer[2] == 0xff);
  ABSL_CHECK(buffer[3] == 0xff);
  ABSL_CHECK(buffer[4] == 0xff);
  ABSL_CHECK(buffer[5] == 0xff);
  ABSL_CHECK(buffer[6] == 0xff);
  ABSL_CHECK(buffer[7] == 0xff);

  BigEndianWriteS64(-2, buffer);
  ABSL_CHECK(buffer[0] == 0xff);
  ABSL_CHECK(buffer[1] == 0xff);
  ABSL_CHECK(buffer[2] == 0xff);
  ABSL_CHECK(buffer[3] == 0xff);
  ABSL_CHECK(buffer[4] == 0xff);
  ABSL_CHECK(buffer[5] == 0xff);
  ABSL_CHECK(buffer[6] == 0xff);
  ABSL_CHECK(buffer[7] == 0xfe);

  /* Check round trip of serializing and deserializing some test values. */
  static const int64_t kTestValues[] = {0, 1, INT64_MAX, -1, -25000, INT64_MIN};
  size_t i;
  for (i = 0; i < sizeof(kTestValues) / sizeof(*kTestValues); ++i) {
    const int64_t value = kTestValues[i];
    LittleEndianWriteS64(value, buffer);
    ABSL_CHECK(LittleEndianReadS64(buffer) == value);

    BigEndianWriteS64(value, buffer);
    ABSL_CHECK(BigEndianReadS64(buffer) == value);
  }
}

/* Test serialization / deserialization of 32-bit float. */
static void TestF32(void) {
  puts("TestF32");
  uint8_t buffer[sizeof(float)];
  /* Check round trip of serializing and deserializing some test values. */
  static const float kTestValues[] = {0.0f, 3.71f, -3.71f, 2.5e-6f, 2.5e6f};
  size_t i;
  for (i = 0; i < sizeof(kTestValues) / sizeof(*kTestValues); ++i) {
    const float value = kTestValues[i];
    LittleEndianWriteF32(value, buffer);
    ABSL_CHECK(LittleEndianReadF32(buffer) == value);

    BigEndianWriteF32(value, buffer);
    ABSL_CHECK(BigEndianReadF32(buffer) == value);
  }
}

/* Test serialization / deserialization of 64-bit double. */
static void TestF64(void) {
  puts("TestF64");
  uint8_t buffer[sizeof(double)];
  /* Check round trip of serializing and deserializing some test values. */
  static const double kTestValues[] = {0.0, 3.71, -3.71, 2.5e-6, 2.5e6};
  size_t i;
  for (i = 0; i < sizeof(kTestValues) / sizeof(*kTestValues); ++i) {
    const double value = kTestValues[i];
    LittleEndianWriteF64(value, buffer);
    ABSL_CHECK(LittleEndianReadF64(buffer) == value);

    BigEndianWriteF64(value, buffer);
    ABSL_CHECK(BigEndianReadF64(buffer) == value);
  }
}

/* A "naive" implementation of Fletcher-8 with modulo by 15 on every step. */
static uint8_t Fletcher8Naive(const uint8_t* data, size_t size) {
  int sum1 = 1;
  int sum2 = 0;
  size_t i;
  for (i = 0; i < size; ++i) {
    sum1 = (sum1 + data[i]) % 15;
    sum2 = (sum2 + sum1) % 15;
  }
  return (uint8_t)((sum2 << 4) | sum1);
}

/* Test Fletcher-8 checksum. */
static void TestFletcher8(void) {
  puts("TestFletcher8");
  /* A short 3-byte input where we can compute the checksum by hand:
   *
   *   step   data    sum1   sum2
   *   init              1      0
   *      0     10      11     11
   *      1     20      31     42
   *      2     30      61    103
   *
   *   sum % 15 = 1, sum2 % 15 = 13
   *   => checksum: 0xd1
   */
  const uint8_t kTestShort[] = {10, 20, 30};
  ABSL_CHECK(Fletcher8(kTestShort, sizeof(kTestShort), 1) == 0xd1);

  const int size = 8192;
  uint8_t* data = (uint8_t*)ABSL_CHECK_NOTNULL(malloc(size));

  /* Compare with naive implementation on 8 KB of random data. */
  int trial;
  for(trial = 0; trial < 3; ++trial) {
    int i;
    for (i = 0; i < size; ++i) {
      data[i] = rand() % 256;
    }
    ABSL_CHECK(Fletcher8(data, size, 1) == Fletcher8Naive(data, size));
  }

  /* Check that streaming computation across multiple calls agrees. */
  const uint8_t nonstreaming = Fletcher8(data, size, 1);

  uint8_t streaming = 1;
  int start;
  for (start = 0; start < size;) {
    int block_size = rand() % 500;
    if (size - start < block_size) { block_size = size - start; }
    streaming = Fletcher8(data + start, block_size, streaming);
    start += block_size;
  }
  ABSL_CHECK(streaming == nonstreaming);

  free(data);
}

/* A "naive" implementation of Fletcher-16 with modulo by 255 on every step. */
static uint16_t Fletcher16Naive(const uint8_t* data, size_t size) {
  int32_t sum1 = 1;
  int32_t sum2 = 0;
  size_t i;
  for (i = 0; i < size; ++i) {
    sum1 = (sum1 + data[i]) % 255;
    sum2 = (sum2 + sum1) % 255;
  }
  return (uint16_t)((sum2 << 8) | sum1);
}

/* Test Fletcher-16 checksum. */
static void TestFletcher16(void) {
  puts("TestFletcher16");
  const int size = 8192;
  uint8_t* data = (uint8_t*)ABSL_CHECK_NOTNULL(malloc(size));

  /* Compare with naive implementation on 8 KB of random data. */
  int trial;
  for(trial = 0; trial < 3; ++trial) {
    int i;
    for (i = 0; i < size; ++i) {
      data[i] = rand() % 256;
    }
    ABSL_CHECK(Fletcher16(data, size, 1) == Fletcher16Naive(data, size));
  }

  /* Check that streaming computation across multiple calls agrees. */
  const uint16_t nonstreaming = Fletcher16(data, size, 1);

  uint16_t streaming = 1;
  int start;
  for (start = 0; start < size;) {
    int block_size = rand() % 500;
    if (size - start < block_size) { block_size = size - start; }
    streaming = Fletcher16(data + start, block_size, streaming);
    start += block_size;
  }
  ABSL_CHECK(streaming == nonstreaming);

  free(data);
}

int main(int argc, char** argv) {
  srand(0);
  TestU16();
  TestU32();
  TestU64();
  TestS16();
  TestS32();
  TestS64();
  TestF32();
  TestF64();
  TestFletcher8();
  TestFletcher16();

  puts("PASS");
  return EXIT_SUCCESS;
}
