/*
 * Copyright 2020 Google LLC
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

#ifndef AUDIO_DSP_OPEN_SOURCE_PORTING_H_
#define AUDIO_DSP_OPEN_SOURCE_PORTING_H_

#include <iostream>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

class MathUtil {
 public:
  enum QuadraticRootType {kNoRealRoots = 0, kAmbiguous = 1, kTwoRealRoots = 2};

  static QuadraticRootType RealRootsForQuadratic(long double a,
                                                 long double b,
                                                 long double c,
                                                 long double *r1,
                                                 long double *r2);

  static bool RealRootsForCubic(long double a,
                                long double b,
                                long double c,
                                long double *r1,
                                long double *r2,
                                long double *r3);
};

// TODO: glog actually includes a demangler function, but it isn't obvious how
//       to access it with our BUILD setup.
namespace util {
// Note that these don't do any demangling in the open source code.
bool Demangle(const char *mangled, char *out, int out_size);
std::string Demangle(const char* mangled);
}  // namespace util

#endif  // AUDIO_DSP_OPEN_SOURCE_PORTING_H_
