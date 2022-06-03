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

#ifndef MALDOCA_BASE_STATUS_PAYLOAD_H_
#define MALDOCA_BASE_STATUS_PAYLOAD_H_

#include "zetasql/base/status_payload.h"

namespace maldoca {

// This one is awkward. We can't hide the ZetaSql part.
using ::zetasql_base::kZetaSqlTypeUrlPrefix;

using ::zetasql_base::AttachPayload;
using ::zetasql_base::GetTypeUrl;

}  // namespace maldoca

#endif  // MALDOCA_BASE_STATUS_PAYLOAD_H_