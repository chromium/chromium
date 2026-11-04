/*
 * Copyright 2019, 2021 Google LLC
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

/* Serializing and deserializing for primitive data types. */

#ifndef AUDIO_DSP_PORTABLE_SERIALIZE_H_
#define AUDIO_DSP_PORTABLE_SERIALIZE_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Little endian byte order. */

/* Deserializes a uint16_t value from bytes in little endian order. */
static uint16_t LittleEndianReadU16(const uint8_t* bytes);
/* Deserializes a uint32_t value from bytes in little endian order. */
static uint32_t LittleEndianReadU32(const uint8_t* bytes);
/* Deserializes a uint64_t value from bytes in little endian order. */
static uint64_t LittleEndianReadU64(const uint8_t* bytes);
/* Deserializes a int16_t value from bytes in little endian order. */
static int16_t LittleEndianReadS16(const uint8_t* bytes);
/* Deserializes a int32_t value from bytes in little endian order. */
static int32_t LittleEndianReadS32(const uint8_t* bytes);
/* Deserializes a int64_t value from bytes in little endian order. */
static int64_t LittleEndianReadS64(const uint8_t* bytes);
/* Deserializes a 32-bit float value from bytes in little endian order. */
static float LittleEndianReadF32(const uint8_t* bytes);
/* Deserializes a 64-bit double value from bytes in little endian order. */
static double LittleEndianReadF64(const uint8_t* bytes);

/* Serializes a uint16_t value to bytes in little endian order. */
static void LittleEndianWriteU16(uint16_t value, uint8_t* bytes);
/* Serializes a uint32_t value to bytes in little endian order. */
static void LittleEndianWriteU32(uint32_t value, uint8_t* bytes);
/* Serializes a uint64_t value to bytes in little endian order. */
static void LittleEndianWriteU64(uint64_t value, uint8_t* bytes);
/* Serializes a int16_t value to bytes in little endian order. */
static void LittleEndianWriteS16(int16_t value, uint8_t* bytes);
/* Serializes a int32_t value to bytes in little endian order. */
static void LittleEndianWriteS32(int32_t value, uint8_t* bytes);
/* Serializes a int64_t value to bytes in little endian order. */
static void LittleEndianWriteS64(int64_t value, uint8_t* bytes);
/* Serializes a 32-bit float value to bytes in little endian order. */
static void LittleEndianWriteF32(float value, uint8_t* bytes);
/* Serializes a 64-bit float value to bytes in little endian order. */
static void LittleEndianWriteF64(double value, uint8_t* bytes);


/* Big endian byte order. */

/* Deserializes a uint16_t value from bytes in big endian order. */
static uint16_t BigEndianReadU16(const uint8_t* bytes);
/* Deserializes a uint32_t value from bytes in big endian order. */
static uint32_t BigEndianReadU32(const uint8_t* bytes);
/* Deserializes a uint64_t value from bytes in big endian order. */
static uint64_t BigEndianReadU64(const uint8_t* bytes);
/* Deserializes a int16_t value from bytes in big endian order. */
static int16_t BigEndianReadS16(const uint8_t* bytes);
/* Deserializes a int32_t value from bytes in big endian order. */
static int32_t BigEndianReadS32(const uint8_t* bytes);
/* Deserializes a int64_t value from bytes in big endian order. */
static int64_t BigEndianReadS64(const uint8_t* bytes);
/* Deserializes a 32-bit float value from bytes in big endian order. */
static float BigEndianReadF32(const uint8_t* bytes);
/* Deserializes a 64-bit double value from bytes in big endian order. */
static double BigEndianReadF64(const uint8_t* bytes);

