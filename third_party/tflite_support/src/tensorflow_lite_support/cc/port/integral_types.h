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

#ifndef TENSORFLOW_LITE_SUPPORT_CC_PORT_INTEGRAL_TYPES_H_
#define TENSORFLOW_LITE_SUPPORT_CC_PORT_INTEGRAL_TYPES_H_

// Add namespace here to avoid conflict with other libraries.
namespace tflite {

typedef signed char schar;
typedef signed char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned int char32;
typedef unsigned long long uint64;
typedef unsigned long uword_t;

#define GG_LONGLONG(x) x##LL
#define GG_ULONGLONG(x) x##ULL
#define GG_LL_FORMAT "ll"  // As in "%lld". Note that "q" is poor form also.
#define GG_LL_FORMAT_W L"ll"

const uint8   kuint8max{0xFF};
const uint16 kuint16max{0xFFFF};
const uint32 kuint32max{0xFFFFFFFF};
const uint64 kuint64max{GG_ULONGLONG(0xFFFFFFFFFFFFFFFF)};
const int8 kint8min{~0x7F};
const int8 kint8max{0x7F};
const int16 kint16min{~0x7FFF};
const int16 kint16max{0x7FFF};
const int32 kint32min{~0x7FFFFFFF};
const int32 kint32max{0x7FFFFFFF};
const int64 kint64min{GG_LONGLONG(~0x7FFFFFFFFFFFFFFF)};
const int64 kint64max{GG_LONGLONG(0x7FFFFFFFFFFFFFFF)};

typedef uint64 Fprint;
static const Fprint kIllegalFprint = 0;
static const Fprint kMaxFprint = GG_ULONGLONG(0xFFFFFFFFFFFFFFFF);

}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_CC_PORT_INTEGRAL_TYPES_H_
