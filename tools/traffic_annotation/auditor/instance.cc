// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/traffic_annotation/auditor/instance.h"

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "third_party/protobuf/src/google/protobuf/io/tokenizer.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_auditor.h"

namespace {

// This class receives parsing errors from google::protobuf::TextFormat::Parser
// which is used during protobuf deserialization.
class SimpleErrorCollector : public google::protobuf::io::ErrorCollector {
 public:
  SimpleErrorCollector(int proto_starting_line)
      : google::protobuf::io::ErrorCollector(),
        line_offset_(proto_starting_line) {}

  ~SimpleErrorCollector() override = default;

  void AddError(int line,
                google::protobuf::io::ColumnNumber column,
                const std::string& message) override {
    AddMessage(line, column, message);
  }

  void AddWarning(int line,
                  google::protobuf::io::ColumnNumber column,
                  const std::string& message) override {
    AddMessage(line, column, message);
  }

  std::string GetMessage() { return message_; }

 private:
  void AddMessage(int line,
                  google::protobuf::io::ColumnNumber column,
                  const std::string& message) {
    message_ += base::StringPrintf(
        "%sLine %i, column %i, %s", message_.length() ? " " : "",
        line_offset_ + line, static_cast<int>(column), message.c_str());
  }

  std::string message_;
  int line_offset_;
};

// This macro merges the content of one string field from two annotations.
// DST->FLD is the destination field, and SRD->FLD is the source field.
#define MERGE_STRING_FIELDS(SRC, DST, FLD)                           \
  if (!SRC.FLD().empty()) {                                          \
    if (!DST->FLD().empty()) {                                       \
      DST->set_##FLD(base::StringPrintf("%s\n%s", SRC.FLD().c_str(), \
                                        DST->FLD().c_str()));        \
    } else {                                                         \
      DST->set_##FLD(SRC.FLD());                                     \
    }                                                                \
  }

std::map<int, std::string> kSemanticsFields = {
    {traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
         kSenderFieldNumber,
     "semantics::sender"},
    {traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
         kDescriptionFieldNumber,
     "semantics::description"},
    {traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
         kTriggerFieldNumber,
     "semantics::trigger"},
    {traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
         kDataFieldNumber,
     "semantics::data"},
    {traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
         kDestinationFieldNumber,
     "semantics::destination"},
};

std::map<int, std::string> kPolicyFields = {
    {traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
         kCookiesAllowedFieldNumber,
     "policy::cookies_allowed"},
    {traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
         kCookiesStoreFieldNumber,
     "policy::cookies_store"},
    {traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
         kSettingFieldNumber,
     "policy::setting"},
    {traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
         kChromePolicyFieldNumber,
     "policy::chrome_policy"},
    {traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
         kPolicyExceptionJustificationFieldNumber,
     "policy::policy_exception_justification"},
};

std::vector<int> kChromePolicyFields = {
    traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
        kChromePolicyFieldNumber,
    traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
        kPolicyExceptionJustificationFieldNumber};

}  // namespace

AnnotationInstance::AnnotationInstance()
    : type(Type::ANNOTATION_COMPLETE),
      unique_id_hash_code(0),
      second_id_hash_code(0),
      archive_content_hash_code(0),
      is_loaded_from_archive(false),
      is_merged(false) {}

AnnotationInstance::AnnotationInstance(const AnnotationInstance& other)
    : proto(other.proto),
      type(other.type),
      second_id(other.second_id),
      unique_id_hash_code(other.unique_id_hash_code),
      second_id_hash_code(other.second_id_hash_code),
      archive_content_hash_code(other.archive_content_hash_code),
      is_loaded_from_archive(other.is_loaded_from_archive),
      is_merged(other.is_merged) {}

