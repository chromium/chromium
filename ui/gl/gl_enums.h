// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_ENUMS_H_
#define UI_GL_GL_ENUMS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "ui/gl/gl_export.h"

namespace gl {

class GL_EXPORT GLEnums {
 public:
  struct EnumToString {
    uint32_t value;
    const char* name;
  };

  static std::string GetStringEnum(uint32_t value);
  static std::string GetStringBool(uint32_t value);
  static std::string GetStringError(uint32_t value);

 private:
  static const EnumToString* const enum_to_string_table_;
  static const size_t enum_to_string_table_len_;
};

}  // namespace gl

#endif  // UI_GL_GL_ENUMS_H_

