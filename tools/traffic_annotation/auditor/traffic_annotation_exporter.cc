// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/traffic_annotation/auditor/traffic_annotation_exporter.h"

#include <ctime>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/libxml/chromium/xml_reader.h"
#include "third_party/libxml/chromium/xml_writer.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"
#include "tools/traffic_annotation/auditor/traffic_annotation_auditor.h"

namespace {

const char* kXmlComment =
    "<!--\n"
    "Copyright 2017 The Chromium Authors. All rights reserved.\n"
    "Use of this source code is governed by a BSD-style license that can be\n"
    "found in the LICENSE file.\n"
    "\nRefer to README.md for content description and update process.\n"
    "-->\n\n";

const base::FilePath kAnnotationsXmlPath =
    base::FilePath(FILE_PATH_LITERAL("tools"))
        .Append(FILE_PATH_LITERAL("traffic_annotation"))
        .Append(FILE_PATH_LITERAL("summary"))
        .Append(FILE_PATH_LITERAL("annotations.xml"));

// Extracts annotation id from a line of XML. Expects to have the line in the
// following format: <... id="..." .../>
// TODO(rhalavati): Use real XML parsing.
std::string GetAnnotationID(const std::string& xml_line) {
  std::string::size_type start = xml_line.find("id=\"");
  if (start == std::string::npos)
    return "";

  start += 4;
  std::string::size_type end = xml_line.find("\"", start);
  if (end == std::string::npos)
    return "";

  return xml_line.substr(start, end - start);
}

// Extracts a map of XML items, keyed by their 'id' tags, from a serialized XML.
void ExtractXMLItems(const std::string& serialized_xml,
                     std::map<std::string, std::string>* items) {
  std::vector<std::string> lines = base::SplitString(
      serialized_xml, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const std::string& line : lines) {
    std::string id = GetAnnotationID(line);
    if (!id.empty())
      items->insert(std::make_pair(id, line));
  }
}

}  // namespace

TrafficAnnotationExporter::ArchivedAnnotation::ArchivedAnnotation()
    : type(AnnotationInstance::Type::ANNOTATION_COMPLETE),
      unique_id_hash_code(-1),
      second_id_hash_code(-1),
      content_hash_code(-1) {}

TrafficAnnotationExporter::ArchivedAnnotation::ArchivedAnnotation(
    const TrafficAnnotationExporter::ArchivedAnnotation& other) = default;

TrafficAnnotationExporter::ArchivedAnnotation::~ArchivedAnnotation() = default;

TrafficAnnotationExporter::TrafficAnnotationExporter(
    const base::FilePath& source_path)
    : source_path_(source_path), modified_(false) {
  all_supported_platforms_.push_back("linux");
  all_supported_platforms_.push_back("windows");
#if defined(OS_LINUX)
  current_platform_ = "linux";
#elif defined(OS_WIN)
  current_platform_ = "windows";
#else
  NOTREACHED() << "Other platforms are not supported yet.";
  current_platform_ = "undefined";
#endif
}

TrafficAnnotationExporter::~TrafficAnnotationExporter() = default;

