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

#ifndef ART_LIBARTBASE_BASE_DUMPABLE_H_
#define ART_LIBARTBASE_BASE_DUMPABLE_H_

#include <ostream>

#include "macros.h"

namespace art {

// A convenience to allow any class with a "Dump(std::ostream& os)" member function
// but without an operator<< to be used as if it had an operator<<. Use like this:
//
//   os << Dumpable<MyType>(my_type_instance);
//
template<typename T>
class Dumpable final {
 public:
  explicit Dumpable(const T& value) : value_(value) {
  }

  void Dump(std::ostream& os) const {
    value_.Dump(os);
  }

 private:
  const T& value_;

  DISALLOW_COPY_AND_ASSIGN(Dumpable);
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const Dumpable<T>& rhs) {
  rhs.Dump(os);
  return os;
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_DUMPABLE_H_
