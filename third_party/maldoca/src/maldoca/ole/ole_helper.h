/*
 * Copyright 2021 Google LLC
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

#ifndef MALDOCA_OLE_OLE_HELPER_H_
#define MALDOCA_OLE_OLE_HELPER_H_

#include "absl/status/status.h"
#include "maldoca/ole/dir.h"
#include "maldoca/ole/header.h"

namespace maldoca {

absl::Status ReadHeaderFatRoot(absl::string_view content, OLEHeader *header,
                               std::vector<uint32_t> *fat,
                               OLEDirectoryEntry *root,
                               std::vector<OLEDirectoryEntry *> *dir_entries,
                               std::string *directory_stream);
}  // namespace maldoca

#endif  // MALDOCA_OLE_OLE_HELPER_H_
