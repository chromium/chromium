/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_HIDDENAPI_DOMAIN_H_
#define ART_LIBARTBASE_BASE_HIDDENAPI_DOMAIN_H_

namespace art {
namespace hiddenapi {

// List of domains supported by the hidden API access checks. Domain with a lower
// ordinal is considered more "trusted", i.e. always allowed to access members of
// domains with a greater ordinal. Access checks are performed when code tries to
// access a method/field from a more trusted domain than itself.
enum class Domain : char {
  kCorePlatform = 0,
  kPlatform,
  kApplication,
};

inline bool IsDomainMoreTrustedThan(Domain domainA, Domain domainB) {
  return static_cast<char>(domainA) <= static_cast<char>(domainB);
}

}  // namespace hiddenapi
}  // namespace art


#endif  // ART_LIBARTBASE_BASE_HIDDENAPI_DOMAIN_H_
