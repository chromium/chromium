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

// A simple API to extract VBA code from Docfile or OOXML input.
//
// See maldoca/ole/vba_extract_test.cc to see how
// this API can be used.

#ifndef MALDOCA_OLE_VBA_EXTRACT_H_
#define MALDOCA_OLE_VBA_EXTRACT_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "maldoca/ole/proto/extract_vba_settings.pb.h"
#include "maldoca/ole/proto/vba_extraction.pb.h"
#include "maldoca/ole/vba.h"

namespace maldoca {

// Test that the 76 first bytes of input contain a OLE2 header and
// that we have a lowercase match of the strings "project" and
// "vba". If both tests succeeds, assert that input most likely
// contains VBA code and return true.
bool LikelyOLE2WithVBAContent(absl::string_view content);

// Extract VBA code chunks and OLE directory content from a string. Errors
// are added to error, but this doesn't necessarily mean that no VBA or
// directory content has been extracted, it just means that some errors
// were encountered. This method properly identifies Docfile, OOXML,
// Office2003 XML or MSO content. If filename's content contains
// extractable VBA content, it will be placed in code_chunks as individual
// VBACodeChunk messages.
//
// Sample usage:
//
// ExtractDirectoryAndVBAFromString(content, &directory, &code, &error);
// if (error.empty() && code.code_size() == 0) {
//   LOG(INFO) << "No VBA found, no errors.";
// } else if (!error.empty() && code.code_size() == 0) {
//   LOG(INFO) << "No VBA found, only errors: " << error;
// }
// } else if (!error.empty() && code.code_size() != 0) {
//   LOG(INFO) << "VBA found, some errors encountered: " << error;
// } else if (error.empty() && code.code_size() != 0) {
//   LOG(INFO) << "VBA found, no errors.";
// }
void ExtractDirectoryAndVBAFromString(absl::string_view content,
                                      OLEDirectoryMessage *directory,
                                      VBACodeChunks *code_chunks,
                                      std::string *error);

// Extract only VBA content from a file that will be read.
void ExtractVBAFromFile(const std::string &filename, VBACodeChunks *code_chunks,
                        std::string *error, bool xor_decode_file = false);

// Same as ExtractVBAFromFile, but the content is passed as a string.
void ExtractVBAFromString(absl::string_view content, VBACodeChunks *code_chunks,
                          std::string *error);

// Same as ExtractVBAFromString, but OOXML content is not
// considered.
void ExtractVBAFromStringLightweight(absl::string_view content,
                                     VBACodeChunks *code_chunks,
                                     std::string *error);

// Calls appropiate vba extraction method based on input settings, based on the
// vba extraction method specified in settings it will call the default
// ExtractVBAFromString or ExtractVBAFromStringLightweight which excludes OOXML
// content.
absl::StatusOr<VBACodeChunks> ExtractVBAFromStringWithSettings(
    absl::string_view content, const vba::ExtractVBASettings &settings);

// Attempts to extract the OLE directory and VBA code from content (under
// the assumption that we have already determined that it's some OLE2
// content.) Error message is propagated through the absl::Status return value.
absl::Status ExtractFromOLE2String(absl::string_view content,
                                   OLEDirectoryMessage *dir,
                                   VBACodeChunks *code_chunks);
// Do the smallest amount of work (in our context) to determine that
// content is most likely an OLE2 file. Return true if it's the case.
bool IsOLE2Content(absl::string_view content);
}  // namespace maldoca

#endif  // MALDOCA_OLE_VBA_EXTRACT_H_