AuditorResult AnnotationInstance::Deserialize(
    const std::vector<std::string>& serialized_lines,
    int start_line,
    int end_line) {
  if (end_line - start_line < 6) {
    return AuditorResult(AuditorResult::Type::ERROR_FATAL,
                         "Not enough lines to deserialize annotation.");
  }

  // Extract header lines.
  const std::string& file_path = serialized_lines[start_line++];
  int line_number;
  base::StringToInt(serialized_lines[start_line++], &line_number);
  std::string function_type = serialized_lines[start_line++];
  const std::string& unique_id = serialized_lines[start_line++];
  second_id = serialized_lines[start_line++];

  // Decode function type.
  if (function_type == "Definition") {
    type = Type::ANNOTATION_COMPLETE;
  } else if (function_type == "Partial") {
    type = Type::ANNOTATION_PARTIAL;
  } else if (function_type == "Completing") {
    type = Type::ANNOTATION_COMPLETING;
  } else if (function_type == "BranchedCompleting") {
    type = Type::ANNOTATION_BRANCHED_COMPLETING;
  } else if (function_type == "Mutable") {
    return AuditorResult(AuditorResult::Type::ERROR_MUTABLE_TAG, "", file_path,
                         line_number);
  } else {
    return AuditorResult(AuditorResult::Type::ERROR_FATAL,
                         base::StringPrintf("Unexpected function type: %s",
                                            function_type.c_str()));
  }

  // Process test tags.
  unique_id_hash_code = TrafficAnnotationAuditor::ComputeHashValue(unique_id);
  if (unique_id_hash_code == TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code ||
      unique_id_hash_code ==
          PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS.unique_id_hash_code) {
    return AuditorResult(AuditorResult::Type::ERROR_TEST_ANNOTATION, "",
                         file_path, line_number);
  }

  // Process missing tag.
  if (unique_id_hash_code == MISSING_TRAFFIC_ANNOTATION.unique_id_hash_code)
    return AuditorResult(AuditorResult::Type::ERROR_MISSING_TAG_USED, "",
                         file_path, line_number);

  // Decode serialized proto.
  std::string annotation_text = "";
  while (start_line < end_line) {
    annotation_text += serialized_lines[start_line++] + "\n";
  }

  SimpleErrorCollector error_collector(line_number);
  google::protobuf::TextFormat::Parser parser;
  parser.RecordErrorsTo(&error_collector);
  if (!parser.ParseFromString(annotation_text,
                              (google::protobuf::Message*)&proto)) {
    return AuditorResult(AuditorResult::Type::ERROR_SYNTAX,
                         error_collector.GetMessage().c_str(), file_path,
                         line_number);
  }

  // Add other fields.
  traffic_annotation::NetworkTrafficAnnotation_TrafficSource* src =
      proto.mutable_source();
  src->set_file(file_path);
  src->set_line(line_number);
  proto.set_unique_id(unique_id);
  second_id_hash_code = TrafficAnnotationAuditor::ComputeHashValue(second_id);

  return AuditorResult(AuditorResult::Type::RESULT_OK);
}

// Returns the proto field numbers of TrafficSemantics.
void AnnotationInstance::GetSemanticsFieldNumbers(
    std::set<int>* field_numbers) const {
  field_numbers->clear();

  const traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics
      semantics = proto.semantics();

  if (!semantics.sender().empty())
    field_numbers->insert(semantics.kSenderFieldNumber);

  if (!semantics.description().empty())
    field_numbers->insert(semantics.kDescriptionFieldNumber);

  if (!semantics.trigger().empty())
    field_numbers->insert(semantics.kTriggerFieldNumber);

  if (!semantics.data().empty())
    field_numbers->insert(semantics.kDataFieldNumber);

  if (semantics.destination() !=
      traffic_annotation::
          NetworkTrafficAnnotation_TrafficSemantics_Destination_UNSPECIFIED) {
    field_numbers->insert(semantics.kDestinationFieldNumber);
  }
}

