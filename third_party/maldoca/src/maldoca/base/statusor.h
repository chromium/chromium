// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MALDOCA_BASE_STATUSOR_H_
#define MALDOCA_BASE_STATUSOR_H_

#ifdef MALDOCA_CHROME

#include "absl/status/statusor.h"

namespace maldoca {

// TODO(X): remove this file and use absl::StatusOr directly
using absl::StatusOr;

}  // namespace maldoca

#else

#include "zetasql/base/statusor.h"

namespace maldoca {

using ::zetasql_base::StatusOr;

}  // namespace maldoca
#endif  // MALDOCA_CHROME
#endif  // MALDOCA_BASE_STATUSOR_H_
