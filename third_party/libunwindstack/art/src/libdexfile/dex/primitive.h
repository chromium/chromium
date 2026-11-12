/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBDEXFILE_DEX_PRIMITIVE_H_
#define ART_LIBDEXFILE_DEX_PRIMITIVE_H_

#include <sys/types.h>

#include <android-base/logging.h>

#include "base/macros.h"

namespace art {

static constexpr size_t kObjectReferenceSize = 4;

constexpr size_t ComponentSizeShiftWidth(size_t component_size) {
  return component_size == 1u ? 0u :
      component_size == 2u ? 1u :
          component_size == 4u ? 2u :
              component_size == 8u ? 3u : 0u;
}

class Primitive {
 public:
  enum Type {
    kPrimNot = 0,
    kPrimBoolean,
    kPrimByte,
    kPrimChar,
    kPrimShort,
    kPrimInt,
    kPrimLong,
    kPrimFloat,
    kPrimDouble,
    kPrimVoid,
    kPrimLast = kPrimVoid
  };

  static constexpr Type GetType(char type) {
    switch (type) {
      case 'B':
        return kPrimByte;
      case 'C':
        return kPrimChar;
      case 'D':
        return kPrimDouble;
      case 'F':
        return kPrimFloat;
      case 'I':
        return kPrimInt;
      case 'J':
        return kPrimLong;
      case 'S':
        return kPrimShort;
      case 'Z':
        return kPrimBoolean;
      case 'V':
        return kPrimVoid;
      default:
        return kPrimNot;
    }
  }

  static constexpr size_t ComponentSizeShift(Type type) {
    switch (type) {
      case kPrimVoid:
      case kPrimBoolean:
      case kPrimByte:    return 0;
      case kPrimChar:
      case kPrimShort:   return 1;
      case kPrimInt:
      case kPrimFloat:   return 2;
      case kPrimLong:
      case kPrimDouble:  return 3;
      case kPrimNot:     return ComponentSizeShiftWidth(kObjectReferenceSize);
    }
    LOG(FATAL) << "Invalid type " << static_cast<int>(type);
    UNREACHABLE();
  }

  static constexpr size_t ComponentSize(Type type) {
    switch (type) {
      case kPrimVoid:    return 0;
      case kPrimBoolean:
      case kPrimByte:    return 1;
      case kPrimChar:
      case kPrimShort:   return 2;
      case kPrimInt:
      case kPrimFloat:   return 4;
      case kPrimLong:
      case kPrimDouble:  return 8;
      case kPrimNot:     return kObjectReferenceSize;
    }
    LOG(FATAL) << "Invalid type " << static_cast<int>(type);
    UNREACHABLE();
  }

  static const char* Descriptor(Type type) {
    switch (type) {
      case kPrimBoolean:
        return "Z";
      case kPrimByte:
        return "B";
      case kPrimChar:
        return "C";
      case kPrimShort:
        return "S";
      case kPrimInt:
        return "I";
      case kPrimFloat:
        return "F";
      case kPrimLong:
        return "J";
      case kPrimDouble:
        return "D";
      case kPrimVoid:
        return "V";
      default:
        LOG(FATAL) << "Primitive char conversion on invalid type " << static_cast<int>(type);
        return nullptr;
    }
  }

  static const char* PrettyDescriptor(Type type);

  // Returns the descriptor corresponding to the boxed type of |type|.
  static const char* BoxedDescriptor(Type type);

  // Returns true if |type| is an numeric type.
  static constexpr bool IsNumericType(Type type) {
    switch (type) {
      case Primitive::Type::kPrimNot: return false;
      case Primitive::Type::kPrimBoolean: return false;
      case Primitive::Type::kPrimByte: return true;
      case Primitive::Type::kPrimChar: return true;
      case Primitive::Type::kPrimShort: return true;
      case Primitive::Type::kPrimInt: return true;
      case Primitive::Type::kPrimLong: return true;
      case Primitive::Type::kPrimFloat: return true;
      case Primitive::Type::kPrimDouble: return true;
      case Primitive::Type::kPrimVoid: return false;
    }
    LOG(FATAL) << "Invalid type " << static_cast<int>(type);
    UNREACHABLE();
  }

  // Return trues if |type| is a signed numeric type.
  static constexpr bool IsSignedNumericType(Type type) {
    switch (type) {
      case Primitive::Type::kPrimNot: return false;
      case Primitive::Type::kPrimBoolean: return false;
      case Primitive::Type::kPrimByte: return true;
      case Primitive::Type::kPrimChar: return false;
      case Primitive::Type::kPrimShort: return true;
      case Primitive::Type::kPrimInt: return true;
      case Primitive::Type::kPrimLong: return true;
      case Primitive::Type::kPrimFloat: return true;
      case Primitive::Type::kPrimDouble: return true;
      case Primitive::Type::kPrimVoid: return false;
    }
    LOG(FATAL) << "Invalid type " << static_cast<int>(type);
    UNREACHABLE();
  }

  // Returns the number of bits required to hold the largest
  // positive number that can be represented by |type|.
  static constexpr size_t BitsRequiredForLargestValue(Type type) {
    switch (type) {
      case Primitive::Type::kPrimNot: return 0u;
      case Primitive::Type::kPrimBoolean: return 1u;
      case Primitive::Type::kPrimByte: return 7u;
      case Primitive::Type::kPrimChar: return 16u;
      case Primitive::Type::kPrimShort: return 15u;
      case Primitive::Type::kPrimInt: return 31u;
      case Primitive::Type::kPrimLong: return 63u;
      case Primitive::Type::kPrimFloat: return 128u;
      case Primitive::Type::kPrimDouble: return 1024u;
      case Primitive::Type::kPrimVoid: return 0u;
    }
  }

  // Returns true if it is possible to widen type |from| to type |to|. Both |from| and
  // |to| should be numeric primitive types.
  static bool IsWidenable(Type from, Type to) {
    if (!IsNumericType(from) || !IsNumericType(to)) {
      // Widening is only applicable between numeric types.
      return false;
    }
    if (IsSignedNumericType(from) && !IsSignedNumericType(to)) {
      // Nowhere to store the sign bit in |to|.
      return false;
    }
    if (BitsRequiredForLargestValue(from) > BitsRequiredForLargestValue(to)) {
      // The from,to pair corresponds to a narrowing.
      return false;
    }
    return true;
  }

  static bool Is64BitType(Type type) {
    return type == kPrimLong || type == kPrimDouble;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Primitive);
};

std::ostream& operator<<(std::ostream& os, Primitive::Type state);

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_PRIMITIVE_H_
