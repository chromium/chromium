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

#include "maldoca/service/common/office_processing_component.h"

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "maldoca/base/digest.h"
#include "maldoca/base/enum_utils.h"
#include "maldoca/base/status.h"
#include "maldoca/base/status_macros.h"

#ifndef MALDOCA_CHROME
#include "maldoca/base/utf8/unicodetext.h"
#endif

#include "maldoca/ole/ole_to_proto.h"
#include "maldoca/ole/ooxml_to_proto.h"
#include "maldoca/ole/oss_utils.h"
#include "maldoca/ole/proto/office.pb.h"
#include "maldoca/service/proto/doc_type.pb.h"

#ifndef MALDOCA_CHROME
#include "maldoca/ole/proto/vba_extraction.pb.h"
#include "maldoca/ole/vba_extract.h"
#endif

#include "maldoca/service/common/utils.h"

#ifndef MALDOCA_CHROME
#include "maldoca/vba/antivirus/extract_stats_features.h"
#include "maldoca/vba/ast/ast2.pb.h"
#include "maldoca/vba/ast/ast2_cast.h"
#include "maldoca/vba/parser/line_parser.h"
#include "maldoca/vba/parser/merge_stmt_likes.h"
#include "maldoca/vba/parser/preprocessor.h"
#endif