// Returns the proto field numbers of TrafficPolicy.
void AnnotationInstance::GetPolicyFieldNumbers(
    std::set<int>* field_numbers) const {
  field_numbers->clear();

  const traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy policy =
      proto.policy();

  // If cookies are not allowed, the negated value of the
  // kCookiesAllowedFieldNumber is returned. As field numbers are positive, this
  // will not collide with any other value.
  if (policy.cookies_allowed() ==
      traffic_annotation::
          NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_YES) {
    field_numbers->insert(policy.kCookiesAllowedFieldNumber);
  } else if (policy.cookies_allowed() ==
             traffic_annotation::
                 NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_NO) {
    field_numbers->insert(-policy.kCookiesAllowedFieldNumber);
  }

  if (!policy.cookies_store().empty())
    field_numbers->insert(policy.kCookiesStoreFieldNumber);

  if (!policy.setting().empty())
    field_numbers->insert(policy.kSettingFieldNumber);

  if (policy.chrome_policy_size())
    field_numbers->insert(policy.kChromePolicyFieldNumber);

  if (!policy.policy_exception_justification().empty())
    field_numbers->insert(policy.kPolicyExceptionJustificationFieldNumber);
}

// Checks if an annotation has all required fields.
AuditorResult AnnotationInstance::IsComplete() const {
  std::vector<std::string> unspecifieds;
  std::string extra_texts;

  std::set<int> fields;
  GetSemanticsFieldNumbers(&fields);
  for (const auto& item : kSemanticsFields) {
    if (!base::Contains(fields, item.first))
      unspecifieds.push_back(item.second);
  }

  GetPolicyFieldNumbers(&fields);
  for (const auto& item : kPolicyFields) {
    if (!base::Contains(fields, item.first)) {
      // If 'cookies_allowed = NO' is provided, ignore not having
      // 'cookies_allowed = YES'.
      if (item.first ==
              traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
                  kCookiesAllowedFieldNumber &&
          base::Contains(fields, -item.first))
        continue;

      // If |cookies_store| is not provided, ignore if 'cookies_allowed = NO' is
      // in the list.
      if (item.first ==
              traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
                  kCookiesStoreFieldNumber &&
          base::Contains(
              fields,
              -traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
                  kCookiesAllowedFieldNumber))
        continue;

      // If either of |chrome_policy| or |policy_exception_justification| are
      // avaliable, ignore not having the other one.
      if (base::Contains(kChromePolicyFields, item.first) &&
          (base::Contains(fields, kChromePolicyFields[0]) ||
           base::Contains(fields, kChromePolicyFields[1]))) {
        continue;
      }
      unspecifieds.push_back(item.second);
    }
  }

  if (!unspecifieds.size())
    return AuditorResult(AuditorResult::Type::RESULT_OK);

  std::string error_text;
  for (const std::string& item : unspecifieds)
    error_text += item + ", ";
  error_text = error_text.substr(0, error_text.length() - 2);
  return AuditorResult(AuditorResult::Type::ERROR_INCOMPLETE_ANNOTATION,
                       error_text, proto.source().file(),
                       proto.source().line());
}

// Checks if annotation fields are consistent.
AuditorResult AnnotationInstance::IsConsistent() const {
  const traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy policy =
      proto.policy();

  if (policy.cookies_allowed() ==
          traffic_annotation::
              NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_NO &&
      policy.cookies_store().size()) {
    return AuditorResult(
        AuditorResult::Type::ERROR_INCONSISTENT_ANNOTATION,
        "Cookies store is specified while cookies are not allowed.",
        proto.source().file(), proto.source().line());
  }

  if (policy.chrome_policy_size() &&
      policy.policy_exception_justification().size()) {
    return AuditorResult(
        AuditorResult::Type::ERROR_INCONSISTENT_ANNOTATION,
        "Both chrome policies and policy exception justification are present.",
        proto.source().file(), proto.source().line());
  }

  return AuditorResult(AuditorResult::Type::RESULT_OK);
}