/* Serializes a uint16_t value to bytes in big endian order. */
static void BigEndianWriteU16(uint16_t value, uint8_t* bytes);
/* Serializes a uint32_t value to bytes in big endian order. */
static void BigEndianWriteU32(uint32_t value, uint8_t* bytes);
/* Serializes a uint64_t value to bytes in big endian order. */
static void BigEndianWriteU64(uint64_t value, uint8_t* bytes);
/* Serializes a int16_t value to bytes in big endian order. */
static void BigEndianWriteS16(int16_t value, uint8_t* bytes);
/* Serializes a int32_t value to bytes in big endian order. */
static void BigEndianWriteS32(int32_t value, uint8_t* bytes);
/* Serializes a int64_t value to bytes in big endian order. */
static void BigEndianWriteS64(int64_t value, uint8_t* bytes);
/* Serializes a 32-bit float value to bytes in big endian order. */
static void BigEndianWriteF32(float value, uint8_t* bytes);
/* Serializes a 64-bit float value to bytes in big endian order. */
static void BigEndianWriteF64(double value, uint8_t* bytes);

/* Fletcher checksums. These checksums approach the error detecting ability of
 * CRCs of the same size, but are cheaper and simpler to compute. Fletcher
 * checksums are position-dependent, meaning that reordering the bytes usually
 * changes the checksum. [https://en.wikipedia.org/wiki/Fletcher%27s_checksum]
 */

/* Computes an 8-bit "Fletcher-8" checksum:
 *
 *   sum1 = (init1 + D0 + D1 + ...) % 15,
 *   sum2 = ((init2 + init1 + D0) + (init2 + init1 + D0 + D1) + ...) % 15,
 *   checksum = (sum2 << 4) | sum1,
 *
 * where D0, D1, etc. are data bytes and init1 and init2 derive from `init`.
 * Fletcher's original work did not consider an 8-bit size, but one can be
 * defined in a similar manner. See Mark Adler's SO post for discussion:
 * https://stackoverflow.com/a/13497669/13223986
 *
 * The `init` arg specifies an initial checksum for starting the two sums. A
 * good starting value is 1. This arg is useful for incrementally computing a
 * checksum over multiple calls:
 *
 *   uint8_t checksum = 1;
 *   while (fgets(line, sizeof(line), f)) {  // Read lines from a text file.
 *      checksum = Fletcher8(line, strlen(line), checksum);
 *   }
 */
uint8_t Fletcher8(const uint8_t* data, size_t size, uint8_t init);
/* Computes the 16-bit Fletcher-16 checksum:
 *
 *   sum1 = (init1 + D0 + D1 + ...) % 255,
 *   sum2 = ((init2 + init1 + D0) + (init2 + init1 + D0 + D1) + ...) % 255,
 *   checksum = (sum2 << 8) | sum1.
 *
 * The `init` arg is used as described above for Fletcher8. A good starting
 * value for `init` is 1.
 */
uint16_t Fletcher16(const uint8_t* data, size_t size, uint16_t init);


/* Implementation details only below this line. ----------------------------- */

/* Note: The deserialization implementations below have a pattern of
 * subscripting, casting, bitshifting, and finally bitwise or. C/C++ operator
 * precedence is in this order, so order of operations is correct without
 * needing many parentheses:
 *
 *   Precedence   Operator            Associativity
 *   1            subscripting a[]    Left to right
 *   3            casting (type)      Right to left
 *   7            Bitshift <<         Left to right
 *   13           Bitwise or |        Left to right
 *
 * [Full table: https://en.cppreference.com/w/cpp/language/operator_precedence]
 */
static uint16_t LittleEndianReadU16(const uint8_t* bytes) {
  /* GCC generates better assembly if we build up the result in uint_fast16_t,
   * then cast the final result down to uint16_t.
   */
  return (uint16_t)((uint_fast16_t)bytes[0]
                    | (uint_fast16_t)bytes[1] << 8);
}

static uint32_t LittleEndianReadU32(const uint8_t* bytes) {
  return (uint32_t)((uint_fast32_t)bytes[0]
                    | (uint_fast32_t)bytes[1] << 8
                    | (uint_fast32_t)bytes[2] << 16
                    | (uint_fast32_t)bytes[3] << 24);
}