namespace maldoca {
namespace {

bool IsOOXML(DocType doc_type) {
  return (doc_type == DocType::DOCX || doc_type == DocType::PPTX ||
          doc_type == DocType::XLSX);
}

// non-MALDOCA_CHROME-begin
#ifndef MALDOCA_CHROME
absl::optional<std::string> UnicodeToAscii(absl::string_view src,
                                           int* num_transformed_chars) {
  std::string dst;
  *num_transformed_chars = 0;

  utf8::UnicodeText unicode_text;
  unicode_text.PointToUTF8(src.data(), src.size());
  for (auto codepoint : unicode_text) {
    if (codepoint < 128) {
      // Char is ascii.
      dst.push_back(static_cast<char>(codepoint));
    } else {
      ++(*num_transformed_chars);
      absl::StrAppend(&dst, "x_", absl::Hex(static_cast<int>(codepoint)));
    }
  }

  if (*num_transformed_chars == 0) {
    return absl::nullopt;
  }
  return dst;
}
#endif
// non-MALDOCA_CHROME-end

// TODO(#103): Extract more features.
absl::Status ExtractOfficeFeatureFromVBA(
    const office::ScriptFeatures& script_features,
    OfficeDocumentFeatures* office_features) {
// non-MALDOCA_CHROME-begin
#ifndef MALDOCA_CHROME
  auto* vba_project_feature = office_features->mutable_vba_project_feature();
  for (const auto& script : script_features.scripts()) {
    const auto& chunks = script.vba_code().chunk();
    for (const auto& chunk : chunks) {
      // Step 1. Transform unicode characters. Our VBA parser does not handle
      // unicode now.
      int num_transformed_chars = 0;
      absl::optional<std::string> optional_transformed_code =
          UnicodeToAscii(chunk.code(), &num_transformed_chars);
      absl::string_view code;
      if (optional_transformed_code.has_value()) {
        code = optional_transformed_code.value();
      } else {
        code = chunk.code();
      }

      // Step 2. Parse VBA into line AST.
      std::vector<std::unique_ptr<VbStmtLike>> stmt_likes;
      std::vector<absl::Status> errors;
      PreprocessFile(code, [&](ExtendedLine extended_line) {
        auto stmt_likes_in_line = ParseExtendedLine(extended_line);
        if (!stmt_likes_in_line.ok()) {
          errors.push_back(stmt_likes_in_line.status());
          return;
        }
        for (auto& stmt_like : *stmt_likes_in_line) {
          stmt_likes.push_back(std::move(stmt_like));
        }
      });

      // Step 3. Extract features from line AST.
      auto* vba_file_feature = office_features->add_vba_file_features();
      for (const auto& stmt_like : stmt_likes) {
        VbaApiStats stats;
        AddVbaApiStats(*stmt_like, &stats);
        AddVbaApiStatsToVbaFeature(stats, vba_file_feature);
        AddVbaApiStatsToVbaFeature(stats, vba_project_feature);
      }

      // Step 4. Extract per-line features.
      VbFile file = MergeVbStmtLikes(std::move(stmt_likes), &errors);

      // TODO(#103): Extract more features.
    }
  }
#endif
  // non-MALDOCA_CHROME-end
  return absl::OkStatus();
}

// TODO(#103): Extract more features.
absl::Status ExtractOfficeFeature(const ParsedOfficeDocument& parsed_office_doc,
                                  OfficeDocumentFeatures* office_features) {
  office_features->set_doc_type(parsed_office_doc.doc_type());
  office_features->set_general_doc_type(parsed_office_doc.general_doc_type());

  return ExtractOfficeFeatureFromVBA(
      parsed_office_doc.parser_output().script_features(), office_features);
}

}  // namespace

void PrepParsedOfficeDocument(absl::string_view file_name,
                              absl::string_view doc, absl::string_view sha256,
                              DocType doc_type, ParsedDocument* pd) {
  pd->set_file_name(file_name.data(), file_name.size());
  pd->set_file_size(doc.size());
  if (sha256.empty()) {
    pd->set_sha256(Sha256HexString(doc));
  } else {
    pd->set_sha256(sha256.data(), sha256.size());
  }

  ParsedOfficeDocument* output = pd->mutable_office();
  // if output->doc_type is set and is not unknown, assume it's correct
  if (!output->has_doc_type() || output->doc_type() == DocType::UNKNOWN_TYPE) {
    output->set_doc_type(doc_type);
  }
  output->set_general_doc_type(utils::DocTypeToGeneralType(output->doc_type()));
}

OfficeParserHandler::OfficeParserHandler(absl::string_view name,
                                         const ParserConfig& config)
    : ParserHandler(name, config) {
  if (Config().use_sandbox()) {
#ifndef MALDOCA_CHROME
    parser_transaction_ = absl::make_unique<OfficeTransaction>(
        absl::make_unique<MaxRestrictiveOfficePolicy>(),
        Config().has_handler_config() ? &(Config().handler_config()) : nullptr);
#else
    // We'll just log error and continue w/o sandbox for now. TBD if we should
    // LOG(FATAL).
    LOG(ERROR) << "Sandbox not implemented for Chrome";
#endif
  }

  const auto& hconf = Config().handler_config();
  // If present, use OLE config from ParserConfig, otherwise use default OLE
  // proto settings.
  ole_to_proto_settings_ =
      (hconf.has_default_office_parser_config() &&
       hconf.default_office_parser_config().has_ole_to_proto_settings())
          ? &hconf.default_office_parser_config().ole_to_proto_settings()
          : &utils::GetDefaultOleToProtoSettings();

  // If present, use OOXML config from ParserConfig, otherwise use default OOXML
  // proto settings.
  ooxml_to_proto_settings_ =
      (hconf.has_default_office_parser_config() &&
       hconf.default_office_parser_config().has_ooxml_to_proto_settings())
          ? &hconf.default_office_parser_config().ooxml_to_proto_settings()
          : &utils::GetDefaultOoxmlToProtoSettings();
}

absl::Status OfficeParserHandler::Handle(const absl::string_view& input,
                                         ParsedDocument* output) {
  if (output->office().general_doc_type() != GENERAL_DOC_TYPE_DOC &&
      output->office().general_doc_type() != GENERAL_DOC_TYPE_XLS &&
      output->office().general_doc_type() != GENERAL_DOC_TYPE_PPT) {
    DLOG(INFO) << "Input type not supported for this pipeline, skipping.";
    return absl::OkStatus();
  }
  if (Config().use_sandbox()) {
#ifndef MALDOCA_CHROME
    DLOG(INFO) << "Using Sandbox to process Office doc";
    parser_transaction_->SetupData(input, output->mutable_office());
    auto status = parser_transaction_->Run();
    if (!status.ok()) {
      return status;
    }
#else
    return maldoca::UnimplementedError(
        "Sandbox not available for Chrome",
        MaldocaErrorCode::NOT_IMPLEMENTED_FOR_CHROME);
#endif
  } else {
    if (IsOOXML(output->office().doc_type())) {
      auto status_or =
          maldoca::GetOoxmlParserOutputProto(input, *ooxml_to_proto_settings_);
      if (!status_or.ok()) {
        return status_or.status();
      }

      *output->mutable_office()->mutable_parser_output() =
          std::move(status_or.value());
    } else {
      auto* pa = output->mutable_office()->mutable_parser_output();
      auto status_or =
          maldoca::GetParserOutputProto(input, *ole_to_proto_settings_);
      if (!status_or.ok()) {
        return status_or.status();
      }
      *pa = std::move(status_or.value());
    }
  }

  if (output->has_office()) {
    output->set_doc_type(output->office().doc_type());
    output->set_general_doc_type(output->office().general_doc_type());
  }
  return absl::OkStatus();
}

absl::Status OfficeFeatureExtractorHandler::Handle(const ParsedDocument& input,
                                                   DocumentFeatures* output) {
  // data passed from one handler to another
  output->set_sha256(input.sha256());
  output->set_doc_type(input.doc_type());
  output->set_general_doc_type(input.general_doc_type());
  output->set_file_name(input.file_name());
  output->set_file_size(input.file_size());
  if (!utils::ParsedDocumentHasVbaScript(input)) {
    DLOG(INFO) << "No VBA found in parsed document, skipping.";
    return absl::OkStatus();
  }
  return ExtractOfficeFeature(input.office(), output->mutable_office());
}

absl::Status OfficeFeatureExportHandler::Handle(const DocumentFeatures& input,
                                                ExportedFeatures* output) {
  // data passed from one handler to another
  output->set_sha256(input.sha256());
  output->set_doc_type(input.doc_type());
  output->set_general_doc_type(input.general_doc_type());
  output->set_file_name(input.file_name());
  output->set_file_size(input.file_size());

  return absl::UnimplementedError(
      "Feature exporting for office docs is not implemented");
}

// non-MALDOCA_CHROME-begin
#ifndef MALDOCA_CHROME
absl::Status OfficeTransaction::Main() {
  sapi::office::OfficeParams sapi_proto;
  auto input = sapi_proto.mutable_input();
  input->set_content({input_.data(), input_.size()});
  if (config_ != nullptr) {
    *input->mutable_config() = *config_;
  }

  sapi::v::Proto<sapi::office::OfficeParams> sandbox_wrapped_proto(sapi_proto);
  int status;
  if (IsOOXML(output_->doc_type())) {
    SAPI_ASSIGN_OR_RETURN(status,
                          api_.ooxml_parse(sandbox_wrapped_proto.PtrBoth()));
  } else {
    SAPI_ASSIGN_OR_RETURN(status,
                          api_.office_parse(sandbox_wrapped_proto.PtrBoth()));
  }

  if (!status) {
    LOG(ERROR) << "Sandboxing failed";
    // Get the real status
    SAPI_ASSIGN_OR_RETURN(auto pb_result, sandbox_wrapped_proto.GetMessage());
    if (pb_result.has_error_status()) {
      // should be the case
      return ::maldoca::utils::TranslateStatusFromProto(
          pb_result.error_status());
    }
    return absl::InternalError("Sandboxing failed");
  }

  SAPI_ASSIGN_OR_RETURN(auto pb_result, sandbox_wrapped_proto.GetMessage());

  *output_->mutable_parser_output() = std::move(pb_result.output());

  return absl::OkStatus();
}
#endif
// non-MALDOCA_CHROME-end
}  // namespace maldoca
