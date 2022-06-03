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

#ifndef MALDOCA_BASE_STATUS_BUILDER_H_
#define MALDOCA_BASE_STATUS_BUILDER_H_

#ifndef MALDOCA_CHROME

#include "zetasql/base/status_builder.h"

namespace maldoca {

using ::zetasql_base::StatusBuilder;

using ::zetasql_base::AbortedErrorBuilder;
using ::zetasql_base::AlreadyExistsErrorBuilder;
using ::zetasql_base::CancelledErrorBuilder;
using ::zetasql_base::DataLossErrorBuilder;
using ::zetasql_base::DeadlineExceededErrorBuilder;
using ::zetasql_base::FailedPreconditionErrorBuilder;
using ::zetasql_base::InternalErrorBuilder;
using ::zetasql_base::InvalidArgumentErrorBuilder;
using ::zetasql_base::NotFoundErrorBuilder;
using ::zetasql_base::OutOfRangeErrorBuilder;
using ::zetasql_base::PermissionDeniedErrorBuilder;
using ::zetasql_base::ResourceExhaustedErrorBuilder;
using ::zetasql_base::UnauthenticatedErrorBuilder;
using ::zetasql_base::UnavailableErrorBuilder;
using ::zetasql_base::UnimplementedErrorBuilder;
using ::zetasql_base::UnknownErrorBuilder;

}  // namespace maldoca

#endif  // MALDOCA_CHROME
#endif  // MALDOCA_BASE_STATUS_BUILDER_H_
