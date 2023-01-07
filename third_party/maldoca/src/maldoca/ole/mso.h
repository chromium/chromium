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

// Detection and decompression of ActiveMime/MSO files in XML documents.
//
// This provides support to extract OLE2 byte streams stored in XML
// document using the ActiveMime/MSO zlib-compressed format. We don't
// need to encapsulate this in an object and a typedef will add
// nothing. A non instantiable class is used to provide a scope to the
// methods defined to read MSO OLE streams and decompress them.

#ifndef MALDOCA_OLE_MSO_H_
#define MALDOCA_OLE_MSO_H_

#include <string>

#include "absl/strings/string_view.h"

namespace maldoca {

class MSOContent {
 public:
  // Extract zlib-compressed ActiveMime/MSO that might be stored in
  // xml, which must be parseable XML. The MSO stream is expected to
  // be found at the /docSuppData/binData path in the XML
  // document. Return true if the operation succeeds, with the
  // extracted MSO in mso_content and the XML node name attribute in
  // mso_filename.
  static bool GetBinDataFromXML(absl::string_view xml,
                                std::string *mso_filename,
                                std::string *mso_content);

  // Attempt to extract final OLE2 content from the zlib-compressed
  // ActiveMime/MSO input mso. Return true if the operation succeeds,
  // with the OLE2 content stored in ole2.
  static bool GetOLE2Data(absl::string_view mso, std::string *ole2);

 private:
  MSOContent() = delete;
  ~MSOContent() = delete;
  MSOContent(const MSOContent&) = delete;
  MSOContent& operator=(const MSOContent&) = delete;
};
}  // namespace maldoca

#endif  // MALDOCA_OLE_MSO_H_