static uint64_t LittleEndianReadU64(const uint8_t* bytes) {
  return (uint64_t)((uint_fast64_t)bytes[0]
                    | (uint_fast64_t)bytes[1] << 8
                    | (uint_fast64_t)bytes[2] << 16
                    | (uint_fast64_t)bytes[3] << 24
                    | (uint_fast64_t)bytes[4] << 32
                    | (uint_fast64_t)bytes[5] << 40
                    | (uint_fast64_t)bytes[6] << 48
                    | (uint_fast64_t)bytes[7] << 56);
}

static void LittleEndianWriteU16(uint16_t value, uint8_t* bytes) {
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
}

static void LittleEndianWriteU32(uint32_t value, uint8_t* bytes) {
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
  bytes[2] = (uint8_t)(value >> 16);
  bytes[3] = (uint8_t)(value >> 24);
}

static void LittleEndianWriteU64(uint64_t value, uint8_t* bytes) {
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
  bytes[2] = (uint8_t)(value >> 16);
  bytes[3] = (uint8_t)(value >> 24);
  bytes[4] = (uint8_t)(value >> 32);
  bytes[5] = (uint8_t)(value >> 40);
  bytes[6] = (uint8_t)(value >> 48);
  bytes[7] = (uint8_t)(value >> 56);
}

/* Deserializes a uint16_t value from bytes in big endian order. */
static uint16_t BigEndianReadU16(const uint8_t* bytes) {
  return (uint16_t)((uint_fast16_t)bytes[0] << 8
                    | (uint_fast16_t)bytes[1]);
}

/* Deserializes a uint32_t value from bytes in big endian order. */
static uint32_t BigEndianReadU32(const uint8_t* bytes) {
  return (uint32_t)((uint_fast32_t)bytes[0] << 24
                    | (uint_fast32_t)bytes[1] << 16
                    | (uint_fast32_t)bytes[2] << 8
                    | (uint_fast32_t)bytes[3]);
}

/* Deserializes a uint64_t value from bytes in big endian order. */
static uint64_t BigEndianReadU64(const uint8_t* bytes) {
  return (uint64_t)((uint_fast64_t)bytes[0] << 56
                    | (uint_fast64_t)bytes[1] << 48
                    | (uint_fast64_t)bytes[2] << 40
                    | (uint_fast64_t)bytes[3] << 32
                    | (uint_fast64_t)bytes[4] << 24
                    | (uint_fast64_t)bytes[5] << 16
                    | (uint_fast64_t)bytes[6] << 8
                    | (uint_fast64_t)bytes[7]);
}

/* Serializes a uint16_t value to bytes in big endian order. */
static void BigEndianWriteU16(uint16_t value, uint8_t* bytes) {
  bytes[0] = (uint8_t)(value >> 8);
  bytes[1] = (uint8_t)value;
}

