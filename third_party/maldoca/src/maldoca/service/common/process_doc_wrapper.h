// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file.h"
#include "maldoca/service/proto/doc_type.pb.h"
#include "maldoca/service/proto/maldoca_service.pb.h"
#include "maldoca/service/proto/processing_config.pb.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace third_party_maldoca {

// Uses the extension form |file_path| to determine the appropriate
// maldoca::DocType. If the extensions is not supported by maldoca,
// returns maldoca::DocType::UNKNOWN_TYPE.
maldoca::DocType GetDocType(base::FilePath file_path);

// Wrapper function around maldoca::ProcessDocumentResponse.ProcessDoc
// that parses the file at |file_path| and sets |contains_macros|, |success|,
// |error_code| and |error_message| based on the results of that parsing
void AnalyzeOfficeDocument(base::File office_file,
                           const base::FilePath& file_path,
                           bool& contains_macro, bool& success,
                           std::string& error_code, std::string& error_message);

// Determines whether or not the Office file contains macros based on the
// the ParsedDocument in |document_response|.
bool HasMacro(const maldoca::ProcessDocumentResponse* document_response);

// Manually constructing the appropriate maldoca::ProcessorConfig to be used
// as part of the maldoca::ProcessDocumentRequest because
// Chromium uses protobuf_lite,
void BuildProcessorConfig(maldoca::ProcessorConfig* processor_config);

// Processes the payload from the absl::Status returnd from
// maldoca::ProcessDocumentResponse.ProcessDoc and sets |success|, |error_code|,
// and |error_message.
void ProcessStatusPayload(absl::Status status, bool& success,
                          std::string& error_code, std::string& error_message);

// Determines whether or not the final extension of |file_path| is equal
// to |extension|. Using this instead of MatchesExtension because it is
// not allowed when the sandbox type is kService.
bool ExtensionEqualInCaseSensitive(base::FilePath file_path,
                                   std::string extension);

}  // namespace third_party_maldoca
