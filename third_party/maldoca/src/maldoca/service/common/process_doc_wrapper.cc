// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "maldoca/service/common/process_doc_wrapper.h"

#include "build/build_config.h"

#include "base/files/file_util.h"
#if defined(OS_LINUX)
#include "base/strings/string_util.h"
#else
#include "base/strings/string_util_win.h"
#endif
#include "base/strings/utf_string_conversions.h"
#include "maldoca/base/status.h"
#include "maldoca/base/status_proto.pb.h"
#include "maldoca/service/common/process_doc.h"

namespace third_party_maldoca {


bool ExtensionEqualInCaseSensitive(base::FilePath file_path, std::string extension){
  #if defined(OS_LINUX)
  std::string file_extension = file_path.FinalExtension();
  return base::CompareCaseInsensitiveASCII(file_extension, extension) == 0;
  #else
  std::wstring file_extension = file_path.FinalExtension();
  return base::CompareCaseInsensitiveASCII(file_extension, base::UTF8ToWide(extension)) == 0;
  #endif
}

maldoca::DocType GetDocType(base::FilePath file_path) {

  if (ExtensionEqualInCaseSensitive(file_path, ".doc")) {
    return maldoca::DocType::DOC;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".docm")) {
    return maldoca::DocType::DOCM;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".docx")) {
    return maldoca::DocType::DOCX;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".pps")) {
    return maldoca::DocType::PPS;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".ppsx")) {
    return maldoca::DocType::PPSX;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".ppt")){
    return maldoca::DocType::PPT;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".pptx")) {
    return maldoca::DocType::PPTX;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".xla"))  {
    return maldoca::DocType::XLA;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".xls"))  {
    return maldoca::DocType::XLS;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".xlsb"))  {
    return maldoca::DocType::XLSB;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".xlsm"))  {
    return maldoca::DocType::XLSM;
  } else if (ExtensionEqualInCaseSensitive(file_path, ".xlsx"))  {
    return maldoca::DocType::XLSX;
  } else {
    return maldoca::DocType::UNKNOWN_TYPE;
  }
}

void AnalyzeOfficeDocument(base::File office_file,
                           const base::FilePath& file_path,
                           bool& contains_macro, bool& success,
                           std::string& error_code,
                           std::string& error_message) {
  // manually building processor_config
  maldoca::ProcessorConfig processor_config;

  third_party_maldoca::BuildProcessorConfig(&processor_config);

  maldoca::DocProcessor doc_processor(processor_config);

  maldoca::ProcessDocumentRequest process_doc_request;
  #if defined(OS_LINUX)
  const std::string file_name = file_path.BaseName().value().c_str();
  process_doc_request.set_file_name(file_name);
  #endif

  std::string office_file_content;
  base::ScopedFILE scoped_office_file(
      base::FileToFILE(std::move(office_file), "rb"));
  if (!base::ReadStreamToString(scoped_office_file.get(),
                                &office_file_content)) {
    contains_macro = false;
    success = false;
    error_code = "INTERNAL";
    error_message = "Could not convert file contents to string";
  }


  process_doc_request.set_doc_content(office_file_content);
  process_doc_request.set_doc_type(GetDocType(file_path));

  maldoca::ProcessDocumentResponse document_response;
  absl::Status analysis_status =
      doc_processor.ProcessDoc(&process_doc_request, &document_response);

  contains_macro = third_party_maldoca::HasMacro(&document_response);

  third_party_maldoca::ProcessStatusPayload(analysis_status, success,
                                            error_code, error_message);
}

bool HasMacro(const maldoca::ProcessDocumentResponse* document_response) {
  return document_response->parsed_document()
             .office()
             .parser_output()
             .script_features()
             .scripts_size() > 0;
}

void BuildProcessorConfig(maldoca::ProcessorConfig* processor_config) {
  processor_config->set_disable_file_type_check(true);

  std::string office_parser_handler_name = "office_parser";
  std::string office_extractor_handler_name = "office_extractor";

  maldoca::HandlerConfig office_parser_handler_config;
  office_parser_handler_config.set_doc_type(
      maldoca::HandlerConfig_DocType_OFFICE);

  maldoca::ParserConfig* parser_config =
      office_parser_handler_config.mutable_parser_config();

  parser_config->set_use_sandbox(false);
  parser_config->set_handler_type(
      maldoca::HandlerConfig_HandlerType_DEFAULT_OFFICE_PARSER);

  (*processor_config->mutable_handler_configs())[office_parser_handler_name] =
      office_parser_handler_config;
}

void ProcessStatusPayload(absl::Status status, bool& success,
                          std::string& error_code, std::string& error_message) {
  int grpc_error_code = static_cast<int>(status.code());
  if (grpc_error_code == 0) {
    success = true;
    error_code = "OK";
    error_message = std::string();
  } else {
    success = false;
    std::string maldoca_error_code_str;
    if (status.GetPayload(maldoca::kMaldocaStatusType)) {
      absl::Cord payload =
          status.GetPayload(maldoca::kMaldocaStatusType).value();
      absl::CopyCordToString(payload, &maldoca_error_code_str);
      error_code = maldoca_error_code_str;
    } else {
      error_code = StatusCodeToString(status.code());
    }
    error_message = std::string(status.ToString());
  }
}

}  // namespace third_party_maldoca