bool AnnotationInstance::IsCompletableWith(
    const AnnotationInstance& other) const {
  if (type != AnnotationInstance::Type::ANNOTATION_PARTIAL || second_id.empty())
    return false;
  if (other.type == AnnotationInstance::Type::ANNOTATION_COMPLETING) {
    return second_id_hash_code == other.unique_id_hash_code;
  } else if (other.type ==
             AnnotationInstance::Type::ANNOTATION_BRANCHED_COMPLETING) {
    return second_id_hash_code == other.second_id_hash_code;
  } else {
    return false;
  }
}

AuditorResult AnnotationInstance::CreateCompleteAnnotation(
    AnnotationInstance& completing_annotation,
    AnnotationInstance* combination) const {
  DCHECK(IsCompletableWith(completing_annotation));

  // To keep the source information meta data, if completing annotation is of
  // type COMPLETING, keep |this| as the main and the other as completing.
  // But if compliting annotation is of type BRANCHED_COMPLETING, reverse
  // the order.
  const AnnotationInstance* other;
  if (completing_annotation.type ==
      AnnotationInstance::Type::ANNOTATION_COMPLETING) {
    *combination = *this;
    other = &completing_annotation;
  } else {
    *combination = completing_annotation;
    other = this;
  }

  combination->is_merged = true;
  combination->type = AnnotationInstance::Type::ANNOTATION_COMPLETE;
  combination->second_id.clear();
  combination->second_id_hash_code = 0;

  // Update comment.
  std::string new_comments = combination->proto.comments();
  if (!other->proto.comments().empty()) {
    if (!new_comments.empty())
      new_comments += "\n";
    new_comments += other->proto.comments();
  }
  if (!new_comments.empty())
    new_comments += "\n";
  new_comments += base::StringPrintf(
      "This annotation is a merge of the following two annotations:\n"
      "'%s' in '%s:%i' and '%s' in '%s:%i'.",
      proto.unique_id().c_str(), proto.source().file().c_str(),
      proto.source().line(), completing_annotation.proto.unique_id().c_str(),
      completing_annotation.proto.source().file().c_str(),
      completing_annotation.proto.source().line());
  combination->proto.set_comments(new_comments);

  // Copy TrafficSemantics.
  const traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics
      src_semantics = other->proto.semantics();
  traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics* dst_semantics =
      combination->proto.mutable_semantics();

  MERGE_STRING_FIELDS(src_semantics, dst_semantics, sender);
  MERGE_STRING_FIELDS(src_semantics, dst_semantics, description);
  MERGE_STRING_FIELDS(src_semantics, dst_semantics, trigger);
  MERGE_STRING_FIELDS(src_semantics, dst_semantics, data);
  MERGE_STRING_FIELDS(src_semantics, dst_semantics, destination_other);

  // If destination is not specified in dst_semantics, get it from
  // src_semantics. If both are specified and they differ, issue error.
  if (dst_semantics->destination() ==
      traffic_annotation::
          NetworkTrafficAnnotation_TrafficSemantics_Destination_UNSPECIFIED) {
    dst_semantics->set_destination(src_semantics.destination());
  } else if (
      src_semantics.destination() !=
          traffic_annotation::
              NetworkTrafficAnnotation_TrafficSemantics_Destination_UNSPECIFIED &&
      src_semantics.destination() != dst_semantics->destination()) {
    AuditorResult error(
        AuditorResult::Type::ERROR_MERGE_FAILED,
        "Annotations contain different semantics::destination values.");
    error.AddDetail(proto.unique_id());
    error.AddDetail(completing_annotation.proto.unique_id());
    return error;
  }

  // Copy TrafficPolicy.
  const traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy src_policy =
      other->proto.policy();
  traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy* dst_policy =
      combination->proto.mutable_policy();

  MERGE_STRING_FIELDS(src_policy, dst_policy, cookies_store);
  MERGE_STRING_FIELDS(src_policy, dst_policy, setting);

  // Set cookies_allowed to the superseding value of both.
  dst_policy->set_cookies_allowed(
      std::max(dst_policy->cookies_allowed(), src_policy.cookies_allowed()));
  DCHECK_GT(traffic_annotation::
                NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_YES,
            traffic_annotation::
                NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_NO);
  DCHECK_GT(
      traffic_annotation::
          NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_NO,
      traffic_annotation::
          NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_UNSPECIFIED);

  for (int i = 0; i < src_policy.chrome_policy_size(); i++)
    *dst_policy->add_chrome_policy() = src_policy.chrome_policy(i);

  if (!src_policy.policy_exception_justification().empty()) {
    if (!dst_policy->policy_exception_justification().empty()) {
      dst_policy->set_policy_exception_justification(
          dst_policy->policy_exception_justification() + "\n");
    }
    dst_policy->set_policy_exception_justification(
        dst_policy->policy_exception_justification() +
        src_policy.policy_exception_justification());
  }

  return AuditorResult::Type::RESULT_OK;
}

