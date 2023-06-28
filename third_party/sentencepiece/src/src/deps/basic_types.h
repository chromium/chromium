#ifndef THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_BASIC_TYPES_H_
#define THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_BASIC_TYPES_H_

#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t char32;
typedef uint32_t uint32;
typedef uint64_t uint64;

static constexpr uint8 kuint8max = ((uint8)0xFF);
static constexpr uint16 kuint16max = ((uint16)0xFFFF);
static constexpr uint32 kuint32max = ((uint32)0xFFFFFFFF);
static constexpr uint64 kuint64max = ((uint64)(0xFFFFFFFFFFFFFFFF));
static constexpr int8 kint8min = ((int8)~0x7F);
static constexpr int8 kint8max = ((int8)0x7F);
static constexpr int16 kint16min = ((int16)~0x7FFF);
static constexpr int16 kint16max = ((int16)0x7FFF);
static constexpr int32 kint32min = ((int32)~0x7FFFFFFF);
static constexpr int32 kint32max = ((int32)0x7FFFFFFF);
static constexpr int64 kint64min = ((int64)(~0x7FFFFFFFFFFFFFFF));
static constexpr int64 kint64max = ((int64)(0x7FFFFFFFFFFFFFFF));

#endif  // THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_BASIC_TYPES_H_
