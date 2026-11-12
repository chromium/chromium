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

#include <string>

#include "modifiers.h"

namespace art {

std::string PrettyJavaAccessFlags(uint32_t access_flags) {
  std::string result;
  if ((access_flags & kAccPublic) != 0) {
    result += "public ";
  }
  if ((access_flags & kAccProtected) != 0) {
    result += "protected ";
  }
  if ((access_flags & kAccPrivate) != 0) {
    result += "private ";
  }
  if ((access_flags & kAccFinal) != 0) {
    result += "final ";
  }
  if ((access_flags & kAccStatic) != 0) {
    result += "static ";
  }
  if ((access_flags & kAccAbstract) != 0) {
    result += "abstract ";
  }
  if ((access_flags & kAccInterface) != 0) {
    result += "interface ";
  }
  if ((access_flags & kAccTransient) != 0) {
    result += "transient ";
  }
  if ((access_flags & kAccVolatile) != 0) {
    result += "volatile ";
  }
  if ((access_flags & kAccSynchronized) != 0) {
    result += "synchronized ";
  }
  return result;
}

}  // namespace art