bool TrafficAnnotationExporter::LoadAnnotationsXML() {
  archive_.clear();
  XmlReader reader;
  if (!reader.LoadFile(
          source_path_.Append(kAnnotationsXmlPath).MaybeAsASCII())) {
    LOG(ERROR) << "Could not load '"
               << source_path_.Append(kAnnotationsXmlPath).MaybeAsASCII()
               << "'.";
    return false;
  }

  bool all_ok = false;
  while (reader.Read()) {
    all_ok = true;
    if (reader.NodeName() != "item")
      continue;

    ArchivedAnnotation item;
    std::string temp_str;
    int temp_int = 0;
    std::string unique_id;

    all_ok &= reader.NodeAttribute("id", &unique_id);
    all_ok &= reader.NodeAttribute("hash_code", &temp_str) &&
              base::StringToInt(temp_str, &item.unique_id_hash_code);
    all_ok &= reader.NodeAttribute("type", &temp_str) &&
              base::StringToInt(temp_str, &temp_int);
    item.type = static_cast<AnnotationInstance::Type>(temp_int);

    if (reader.NodeAttribute("second_id", &temp_str))
      all_ok &= base::StringToInt(temp_str, &item.second_id_hash_code);

    if (all_ok && reader.NodeAttribute("content_hash_code", &temp_str))
      all_ok &= base::StringToInt(temp_str, &item.content_hash_code);
    else
      item.content_hash_code = -1;

    reader.NodeAttribute("deprecated", &item.deprecation_date);

    if (reader.NodeAttribute("os_list", &temp_str)) {
      item.os_list = base::SplitString(temp_str, ",", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_NONEMPTY);
    }

    if (reader.NodeAttribute("semantics_fields", &temp_str)) {
      std::vector<std::string> temp_list = base::SplitString(
          temp_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      for (std::string field : temp_list) {
        base::StringToInt(field, &temp_int);
        item.semantics_fields.insert(temp_int);
      }
    }

    if (reader.NodeAttribute("policy_fields", &temp_str)) {
      std::vector<std::string> temp_list = base::SplitString(
          temp_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      for (std::string field : temp_list) {
        base::StringToInt(field, &temp_int);
        item.policy_fields.insert(temp_int);
      }
    }

    all_ok &= reader.NodeAttribute("file_path", &item.file_path);

    if (!all_ok) {
      LOG(ERROR) << "Unexpected format in annotations.xml.";
      break;
    }

    archive_.insert(std::make_pair(unique_id, item));
  }

  modified_ = false;
  return all_ok;
}

void TrafficAnnotationExporter::UpdateAnnotations(
    const std::vector<AnnotationInstance>& annotations,
    const std::map<int, std::string>& reserved_ids,
    std::vector<AuditorResult>* errors) {
  CHECK(!archive_.empty());
  DCHECK(errors);

  std::set<int> current_platform_hashcodes;

  // Iterate annotations extracted from the code, and add/update them in the
  // reported list, if required.
  for (AnnotationInstance annotation : annotations) {
    // Annotations.XML only stores raw annotations.
    if (annotation.is_merged)
      continue;

    int content_hash_code = annotation.GetContentHashCode();
    // If annotation unique id is already in the imported annotations list,
    // check if other fields have changed.
    if (base::Contains(archive_, annotation.proto.unique_id())) {
      ArchivedAnnotation* current = &archive_[annotation.proto.unique_id()];

      // Check second id.
      if (current->second_id_hash_code !=
          archive_[annotation.proto.unique_id()].second_id_hash_code) {
        archive_[annotation.proto.unique_id()].second_id_hash_code =
            current->second_id_hash_code;
        modified_ = true;
      }

      // Check platform.
      if (!base::Contains(current->os_list, current_platform_)) {
        current->os_list.push_back(current_platform_);
        modified_ = true;
      }

      // Check content (including policy and semnantic fields).
      if (current->content_hash_code != content_hash_code) {
        current->content_hash_code = content_hash_code;
        modified_ = true;
      }

      // Check file path.
      if (current->file_path != annotation.proto.source().file()) {
        current->file_path = annotation.proto.source().file();
        modified_ = true;
      }
    } else {
      // If annotation is new, add it and assume it is on all platforms. Tests
      // running on other platforms will request updating this if required.
      ArchivedAnnotation new_item;
      new_item.type = annotation.type;
      new_item.unique_id_hash_code = annotation.unique_id_hash_code;
      if (annotation.NeedsTwoIDs())
        new_item.second_id_hash_code = annotation.second_id_hash_code;
      new_item.content_hash_code = content_hash_code;
      new_item.os_list = all_supported_platforms_;
      if (annotation.type != AnnotationInstance::Type::ANNOTATION_COMPLETE) {
        annotation.GetSemanticsFieldNumbers(&new_item.semantics_fields);
        annotation.GetPolicyFieldNumbers(&new_item.policy_fields);
      }
      new_item.file_path = annotation.proto.source().file();
      archive_[annotation.proto.unique_id()] = new_item;
      modified_ = true;
    }
    current_platform_hashcodes.insert(annotation.unique_id_hash_code);
  }

  // If a none-reserved annotation is removed from current platform, update it.
  for (auto& item : archive_) {
    if (base::Contains(item.second.os_list, current_platform_) &&
        item.second.content_hash_code != -1 &&
        !base::Contains(current_platform_hashcodes,
                        item.second.unique_id_hash_code)) {
      base::Erase(item.second.os_list, current_platform_);
      modified_ = true;
    }
  }

  // If there is a new reserved id, add it.
  for (const auto& item : reserved_ids) {
    if (!base::Contains(archive_, item.second)) {
      ArchivedAnnotation new_item;
      new_item.unique_id_hash_code = item.first;
      new_item.os_list = all_supported_platforms_;
      archive_[item.second] = new_item;
      modified_ = true;
    }
  }

  // If there are annotations that are not used in any OS, set the deprecation
  // flag.
  for (auto& item : archive_) {
    if (item.second.os_list.empty() && item.second.deprecation_date.empty()) {
      base::Time::Exploded now;
      base::Time::Now().UTCExplode(&now);
      item.second.deprecation_date = base::StringPrintf(
          "%i-%02i-%02i", now.year, now.month, now.day_of_month);
      item.second.file_path = "";
      item.second.semantics_fields.clear();
      item.second.policy_fields.clear();
      modified_ = true;
    }
  }

  CheckArchivedAnnotations(errors);
}

std::string TrafficAnnotationExporter::GenerateSerializedXML() const {
  XmlWriter writer;
  writer.StartWriting();
  writer.AppendElementContent(kXmlComment);
  writer.StartElement("annotations");

  for (const auto& item : archive_) {
    writer.StartElement("item");
    writer.AddAttribute("id", item.first);
    writer.AddAttribute(
        "hash_code", base::StringPrintf("%i", item.second.unique_id_hash_code));
    writer.AddAttribute("type", base::StringPrintf("%i", item.second.type));

    if (item.second.second_id_hash_code != -1)
      writer.AddAttribute(
          "second_id",
          base::StringPrintf("%i", item.second.second_id_hash_code));

    if (!item.second.deprecation_date.empty())
      writer.AddAttribute("deprecated", item.second.deprecation_date);

    if (item.second.content_hash_code == -1)
      writer.AddAttribute("reserved", "1");
    else
      writer.AddAttribute(
          "content_hash_code",
          base::StringPrintf("%i", item.second.content_hash_code));

    // Write OS list.
    if (!item.second.os_list.empty()) {
      std::string text;
      for (const std::string& platform : item.second.os_list)
        text += platform + ",";
      text.pop_back();
      writer.AddAttribute("os_list", text);
    }

    // Write semantics list (for incomplete annotations).
    if (!item.second.semantics_fields.empty()) {
      std::string text;
      for (int field : item.second.semantics_fields)
        text += base::StringPrintf("%i,", field);
      text.pop_back();
      writer.AddAttribute("semantics_fields", text);
    }

    // Write policy list (for incomplete annotations).
    if (!item.second.policy_fields.empty()) {
      std::string text;
      for (int field : item.second.policy_fields)
        text += base::StringPrintf("%i,", field);
      text.pop_back();
      writer.AddAttribute("policy_fields", text);
    }

    writer.AddAttribute("file_path", item.second.file_path);

    writer.EndElement();
  }
  writer.EndElement();

  writer.StopWriting();

  return writer.GetWrittenString();
}

bool TrafficAnnotationExporter::SaveAnnotationsXML() const {
  std::string xml_content = GenerateSerializedXML();

  return base::WriteFile(source_path_.Append(kAnnotationsXmlPath),
                         xml_content.c_str(), xml_content.length()) != -1;
}

void TrafficAnnotationExporter::GetDeprecatedHashCodes(
    std::set<int>* hash_codes) {
  CHECK(!archive_.empty());

  hash_codes->clear();
  for (const auto& item : archive_) {
    if (!item.second.deprecation_date.empty())
      hash_codes->insert(item.second.unique_id_hash_code);
  }
}

void TrafficAnnotationExporter::CheckArchivedAnnotations(
    std::vector<AuditorResult>* errors) {
  DCHECK(errors);
  // Check for annotation hash code duplications.
  std::map<int, std::string> used_codes;
  for (auto& item : archive_) {
    if (base::Contains(used_codes, item.second.unique_id_hash_code)) {
      AuditorResult error(AuditorResult::Type::ERROR_HASH_CODE_COLLISION);
      error.AddDetail(used_codes[item.second.unique_id_hash_code]);
      error.AddDetail(item.first);
      errors->push_back(std::move(error));
    } else {
      used_codes[item.second.unique_id_hash_code] = item.first;
    }
  }

  // Check for coexistence of OS(es) and deprecation date.
  for (auto& item : archive_) {
    if (!item.second.deprecation_date.empty() && !item.second.os_list.empty()) {
      errors->push_back(
          AuditorResult(AuditorResult::Type::ERROR_DEPRECATED_WITH_OS,
                        item.first, kAnnotationsXmlPath.MaybeAsASCII(),
                        AuditorResult::kNoCodeLineSpecified));
    }
  }

  // Check that listed OSes are valid.
  for (const auto& pair : archive_) {
    for (const auto& os : pair.second.os_list) {
      if (!base::Contains(all_supported_platforms_, os)) {
        AuditorResult error(AuditorResult::Type::ERROR_INVALID_OS,
                            std::string(), kAnnotationsXmlPath.MaybeAsASCII(),
                            AuditorResult::kNoCodeLineSpecified);
        error.AddDetail(os);
        error.AddDetail(pair.first);
        errors->push_back(std::move(error));
      }
    }
  }
}

unsigned TrafficAnnotationExporter::GetXMLItemsCountForTesting() {
  std::string xml_content;
  if (!base::ReadFileToString(
          base::MakeAbsoluteFilePath(source_path_.Append(kAnnotationsXmlPath)),
          &xml_content)) {
    LOG(ERROR) << "Could not read 'annotations.xml'.";
    return 0;
  }

  std::map<std::string, std::string> lines;
  ExtractXMLItems(xml_content, &lines);
  return lines.size();
}

std::string TrafficAnnotationExporter::GetRequiredUpdates() {
  std::string old_xml;
  if (!base::ReadFileToString(
          base::MakeAbsoluteFilePath(source_path_.Append(kAnnotationsXmlPath)),
          &old_xml)) {
    return "Could not generate required changes.";
  }

  return GetXMLDifferences(old_xml, GenerateSerializedXML());
}

std::string TrafficAnnotationExporter::GetXMLDifferences(
    const std::string& old_xml,
    const std::string& new_xml) {
  std::map<std::string, std::string> old_items;
  ExtractXMLItems(old_xml, &old_items);
  std::set<std::string> old_keys;
  std::transform(old_items.begin(), old_items.end(),
                 std::inserter(old_keys, old_keys.end()),
                 [](auto pair) { return pair.first; });

  std::map<std::string, std::string> new_items;
  ExtractXMLItems(new_xml, &new_items);
  std::set<std::string> new_keys;
  std::transform(new_items.begin(), new_items.end(),
                 std::inserter(new_keys, new_keys.end()),
                 [](auto pair) { return pair.first; });

  std::set<std::string> added_items;
  std::set<std::string> removed_items;

  std::set_difference(new_keys.begin(), new_keys.end(), old_keys.begin(),
                      old_keys.end(),
                      std::inserter(added_items, added_items.begin()));
  std::set_difference(old_keys.begin(), old_keys.end(), new_keys.begin(),
                      new_keys.end(),
                      std::inserter(removed_items, removed_items.begin()));

  std::string message;

  for (const std::string& id : added_items) {
    message += base::StringPrintf("\n\tAdd line: '%s'", new_items[id].c_str());
  }

  for (const std::string& id : removed_items) {
    message +=
        base::StringPrintf("\n\tRemove line: '%s'", old_items[id].c_str());
  }

  for (const std::string& id : old_keys) {
    if (base::Contains(new_items, id) && old_items[id] != new_items[id]) {
      message +=
          base::StringPrintf("\n\tUpdate line: '%s' --> '%s'",
                             old_items[id].c_str(), new_items[id].c_str());
    }
  }

  return message;
}

bool TrafficAnnotationExporter::GetOtherPlatformsAnnotationIDs(
    std::vector<std::string>* ids) const {
  if (archive_.empty())
    return false;

  ids->clear();
  for (const std::pair<std::string, ArchivedAnnotation>& item : archive_) {
    if (item.second.deprecation_date.empty() &&
        !MatchesCurrentPlatform(item.second))
      ids->push_back(item.first);
  }
  return true;
}