int AnnotationInstance::GetContentHashCode() const {
  if (is_loaded_from_archive)
    return archive_content_hash_code;

  traffic_annotation::NetworkTrafficAnnotation source_free_proto = proto;
  source_free_proto.clear_source();
  std::string content;
  google::protobuf::TextFormat::PrintToString(source_free_proto, &content);
  return TrafficAnnotationAuditor::ComputeHashValue(content);
}

// static
AnnotationInstance AnnotationInstance::LoadFromArchive(
    AnnotationInstance::Type type,
    const std::string& unique_id,
    int unique_id_hash_code,
    int second_id_hash_code,
    int content_hash_code,
    const std::set<int>& semantics_fields,
    const std::set<int>& policy_fields,
    const std::string& file_path) {
  AnnotationInstance annotation;

  annotation.is_loaded_from_archive = true;
  annotation.type = type;
  annotation.proto.set_unique_id(unique_id);
  annotation.proto.mutable_source()->set_file(file_path);
  annotation.unique_id_hash_code = unique_id_hash_code;

  if (annotation.NeedsTwoIDs()) {
    annotation.second_id_hash_code = second_id_hash_code;
    // As we don't have the actual second id, a generated value is written to
    // ensure that the field is not empty. Current set of auditor tests and
    // unittests just check if this field is not empty when a second id is
    // required. Tests that are based on matching the ids (like
    // partial/completing annotations) are based on the hash codes.
    annotation.second_id =
        base::StringPrintf("ARCHIVED_ID_%i", annotation.second_id_hash_code);
  }

  annotation.archive_content_hash_code = content_hash_code;

  // The values of the semantics and policy are set so that the tests would know
  // which fields were available before archive.
  if (base::Contains(
          semantics_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
              kSenderFieldNumber)) {
    annotation.proto.mutable_semantics()->set_sender("[Archived]");
  }

  if (base::Contains(
          semantics_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
              kDescriptionFieldNumber)) {
    annotation.proto.mutable_semantics()->set_description("[Archived]");
  }

  if (base::Contains(
          semantics_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
              kTriggerFieldNumber)) {
    annotation.proto.mutable_semantics()->set_trigger("[Archived]");
  }

  if (base::Contains(
          semantics_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
              kDataFieldNumber)) {
    annotation.proto.mutable_semantics()->set_data("[Archived]");
  }

  if (base::Contains(
          semantics_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficSemantics::
              kDestinationFieldNumber)) {
    annotation.proto.mutable_semantics()->set_destination(
        traffic_annotation::
            NetworkTrafficAnnotation_TrafficSemantics_Destination_WEBSITE);
  }

  if (base::Contains(
          policy_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
              kCookiesAllowedFieldNumber)) {
    annotation.proto.mutable_policy()->set_cookies_allowed(
        traffic_annotation::
            NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_YES);
  }

  if (base::Contains(
          policy_fields,
          -traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
              kCookiesAllowedFieldNumber)) {
    annotation.proto.mutable_policy()->set_cookies_allowed(
        traffic_annotation::
            NetworkTrafficAnnotation_TrafficPolicy_CookiesAllowed_NO);
  }

  if (base::Contains(
          policy_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
              kCookiesStoreFieldNumber)) {
    annotation.proto.mutable_policy()->set_cookies_store("[Archived]");
  }

  if (base::Contains(
          policy_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
              kSettingFieldNumber)) {
    annotation.proto.mutable_policy()->set_setting("[Archived]");
  }

  if (base::Contains(
          policy_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
              kChromePolicyFieldNumber)) {
    annotation.proto.mutable_policy()->add_chrome_policy();
  }

  if (base::Contains(
          policy_fields,
          traffic_annotation::NetworkTrafficAnnotation_TrafficPolicy::
              kPolicyExceptionJustificationFieldNumber)) {
    annotation.proto.mutable_policy()->set_policy_exception_justification(
        "[Archived]");
  }

  return annotation;
}