/* Serializes a uint32_t value to bytes in big endian order. */
static void BigEndianWriteU32(uint32_t value, uint8_t* bytes) {
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

/* Serializes a uint64_t value to bytes in big endian order. */
static void BigEndianWriteU64(uint64_t value, uint8_t* bytes) {
  bytes[0] = (uint8_t)(value >> 56);
  bytes[1] = (uint8_t)(value >> 48);
  bytes[2] = (uint8_t)(value >> 40);
  bytes[3] = (uint8_t)(value >> 32);
  bytes[4] = (uint8_t)(value >> 24);
  bytes[5] = (uint8_t)(value >> 16);
  bytes[6] = (uint8_t)(value >> 8);
  bytes[7] = (uint8_t)value;
}

/* Casting an unsigned integer to a signed type has implementation defined
 * behavior if the value exceeds the range of the signed type, e.g. this may
 * raise a signal. To avoid this, we pun the type through a union, like Google
 * Protobufs do:
 * https://github.com/protocolbuffers/protobuf/blob/master/src/google/protobuf/wire_format_lite.h
 */
static int16_t LittleEndianReadS16(const uint8_t* bytes) {
  union {int16_t s16; uint16_t u16;} u;
  u.u16 = LittleEndianReadU16(bytes);
  return u.s16;
}

static int32_t LittleEndianReadS32(const uint8_t* bytes) {
  union {int32_t s32; uint32_t u32;} u;
  u.u32 = LittleEndianReadU32(bytes);
  return u.s32;
}

static int64_t LittleEndianReadS64(const uint8_t* bytes) {
  union {int64_t s64; uint64_t u64;} u;
  u.u64 = LittleEndianReadU64(bytes);
  return u.s64;
}

static float LittleEndianReadF32(const uint8_t* bytes) {
  union {float f32; uint32_t u32;} u;
  u.u32 = LittleEndianReadU32(bytes);
  return u.f32;
}

static double LittleEndianReadF64(const uint8_t* bytes) {
  union {double f64; uint64_t u64;} u;
  u.u64 = LittleEndianReadU64(bytes);
  return u.f64;
}

static void LittleEndianWriteS16(int16_t value, uint8_t* bytes) {
  /* Directly casting signed int to unsigned is Ok. */
  LittleEndianWriteU16((uint16_t)value, bytes);
}

static void LittleEndianWriteS32(int32_t value, uint8_t* bytes) {
  LittleEndianWriteU32((uint32_t)value, bytes);
}

static void LittleEndianWriteS64(int64_t value, uint8_t* bytes) {
  LittleEndianWriteU64((uint64_t)value, bytes);
}

static void LittleEndianWriteF32(float value, uint8_t* bytes) {
  union {float f32; uint32_t u32;} u;
  u.f32 = value;
  LittleEndianWriteU32(u.u32, bytes);
}

static void LittleEndianWriteF64(double value, uint8_t* bytes) {
  union {double f64; uint64_t u64;} u;
  u.f64 = value;
  LittleEndianWriteU64(u.u64, bytes);
}

static int16_t BigEndianReadS16(const uint8_t* bytes) {
  union {int16_t s16; uint16_t u16;} u;
  u.u16 = BigEndianReadU16(bytes);
  return u.s16;
}

static int32_t BigEndianReadS32(const uint8_t* bytes) {
  union {int32_t s32; uint32_t u32;} u;
  u.u32 = BigEndianReadU32(bytes);
  return u.s32;
}

static int64_t BigEndianReadS64(const uint8_t* bytes) {
  union {int64_t s64; uint64_t u64;} u;
  u.u64 = BigEndianReadU64(bytes);
  return u.s64;
}

static float BigEndianReadF32(const uint8_t* bytes) {
  union {float f32; uint32_t u32;} u;
  u.u32 = BigEndianReadU32(bytes);
  return u.f32;
}

static double BigEndianReadF64(const uint8_t* bytes) {
  union {double f64; uint64_t u64;} u;
  u.u64 = BigEndianReadU64(bytes);
  return u.f64;
}

static void BigEndianWriteS16(int16_t value, uint8_t* bytes) {
  BigEndianWriteU16((uint16_t)value, bytes);
}

static void BigEndianWriteS32(int32_t value, uint8_t* bytes) {
  BigEndianWriteU32((uint32_t)value, bytes);
}

static void BigEndianWriteS64(int64_t value, uint8_t* bytes) {
  BigEndianWriteU64((uint64_t)value, bytes);
}

static void BigEndianWriteF32(float value, uint8_t* bytes) {
  union {float f32; uint32_t u32;} u;
  u.f32 = value;
  BigEndianWriteU32(u.u32, bytes);
}

static void BigEndianWriteF64(double value, uint8_t* bytes) {
  union {double f64; uint64_t u64;} u;
  u.f64 = value;
  BigEndianWriteU64(u.u64, bytes);
}

#ifdef __cplusplus
}  /* extern "C" */
#endif
#endif /* AUDIO_DSP_PORTABLE_SERIALIZE_H_ */