std::string AnnotationInstance::Serialize() const {
  std::string text;
  std::set<int> fields;

  text = base::StringPrintf(
      "{\tType: %i\n"
      "\tID: %s\n"
      "\tHashcode 1: %i\n"
      "\tHashcode 2: %i\n"
      "\tFrom Archive: %i\n"
      "\tSource File: %s\n",
      static_cast<int>(type), proto.unique_id().c_str(), unique_id_hash_code,
      second_id_hash_code, is_loaded_from_archive,
      proto.source().file().c_str());

  GetSemanticsFieldNumbers(&fields);
  text += "\tSemantics: ";
  for (int i : fields)
    text += base::StringPrintf("%s,", kSemanticsFields[i].c_str());

  GetPolicyFieldNumbers(&fields);
  text += "\n\tPolicies: ";
  for (int i : fields) {
    text += base::StringPrintf("%s%s,", i < 0 ? "-" : "",
                               kPolicyFields[abs(i)].c_str());
  }
  text += "\n}";

  return text;
}

std::ostream& operator<<(std::ostream& out,
                         const AnnotationInstance& instance) {
  return out << instance.Serialize();
}

CallInstance::CallInstance() : line_number(0), is_annotated(false) {}

CallInstance::CallInstance(const CallInstance& other)
    : file_path(other.file_path),
      line_number(other.line_number),
      function_name(other.function_name),
      is_annotated(other.is_annotated) {}

AuditorResult CallInstance::Deserialize(
    const std::vector<std::string>& serialized_lines,
    int start_line,
    int end_line) {
  if (end_line - start_line != 4) {
    return AuditorResult(AuditorResult::Type::ERROR_FATAL,
                         "Not enough lines to deserialize call.");
  }

  file_path = serialized_lines[start_line++];
  int line_number_int;
  base::StringToInt(serialized_lines[start_line++], &line_number_int);
  line_number = static_cast<uint32_t>(line_number_int);
  function_name = serialized_lines[start_line++];
  int is_annotated_int;
  base::StringToInt(serialized_lines[start_line++], &is_annotated_int);
  is_annotated = is_annotated_int != 0;
  return AuditorResult(AuditorResult::Type::RESULT_OK);
}

AssignmentInstance::AssignmentInstance() : line_number(0) {}

AssignmentInstance::AssignmentInstance(const AssignmentInstance& other)
    : file_path(other.file_path), line_number(other.line_number) {}

AuditorResult AssignmentInstance::Deserialize(
    const std::vector<std::string>& serialized_lines,
    int start_line,
    int end_line) {
  if (end_line - start_line != 2) {
    return AuditorResult(AuditorResult::Type::ERROR_FATAL,
                         "Not enough lines to deserialize assignment.");
  }
  file_path = serialized_lines[start_line++];
  int line_number_int;
  base::StringToInt(serialized_lines[start_line++], &line_number_int);
  line_number = static_cast<uint32_t>(line_number_int);
  return AuditorResult(AuditorResult::Type::RESULT_OK);
}
